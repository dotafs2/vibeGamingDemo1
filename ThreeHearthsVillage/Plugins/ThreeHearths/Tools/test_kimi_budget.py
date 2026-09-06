"""Offline accounting, HTTP-boundary, concurrency and process-crash acceptance.

All ledgers are temporary. All providers are fake or loopback-only. No real key.
"""
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
from dataclasses import replace
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json
import multiprocessing
from pathlib import Path
import sqlite3
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request

from kimi_budget import BudgetDenied,InvalidRequest,Ledger,LedgerCorrupt,NANO,NIGHT_POLICY,usage_cost
from kimi_gateway import BudgetServer,Gateway,UpstreamUnknown,handler_type

BODY={'model':'kimi-k2.6','messages':[{'role':'user','content':'选择一个可执行动作，以中文JSON回答。'}],
      'max_tokens':512,'thinking':{'type':'disabled'},'stream':False}
RECEIPT={'model':'kimi-k2.6','choices':[{'index':0,'finish_reason':'stop','message':{'role':'assistant','content':'{"action_id":0,"reason":"休息一下。"}'}}],
         'usage':{'prompt_tokens':1200,'completion_tokens':200,'total_tokens':1400,'cached_tokens':300}}
RESERVATION=262144*6500+512*27000

def reserve_worker(path,policy,barrier,queue,index):
    ledger=Ledger(path,policy,clock=lambda:1)
    barrier.wait(timeout=30)
    try:
        ledger.reserve('operation-'+str(index),'resident-'+str(index),BODY)
        queue.put('reserved')
    except BudgetDenied: queue.put('denied')
    except Exception as exc: queue.put(type(exc).__name__)

class FakeProvider:
    def __init__(self): self.calls=0
    def complete(self,body):
        self.calls+=1
        return json.loads(json.dumps(RECEIPT))

def crash_worker(path,url):
    class LocalProvider:
        def complete(self,body):
            request=urllib.request.Request(url,data=json.dumps(body).encode(),headers={'Content-Type':'application/json'})
            with urllib.request.urlopen(request,timeout=60) as result: return json.load(result)
    Gateway(Ledger(path,clock=lambda:1),LocalProvider()).complete('crash-operation','resident-crash',BODY)

