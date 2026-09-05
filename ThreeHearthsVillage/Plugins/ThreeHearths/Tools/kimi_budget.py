"""Persistent, fail-closed CNY reservations for this project's authorized night.

Amounts use integer nanoyuan (1 CNY = 1,000,000,000 units). The production
gateway always uses NIGHT_POLICY and a fixed project path, never world state.
"""
from contextlib import closing, contextmanager
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import time
import uuid

NANO=1_000_000_000
MODEL='kimi-k2.6'
DEADLINE=int(datetime(2026,9,6,1,30,tzinfo=timezone.utc).timestamp())

@dataclass(frozen=True)
class Policy:
    model: str=MODEL
    authorized_nano: int=100*NANO
    allocatable_nano: int=95*NANO
    # 119 tokens from the earlier configuration probe, rounded conservatively.
    prior_unverified_nano: int=10_000_000
    input_nano_per_token: int=6500
    cached_nano_per_token: int=1100
    output_nano_per_token: int=27000
    input_ceiling: int=262144
    max_output: int=512
    max_request_bytes: int=32768
    max_context_utf8_bytes: int=24576
    concurrency: int=10
    deadline_utc: int=DEADLINE
    price_verified: str='2026-09-06 https://platform.kimi.com/ K2.6 China'

NIGHT_POLICY=Policy()

class BudgetError(RuntimeError): pass
class BudgetDenied(BudgetError): pass
class LedgerCorrupt(BudgetError): pass
class InvalidRequest(BudgetError): pass

def encoded(value):
    return json.dumps(value,ensure_ascii=False,sort_keys=True,separators=(',',':'),allow_nan=False)

def fingerprint(value): return hashlib.sha256(encoded(value).encode('utf-8')).hexdigest()

def integer(value, label, minimum=0):
    if type(value) is not int or value<minimum: raise InvalidRequest('Invalid '+label)
    return value

def normalize_request(body, policy=NIGHT_POLICY):
    if not isinstance(body,dict): raise InvalidRequest('JSON object required')
    allowed={'model','messages','stream','max_tokens','max_completion_tokens','thinking','response_format'}
    if set(body)-allowed: raise InvalidRequest('Unsupported generation option')
    if body.get('model')!=policy.model or body.get('stream',False) is not False:
        raise InvalidRequest('Only non-streaming authorized model is allowed')
    if 'max_tokens' in body and 'max_completion_tokens' in body: raise InvalidRequest('Duplicate output limits')
    maximum=integer(body.get('max_tokens',body.get('max_completion_tokens')),'output limit',1)
    if maximum>policy.max_output: raise InvalidRequest('Output limit exceeds policy')
    if body.get('thinking',{'type':'disabled'})!={'type':'disabled'}: raise InvalidRequest('Thinking must be disabled')
    if body.get('response_format',{'type':'json_object'})!={'type':'json_object'}:
        raise InvalidRequest('Only JSON object output is supported')
    messages=body.get('messages')
    if not isinstance(messages,list) or not 1<=len(messages)<=8: raise InvalidRequest('Bounded messages required')
    length=0
    for message in messages:
        if not isinstance(message,dict) or set(message)!={'role','content'}: raise InvalidRequest('Text messages only')
        if message['role'] not in ('system','user','assistant') or not isinstance(message['content'],str) or not message['content']:
            raise InvalidRequest('Invalid text message')
        length+=len(message['content'].encode('utf-8'))
    if length>policy.max_context_utf8_bytes: raise InvalidRequest('Context byte limit exceeded')
    clean={'model':policy.model,'messages':messages,'stream':False,'max_tokens':maximum,
           'thinking':{'type':'disabled'},'response_format':{'type':'json_object'}}
    if len(encoded(clean).encode('utf-8'))>policy.max_request_bytes: raise InvalidRequest('Request byte limit exceeded')
    return clean

def usage_cost(response, maximum, policy=NIGHT_POLICY):
    if response.get('model')!=policy.model: raise InvalidRequest('Unverified response model')
    usage=response.get('usage')
    if not isinstance(usage,dict): raise InvalidRequest('Missing billing usage')
    prompt=integer(usage.get('prompt_tokens'),'prompt tokens')
    output=integer(usage.get('completion_tokens'),'completion tokens')
    total=integer(usage.get('total_tokens'),'total tokens')
    if prompt>policy.input_ceiling or output>maximum or total!=prompt+output:
        raise InvalidRequest('Usage exceeds policy or is inconsistent')
    values=[]
    if 'cached_tokens' in usage: values.append(integer(usage['cached_tokens'],'cached tokens'))
    details=usage.get('prompt_tokens_details',{})
    if not isinstance(details,dict): raise InvalidRequest('Invalid prompt usage details')
    if 'cached_tokens' in details: values.append(integer(details['cached_tokens'],'detailed cached tokens'))
    if len(set(values))>1: raise InvalidRequest('Conflicting cache usage')
    cached=values[0] if values else 0  # Missing cache counts are charged at the higher input price.
    if cached>prompt: raise InvalidRequest('Cache exceeds input')
    charge=(prompt-cached)*policy.input_nano_per_token+cached*policy.cached_nano_per_token+output*policy.output_nano_per_token
    return charge,prompt,output,cached

