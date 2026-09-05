"""The sole paid Kimi path for this project; loopback HTTP, durable CNY gate.

First use: python kimi_gateway.py init
After offline tests and explicit local enabled=true: python kimi_gateway.py serve
Inspect without contacting Kimi: python kimi_gateway.py status
"""
import argparse
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import secrets
import sys
import urllib.error
import urllib.request

from kimi_budget import BudgetDenied, BudgetError, InvalidRequest, Ledger, LedgerCorrupt, NIGHT_POLICY, encoded

ROOT=Path(__file__).resolve().parents[3]
BUDGET_DIR=ROOT/'Saved/ThreeHearths/Budget'
LEDGER_PATH=BUDGET_DIR/'kimi-overnight-2026-09-06.sqlite3'
CONFIG_PATH=ROOT/'Saved/ThreeHearths/api-config.json'
ENDPOINT_PATH=BUDGET_DIR/'gateway-endpoint.json'
PORT=18766
UPSTREAM='https://api.moonshot.cn/v1'

class UpstreamUnknown(BudgetError): pass

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs):
        raise UpstreamUnknown('Upstream redirect refused')

class KimiProvider:
    def __init__(self,config):
        if config.get('base_url','').rstrip('/')!=UPSTREAM or config.get('model')!=NIGHT_POLICY.model:
            raise InvalidRequest('Only the verified China K2.6 service is authorized')
        if config.get('thinking_mode')!='disabled': raise InvalidRequest('Thinking must be disabled in config')
        self.key=config.get('api_key','')
        if not isinstance(self.key,str) or not self.key or any(c.isspace() for c in self.key):
            raise InvalidRequest('A local service key is required')
        self.opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),NoRedirect())

    def request(self,path,body=None):
        payload=None if body is None else encoded(body).encode('utf-8')
        request=urllib.request.Request(UPSTREAM+path,data=payload,
            headers={'Authorization':'Bearer '+self.key,'Content-Type':'application/json'},
            method='GET' if body is None else 'POST')
        try:
            with self.opener.open(request,timeout=35) as response:
                if response.status!=200: raise UpstreamUnknown('Unexpected upstream HTTP status')
                raw=response.read(131073)
                if len(raw)>131072: raise UpstreamUnknown('Oversized upstream response')
                value=json.loads(raw)
                if not isinstance(value,dict): raise UpstreamUnknown('Invalid upstream JSON object')
                return value
        except (OSError,ValueError,urllib.error.URLError) as exc:
            # Never echo upstream error bodies, URLs, or credentials.
            raise UpstreamUnknown('Upstream result uncertain; reservation retained') from None

    def complete(self,body): return self.request('/chat/completions',body)
    def balance(self):
        result=self.request('/users/me/balance')
        if result.get('status') is not True or result.get('code')!=0 or not isinstance(result.get('data'),dict):
            raise UpstreamUnknown('Could not verify account balance')
        return result['data']

class Gateway:
    def __init__(self,ledger,provider):
        self.ledger=ledger
        self.provider=provider

    def complete(self,request_id,resident,body):
        reservation=self.ledger.reserve(request_id,resident,body)
        if not reservation['send']:
            response=reservation['response']
        else:
            try:
                response=self.provider.complete(reservation['body'])
                self.ledger.settle(request_id,response)
            except InvalidRequest as exc:
                self.ledger.uncertain(request_id,str(exc),halt=True)
                raise UpstreamUnknown('Billing receipt invalid; all further paid calls halted') from None
            except Exception:
                # If this write also fails, the original durable reserved row remains.
                self.ledger.uncertain(request_id,'Upstream or settlement did not complete')
                raise UpstreamUnknown('Result uncertain; no refund or automatic retry') from None
        return dict(response,_hearth_budget=self.ledger.status())

class BudgetServer(ThreadingHTTPServer):
    daemon_threads=True
    allow_reuse_address=False