class BudgetTests(unittest.TestCase):
    def setUp(self):
        self.folder=tempfile.TemporaryDirectory()
        self.path=Path(self.folder.name)/'budget.sqlite3'
        self.ledger=Ledger(self.path,clock=lambda:1)
        self.ledger.initialize()
    def tearDown(self): self.folder.cleanup()

    def test_initialization_is_explicit_and_cannot_reset(self):
        with self.assertRaises(BudgetDenied): self.ledger.initialize()
        missing=Ledger(Path(self.folder.name)/'another-world'/'budget.sqlite3')
        with self.assertRaises(LedgerCorrupt): missing.status()
        self.assertFalse(missing.path.exists())

    def test_integer_maximum_reservation_and_cached_settlement(self):
        self.assertEqual(RESERVATION,1_717_760_000)
        self.ledger.reserve('one','resident',BODY)
        self.assertEqual(self.ledger.status()['liability_cny'],1.72776)
        expected=900*6500+300*1100+200*27000
        self.assertEqual(self.ledger.settle('one',RECEIPT),expected)
        status=self.ledger.status()
        self.assertEqual(status['reserved_cny'],0)
        self.assertEqual(status['settled_cny'],expected/NANO)
        self.assertEqual(status['prior_unverified_cny'],0.01)

    def test_restart_preserves_paid_and_reserved_liability(self):
        self.ledger.reserve('settled','one',BODY)
        self.ledger.settle('settled',RECEIPT)
        self.ledger.reserve('pending','two',BODY)
        before=self.ledger.status()
        reopened=Ledger(self.path,clock=lambda:1)
        self.assertEqual(reopened.status(),before)
        with self.assertRaises(BudgetDenied): reopened.reserve('new','two',BODY)

    def test_duplicate_callback_and_operation_do_not_charge_or_send_twice(self):
        provider=FakeProvider(); gateway=Gateway(self.ledger,provider)
        first=gateway.complete('one','resident',BODY)
        liability=self.ledger.status()['liability_cny']
        self.ledger.settle('one',RECEIPT)
        replay=gateway.complete('one','resident',BODY)
        self.assertEqual(first,replay)
        self.assertEqual(provider.calls,1)
        self.assertEqual(self.ledger.status()['liability_cny'],liability)
        with self.assertRaises(BudgetDenied): gateway.complete('one','different-resident',BODY)
        with self.assertRaises(BudgetDenied): gateway.complete('one','resident',dict(BODY,max_tokens=256))
        changed=json.loads(json.dumps(RECEIPT)); changed['id']='different'
        with self.assertRaises(BudgetDenied): self.ledger.settle('one',changed)

    def test_timeout_keeps_money_and_blocks_resident_retry(self):
        class Timeout:
            def complete(self,body): raise TimeoutError()
        gateway=Gateway(self.ledger,Timeout())
        with self.assertRaises(UpstreamUnknown): gateway.complete('one','resident',BODY)
        status=self.ledger.status()
        self.assertEqual(status['counts'],{'uncertain':1})
        self.assertEqual(status['reserved_cny'],1.71776)
        with self.assertRaises(BudgetDenied): gateway.complete('two','resident',BODY)
        with self.assertRaises(BudgetDenied): gateway.complete('one','resident',BODY)

    def test_ten_independent_requests_run_concurrently_and_eleventh_is_denied(self):
        release=threading.Event(); arrived=threading.Barrier(11)
        class Waiting:
            def complete(self,body):
                arrived.wait(timeout=15)
                if not release.wait(timeout=15): raise TimeoutError()
                return RECEIPT
        gateway=Gateway(self.ledger,Waiting())
        with ThreadPoolExecutor(max_workers=10) as pool:
            futures=[pool.submit(gateway.complete,'op-'+str(i),'resident-'+str(i),BODY) for i in range(10)]
            try:
                arrived.wait(timeout=15)
                self.assertEqual(self.ledger.status()['counts'],{'reserved':10})
                with self.assertRaises(BudgetDenied): self.ledger.reserve('eleventh','resident-11',BODY)
                with self.assertRaises(BudgetDenied): self.ledger.reserve('duplicate-resident','resident-0',BODY)
            finally: release.set()
            self.assertTrue(all(f.result(timeout=15)['model']=='kimi-k2.6' for f in futures))
        self.assertEqual(self.ledger.status()['counts'],{'settled':10})

    def test_ten_processes_race_for_last_reservation(self):
        path=Path(self.folder.name)/'last-budget.sqlite3'
        policy=replace(NIGHT_POLICY,allocatable_nano=RESERVATION+NIGHT_POLICY.prior_unverified_nano)
        ledger=Ledger(path,policy,clock=lambda:1); ledger.initialize()
        ctx=multiprocessing.get_context('spawn')
        barrier=ctx.Barrier(10); queue=ctx.Queue()
        workers=[ctx.Process(target=reserve_worker,args=(str(path),policy,barrier,queue,i)) for i in range(10)]
        try:
            for worker in workers: worker.start()
            outcomes=[queue.get(timeout=40) for _ in workers]
            for worker in workers:
                worker.join(timeout=10)
                self.assertEqual(worker.exitcode,0)
            self.assertEqual(outcomes.count('reserved'),1,outcomes)
            self.assertEqual(outcomes.count('denied'),9,outcomes)
            self.assertEqual(ledger.status()['remaining_allocatable_cny'],0)
        finally:
            for worker in workers:
                if worker.is_alive(): worker.terminate(); worker.join()
            queue.close()

    def test_process_killed_after_upstream_received_keeps_durable_reservation(self):
        received=threading.Event(); release=threading.Event(); calls=[]
        class Upstream(BaseHTTPRequestHandler):
            def log_message(self,*args): pass
            def do_POST(self):
                self.rfile.read(int(self.headers['Content-Length']))
                calls.append(1); received.set(); release.wait(timeout=30)
                try:
                    self.send_response(200); self.end_headers(); self.wfile.write(json.dumps(RECEIPT).encode())
                except OSError: pass
        server=ThreadingHTTPServer(('127.0.0.1',0),Upstream)
        thread=threading.Thread(target=server.serve_forever,daemon=True); thread.start()
        child=multiprocessing.get_context('spawn').Process(target=crash_worker,args=(str(self.path),f'http://127.0.0.1:{server.server_port}/'))
        try:
            child.start()
            self.assertTrue(received.wait(timeout=20),'Fake upstream must receive before crash')
            child.terminate(); child.join(timeout=10)
            reopened=Ledger(self.path,clock=lambda:1)
            self.assertEqual(reopened.status()['reserved_cny'],1.71776)
            with self.assertRaises(BudgetDenied): reopened.reserve('crash-operation','resident-crash',BODY)
            with self.assertRaises(BudgetDenied): reopened.reserve('after-restart','resident-crash',BODY)
            self.assertEqual(len(calls),1)
        finally:
            release.set()
            if child.is_alive(): child.terminate(); child.join()
            server.shutdown(); server.server_close(); thread.join()

    def test_missing_or_corrupt_ledger_never_refills(self):
        self.ledger.reserve('one','resident',BODY)
        self.path.unlink()
        with self.assertRaises(LedgerCorrupt): self.ledger.status()
        self.assertFalse(self.path.exists())
        with self.assertRaises(BudgetDenied): self.ledger.initialize()
        self.path.write_bytes(b'not a database')
        with self.assertRaises(LedgerCorrupt): self.ledger.reserve('two','another',BODY)

    def test_deleted_records_and_changed_policy_fail_closed(self):
        self.ledger.reserve('one','resident',BODY)
        with closing(sqlite3.connect(self.path)) as db, db: db.execute('DELETE FROM requests')
        with self.assertRaises(LedgerCorrupt): self.ledger.status()
        wrong=Ledger(self.path,replace(NIGHT_POLICY,allocatable_nano=100*NANO))
        with self.assertRaises(LedgerCorrupt): wrong.status()

    def test_bad_usage_halts_future_paid_requests_without_refund(self):
        class Bad:
            def complete(self,body): return {'model':'kimi-k2.6','usage':{'prompt_tokens':5,'completion_tokens':513,'total_tokens':518}}
        with self.assertRaises(UpstreamUnknown): Gateway(self.ledger,Bad()).complete('one','resident',BODY)
        self.assertTrue(self.ledger.status()['halted'])
        self.assertEqual(self.ledger.status()['reserved_cny'],1.71776)
        with self.assertRaises(BudgetDenied): self.ledger.reserve('two','other',BODY)

    def test_usage_rejects_negative_boolean_inconsistent_and_model_changes(self):
        for field,value in [('prompt_tokens',-1),('completion_tokens',True),('total_tokens',1),('cached_tokens',1201)]:
            receipt=json.loads(json.dumps(RECEIPT)); receipt['usage'][field]=value
            with self.assertRaises(InvalidRequest): usage_cost(receipt,512)
        with self.assertRaises(InvalidRequest): usage_cost(dict(RECEIPT,model='kimi-k3'),512)
        receipt=json.loads(json.dumps(RECEIPT)); receipt['usage']['prompt_tokens_details']={'cached_tokens':301}
        with self.assertRaises(InvalidRequest): usage_cost(receipt,512)
        del receipt['usage']['cached_tokens']
        self.assertEqual(usage_cost(receipt,512)[3],301)
        del receipt['usage']['prompt_tokens_details']
        self.assertEqual(usage_cost(receipt,512)[0],1200*6500+200*27000)

    def test_expiry_and_disallowed_requests_never_send(self):
        expired_policy=replace(NIGHT_POLICY,deadline_utc=1)
        expired=Ledger(Path(self.folder.name)/'expired.sqlite3',policy=expired_policy,clock=lambda:1)
        expired.initialize()
        with self.assertRaises(BudgetDenied): expired.reserve('one','resident',BODY)
        invalid=[dict(BODY,model='kimi-k3'),dict(BODY,max_tokens=513),dict(BODY,tools=[]),
            dict(BODY,stream=True),dict(BODY,thinking={'type':'enabled'}),dict(BODY,max_completion_tokens=256),
            dict(BODY,messages=[{'role':'user','content':'字'*9000}]),
            dict(BODY,messages=[{'role':'user','content':[{'type':'image_url','image_url':'https://invalid'}]}])]
        for body in invalid:
            with self.assertRaises(InvalidRequest): self.ledger.reserve('one','resident',body)
        self.assertEqual(self.ledger.status()['counts'],{})

    def test_http_gateway_auth_and_idempotency(self):
        provider=FakeProvider(); gateway=Gateway(self.ledger,provider)
        server=BudgetServer(('127.0.0.1',0),handler_type(gateway,'fake-local-token'))
        thread=threading.Thread(target=server.serve_forever,daemon=True); thread.start()
        url=f'http://127.0.0.1:{server.server_port}/v1/chat/completions'
        def request(token='fake-local-token',body=BODY):
            return urllib.request.Request(url,data=json.dumps(body).encode(),headers={'Authorization':'Bearer '+token,
                'X-Hearth-Operation':'http-test','X-Hearth-Resident':'resident','Content-Type':'application/json'})
        try:
            with self.assertRaises(urllib.error.HTTPError) as caught: urllib.request.urlopen(request(token='wrong'))
            self.assertEqual(caught.exception.code,401)
            with self.assertRaises(urllib.error.HTTPError) as caught: urllib.request.urlopen(request(body=dict(BODY,tools=[])))
            self.assertEqual(caught.exception.code,400)
            for _ in range(2):
                with urllib.request.urlopen(request()) as response:
                    value=json.load(response)
                    self.assertIn('_hearth_budget',value)
            self.assertEqual(provider.calls,1)
        finally: server.shutdown(); server.server_close(); thread.join()

if __name__=='__main__': unittest.main(verbosity=2)