class Ledger:
    def __init__(self, path, policy=NIGHT_POLICY, clock=time.time):
        self.path=Path(path)
        self.guard=self.path.with_suffix('.guard.json')
        self.policy=policy
        self.clock=clock
        self.policy_hash=fingerprint(asdict(policy))

    def initialize(self):
        """Explicit first-use only; neither server startup nor reset calls this."""
        self.path.parent.mkdir(parents=True,exist_ok=True)
        if self.path.exists() or self.guard.exists(): raise BudgetDenied('Budget already initialized; no reset supported')
        identity=str(uuid.uuid4())
        with self.guard.open('x',encoding='utf-8') as file:
            file.write(encoded({'ledger_id':identity,'policy':asdict(self.policy),'policy_sha256':self.policy_hash}))
            file.flush()
            import os
            os.fsync(file.fileno())
        # The guard deliberately survives an interrupted initialization.
        with closing(sqlite3.connect(self.path)) as db, db:
            db.execute('PRAGMA journal_mode=WAL')
            db.execute('PRAGMA synchronous=FULL')
            db.executescript('''
              CREATE TABLE meta (id INTEGER PRIMARY KEY CHECK(id=1), ledger_id TEXT NOT NULL,
                policy_sha256 TEXT NOT NULL, liability INTEGER NOT NULL CHECK(liability>=0),
                request_count INTEGER NOT NULL, halted TEXT NOT NULL DEFAULT '');
              CREATE TABLE requests (id TEXT PRIMARY KEY, resident TEXT NOT NULL, payload_sha TEXT NOT NULL,
                maximum INTEGER NOT NULL, reserve INTEGER NOT NULL CHECK(reserve>0),
                state TEXT NOT NULL CHECK(state IN ('reserved','uncertain','settled')),
                charge INTEGER CHECK(charge>=0), prompt_tokens INTEGER, output_tokens INTEGER,
                cached_tokens INTEGER, response TEXT, response_sha TEXT, created REAL NOT NULL,
                finished REAL, note TEXT NOT NULL DEFAULT '');
              CREATE UNIQUE INDEX resident_inflight ON requests(resident) WHERE state IN ('reserved','uncertain');
              CREATE TABLE balances (at REAL NOT NULL, available TEXT NOT NULL, cash TEXT NOT NULL, voucher TEXT NOT NULL);
            ''')
            db.execute('INSERT INTO meta(id,ledger_id,policy_sha256,liability,request_count) VALUES(1,?,?,?,0)',
                (identity,self.policy_hash,self.policy.prior_unverified_nano))
        return self.status()

    @contextmanager
    def transaction(self):
        db=None
        try:
            guard=json.loads(self.guard.read_text(encoding='utf-8'))
            if guard.get('policy')!=asdict(self.policy) or guard.get('policy_sha256')!=self.policy_hash:
                raise LedgerCorrupt('Budget policy/guard mismatch')
            # mode=rw is essential: missing database must never create a fresh budget.
            db=sqlite3.connect(self.path.resolve().as_uri()+'?mode=rw',uri=True,timeout=10)
            db.row_factory=sqlite3.Row
            db.execute('PRAGMA synchronous=FULL')
            db.execute('BEGIN IMMEDIATE')
            if db.execute('PRAGMA quick_check').fetchone()[0]!='ok': raise LedgerCorrupt('Database integrity check failed')
            meta=db.execute('SELECT * FROM meta WHERE id=1').fetchone()
            if not meta or meta['ledger_id']!=guard['ledger_id'] or meta['policy_sha256']!=self.policy_hash:
                raise LedgerCorrupt('Budget identity mismatch')
            liability,count=db.execute("SELECT COALESCE(SUM(CASE WHEN state='settled' THEN charge ELSE reserve END),0),COUNT(*) FROM requests").fetchone()
            if meta['liability']!=liability+self.policy.prior_unverified_nano or meta['request_count']!=count:
                raise LedgerCorrupt('Budget accounting mismatch')
            if not 0<=meta['liability']<=self.policy.allocatable_nano: raise LedgerCorrupt('Budget liability outside cap')
            yield db,meta
            db.commit()
        except (sqlite3.Error,OSError,ValueError,KeyError,TypeError) as exc:
            raise LedgerCorrupt('Budget ledger unavailable or corrupt; paid calls stopped') from exc
        finally:
            if db is not None: db.close()

    def reserve(self, request_id, resident, body):
        for value in (request_id,resident):
            if not isinstance(value,str) or not re.fullmatch(r'[A-Za-z0-9_.:-]{1,128}',value):
                raise InvalidRequest('Bounded operation and resident identifiers required')
        clean=normalize_request(body,self.policy)
        payload=fingerprint(clean)
        maximum=clean['max_tokens']
        reserve=self.policy.input_ceiling*self.policy.input_nano_per_token+maximum*self.policy.output_nano_per_token
        with self.transaction() as (db,meta):
            old=db.execute('SELECT * FROM requests WHERE id=?',(request_id,)).fetchone()
            if old:
                if old['payload_sha']!=payload or old['resident']!=resident: raise BudgetDenied('Operation ID reused with different request')
                if old['state']!='settled': raise BudgetDenied('Operation remains reserved or uncertain; no retry')
                response=json.loads(old['response'])
                if fingerprint(response)!=old['response_sha']: raise LedgerCorrupt('Cached receipt corrupted')
                return {'send':False,'response':response}
            if meta['halted']: raise BudgetDenied('Paid requests halted: '+meta['halted'])
            if self.clock()>=self.policy.deadline_utc: raise BudgetDenied('Authorized night has ended')
            active=db.execute("SELECT COUNT(*) FROM requests WHERE state IN ('reserved','uncertain')").fetchone()[0]
            if active>=self.policy.concurrency: raise BudgetDenied('Ten in-flight or unresolved requests already exist')
            if db.execute("SELECT 1 FROM requests WHERE resident=? AND state IN ('reserved','uncertain')",(resident,)).fetchone():
                raise BudgetDenied('Resident already has an in-flight or unresolved request')
            if meta['liability']+reserve>self.policy.allocatable_nano: raise BudgetDenied('CNY budget cannot cover worst-case reservation')
            db.execute("INSERT INTO requests(id,resident,payload_sha,maximum,reserve,state,created) VALUES(?,?,?,?,?,'reserved',?)",
                (request_id,resident,payload,maximum,reserve,self.clock()))
            db.execute('UPDATE meta SET liability=liability+?,request_count=request_count+1 WHERE id=1',(reserve,))
        return {'send':True,'body':clean,'reserved_nano':reserve}

    def settle(self, request_id, response):
        with self.transaction() as (db,meta):
            row=db.execute('SELECT * FROM requests WHERE id=?',(request_id,)).fetchone()
            if not row: raise LedgerCorrupt('No durable reservation for receipt')
            response_sha=fingerprint(response)
            if row['state']=='settled':
                if row['response_sha']!=response_sha: raise BudgetDenied('Conflicting duplicate receipt')
                return row['charge']
            charge,prompt,output,cached=usage_cost(response,row['maximum'],self.policy)
            if charge>row['reserve']: raise InvalidRequest('Charge exceeds reservation')
            db.execute("UPDATE requests SET state='settled',charge=?,prompt_tokens=?,output_tokens=?,cached_tokens=?,response=?,response_sha=?,finished=? WHERE id=?",
                (charge,prompt,output,cached,encoded(response),response_sha,self.clock(),request_id))
            db.execute('UPDATE meta SET liability=liability-?+? WHERE id=1',(row['reserve'],charge))
        return charge

    def uncertain(self, request_id, note='upstream result unknown', halt=False):
        with self.transaction() as (db,meta):
            db.execute("UPDATE requests SET state='uncertain',note=? WHERE id=? AND state='reserved'",(note[:200],request_id))
            if halt: db.execute("UPDATE meta SET halted=? WHERE id=1",(note[:200],))

    def record_balance(self, data):
        from decimal import Decimal
        values=[Decimal(str(data[k])) for k in ('available_balance','cash_balance','voucher_balance')]
        if any(not v.is_finite() for v in values): raise InvalidRequest('Invalid balance')
        with self.transaction() as (db,meta):
            db.execute('INSERT INTO balances VALUES(?,?,?,?)',(self.clock(),*(str(v) for v in values)))
            if values[0]<=0: db.execute("UPDATE meta SET halted='Provider balance exhausted' WHERE id=1")

    def status(self):
        with self.transaction() as (db,meta):
            rows=db.execute('SELECT state,COUNT(*) AS count,COALESCE(SUM(reserve),0) AS reserved,COALESCE(SUM(charge),0) AS charge FROM requests GROUP BY state').fetchall()
            by_state={row['state']:dict(row) for row in rows}
            settled=by_state.get('settled',{}).get('charge',0)
            reserved=sum(row['reserved'] for row in rows if row['state']!='settled')
            counts={row['state']:row['count'] for row in rows}
            return {'ledger_id':meta['ledger_id'],'model':self.policy.model,
                'authorized_cny':self.policy.authorized_nano/NANO,'allocation_cap_cny':self.policy.allocatable_nano/NANO,
                'settled_cny':settled/NANO,'prior_unverified_cny':self.policy.prior_unverified_nano/NANO,
                'reserved_cny':reserved/NANO,'liability_cny':meta['liability']/NANO,
                'remaining_allocatable_cny':(self.policy.allocatable_nano-meta['liability'])/NANO,
                'counts':counts,'halted':meta['halted'],'deadline_utc':self.policy.deadline_utc}