def handler_type(gateway,token):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass

        def reply(self,status,value):
            data=encoded(value).encode('utf-8')
            try:
                self.send_response(status)
                self.send_header('Content-Type','application/json; charset=utf-8')
                self.send_header('Content-Length',str(len(data)))
                self.send_header('Cache-Control','no-store')
                self.end_headers()
                self.wfile.write(data)
            except (BrokenPipeError,ConnectionResetError): pass

        def authenticated(self):
            if not hmac.compare_digest(self.headers.get('Authorization',''),'Bearer '+token):
                self.reply(401,{'error':'Local budget gateway authentication required'})
                return False
            return True

        def do_GET(self):
            if not self.authenticated(): return
            if self.path!='/budget': return self.reply(404,{'error':'Unknown route'})
            try: self.reply(200,gateway.ledger.status())
            except BudgetError: self.reply(503,{'error':'Budget ledger unavailable; paid calls stopped'})

        def do_POST(self):
            if not self.authenticated(): return
            if self.path!='/v1/chat/completions': return self.reply(404,{'error':'Unknown route'})
            try:
                length=int(self.headers.get('Content-Length','0'))
                if not 1<=length<=gateway.ledger.policy.max_request_bytes: raise InvalidRequest('Request size rejected')
                if self.headers.get('Transfer-Encoding'): raise InvalidRequest('Chunked requests are not supported')
                self.connection.settimeout(5)
                raw=self.rfile.read(length)
                if len(raw)!=length: raise InvalidRequest('Incomplete request')
                body=json.loads(raw)
                response=gateway.complete(self.headers.get('X-Hearth-Operation'),self.headers.get('X-Hearth-Resident'),body)
                self.reply(200,response)
            except (ValueError,InvalidRequest): self.reply(400,{'error':'Invalid or unauthorized request options'})
            except BudgetDenied as exc: self.reply(402,{'error':str(exc)})
            except UpstreamUnknown as exc: self.reply(502,{'error':str(exc)})
            except LedgerCorrupt: self.reply(503,{'error':'Budget ledger unavailable; paid calls stopped'})
            except Exception: self.reply(503,{'error':'Budget gateway failed closed'})
    return Handler

def write_private_json(path,value):
    temporary=path.with_suffix('.tmp')
    with temporary.open('w',encoding='utf-8') as file:
        file.write(encoded(value))
        file.flush()
        os.fsync(file.fileno())
    os.replace(temporary,path)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=('init','serve','status'))
    args=parser.parse_args()
    ledger=Ledger(LEDGER_PATH)
    if args.command=='init':
        print(encoded(ledger.initialize()))
        return
    if args.command=='status':
        print(encoded(ledger.status()))
        return
    # Check existing budget first: server startup cannot initialize or reset it.
    status=ledger.status()
    if status['halted']: raise BudgetDenied('Budget is halted')
    config=json.loads(CONFIG_PATH.read_text(encoding='utf-8-sig'))
    if config.get('enabled') is not True: raise BudgetDenied('Paid provider remains disabled in local config')
    provider=KimiProvider(config)
    token=secrets.token_hex(32)
    gateway=Gateway(ledger,provider)
    server=BudgetServer(('127.0.0.1',PORT),handler_type(gateway,token))
    # Read-only official balance query, through the same controlled provider.
    ledger.record_balance(provider.balance())
    if ledger.status()['halted']: raise BudgetDenied('Provider balance does not permit requests')
    write_private_json(ENDPOINT_PATH,{'schema_version':1,'base_url':f'http://127.0.0.1:{PORT}/v1',
        'api_key':token,'model':NIGHT_POLICY.model,'ledger_id':status['ledger_id'],
        'policy_sha256':ledger.policy_hash,'allocation_cap_cny':95,'pid':os.getpid()})
    print('Kimi budget gateway ready on loopback; persistent allocation cap CNY 95',flush=True)
    try: server.serve_forever(poll_interval=0.5)
    finally:
        server.server_close()
        # Do not remove or refund reservations on shutdown.
        if ENDPOINT_PATH.exists():
            current=json.loads(ENDPOINT_PATH.read_text(encoding='utf-8'))
            if current.get('pid')==os.getpid(): ENDPOINT_PATH.unlink()

if __name__=='__main__':
    try: main()
    except BudgetError as exc:
        print('Budget gateway stopped: '+str(exc),file=sys.stderr)
        sys.exit(1)
