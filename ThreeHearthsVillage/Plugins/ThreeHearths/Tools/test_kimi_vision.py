"""Offline visual billing transport regressions; no real API or persistent budget."""
import base64
import copy
from dataclasses import asdict
import json
from pathlib import Path
import struct
import tempfile
import unittest
import urllib.request
import threading
import zlib

from kimi_vision import validate_png_url, MAX_REQUEST_BYTES
from kimi_budget import Ledger, NIGHT_POLICY, InvalidRequest, BudgetDenied, normalize_request, fingerprint
from kimi_gateway import BudgetServer, Gateway, handler_type
from test_kimi_budget import BODY, RECEIPT, RESERVATION


def png(width=128, height=128, noisy=False):
    def chunk(kind, value):
        return struct.pack('>I', len(value))+kind+value+struct.pack('>I', zlib.crc32(kind+value)&0xffffffff)
    if noisy:
        import random
        rng=random.Random(583)
        rows=b''.join(b'\0'+rng.randbytes(width*4) for _ in range(height))
    else:
        rows=(b'\0'+b'\x80\x90\x70\xff'*width)*height
    data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')
    return 'data:image/png;base64,'+base64.b64encode(data).decode()


def body(url=None):
    value=copy.deepcopy(BODY)
    value['messages'][0]['content']=[{'type':'text','text':'根据自家实景评估院落。'},
        {'type':'image_url','image_url':{'url':url or png()}}]
    return value


class VisionTests(unittest.TestCase):
    def test_pixels_and_bounds(self):
        self.assertEqual(validate_png_url(png()),(128,128))
        for bad in ('https://example.org/house.png','data:image/png;base64,AAAA',png(1025,1),png()[:-7]):
            with self.assertRaises(ValueError): validate_png_url(bad)

    def test_reject_malformed_and_unbounded_messages(self):
        for patch in ('remote','two','system','video','excess_text'):
            request=body()
            if patch=='remote': request['messages'][0]['content'][1]['image_url']['url']='https://example.org/a.png'
            if patch=='two': request['messages'][0]['content'].append(copy.deepcopy(request['messages'][0]['content'][1]))
            if patch=='system': request['messages'][0]['role']='system'
            if patch=='video': request['messages'][0]['content'][1]['type']='video_url'
            if patch=='excess_text': request['messages'][0]['content'][0]['text']='x'*25000
            with self.assertRaises(InvalidRequest): normalize_request(request)

    def test_image_replay_and_financial_policy_unchanged(self):
        policy_hash=fingerprint(asdict(NIGHT_POLICY))
        with tempfile.TemporaryDirectory() as folder:
            ledger=Ledger(Path(folder)/'test.sqlite3'); ledger.initialize()
            request=body(png(128,128,True))
            self.assertGreater(len(json.dumps(request)),32768)
            self.assertLess(len(json.dumps(request)),MAX_REQUEST_BYTES)
            ledger.reserve('visual-one','resident-one',request)
            self.assertEqual(ledger.status()['reserved_cny'],RESERVATION/1e9)
            ledger.settle('visual-one',RECEIPT)
            self.assertFalse(ledger.reserve('visual-one','resident-one',request)['send'])
            with self.assertRaises(BudgetDenied): ledger.reserve('visual-one','resident-one',body(png(64,64)))
            self.assertEqual(ledger.policy_hash,policy_hash)

    def test_real_http_multimodal_envelope_and_usage_receipt(self):
        class Provider:
            calls=0
            def complete(self, request):
                self.calls+=1
                assert validate_png_url(request['messages'][0]['content'][1]['image_url']['url'])==(128,128)
                return copy.deepcopy(RECEIPT)
        with tempfile.TemporaryDirectory() as folder:
            ledger=Ledger(Path(folder)/'test.sqlite3'); ledger.initialize(); provider=Provider()
            server=BudgetServer(('127.0.0.1',0),handler_type(Gateway(ledger,provider),'local-test'))
            thread=threading.Thread(target=server.serve_forever); thread.start()
            try:
                request=urllib.request.Request(f'http://127.0.0.1:{server.server_port}/v1/chat/completions',data=json.dumps(body(png(128,128,True))).encode(),
                    headers={'Authorization':'Bearer local-test','X-Hearth-Operation':'image-operation','X-Hearth-Resident':'resident-a','Content-Type':'application/json'})
                opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
                for _ in range(2):
                    with opener.open(request,timeout=5) as response:
                        value=json.load(response)
                        self.assertEqual(value['_hearth_budget']['counts']['settled'],1)
                self.assertEqual(provider.calls,1)
            finally:
                server.shutdown(); server.server_close(); thread.join(timeout=3)


if __name__=='__main__': unittest.main(verbosity=2)
