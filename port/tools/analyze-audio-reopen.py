#!/usr/bin/env python3
"""Read-only qualification of one hash-bound silent reopen probe run."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def analyze(evidence, run=1):
    record=json.loads((evidence/f'run-{run:02d}.json').read_text())
    info=json.loads((evidence/'BUILD-INFO.json').read_text())
    raw=(evidence/f'run-{run:02d}.log').read_bytes()
    sha=lambda data:hashlib.sha256(data).hexdigest()
    if (record['xbe_sha256']!=info['xbe_sha256'] or
        sha((evidence/'default.xbe').read_bytes())!=info['xbe_sha256'] or
        sha(raw)!=record['log_sha256']):
        raise ValueError('Executable/log hashes do not match archived provenance')
    text=raw.decode('utf-8',errors='strict')
    headers=re.findall(r'^Silent SDL reopen probe cycles=(\d+) watchdog_ms=(\d+)$',text,re.M)
    if len(headers)!=1 or not 1<=int(headers[0][0])<=500 or not 1000<=int(headers[0][1])<=300000:raise ValueError('Missing or invalid bounded workload header')
    expected_cycles=int(headers[0][0])
    configurations=re.findall(r'^\[audio-close\] configuration=(valid|compiled) mode=(legacy|clean) experimental=([01])$',text,re.M)
    if len(configurations)!=1 or configurations[0] not in [('valid','legacy','1'),('valid','clean','1'),('compiled','clean','0')]:raise ValueError('Missing or ambiguous valid configuration')
    modes=[configurations[0][1]]
    callbacks=[tuple(map(int,m)) for m in re.findall(r'^cycle=(\d+) callbacks_before=(\d+) callbacks_after=(\d+)$',text,re.M)]
    closes=[]
    for m in re.findall(r'^cycle=(\d+) stage=close-return elapsed_ms=(\d+) memory_status=([0-9a-f]+) available_pages=(\d+)$',text,re.M):
        closes.append((int(m[0]),int(m[1]),int(m[2],16),int(m[3])))
    connects=[tuple(map(int,m)) for m in re.findall(r'^\[audio-close\] startup init_calls=(\d+) connected_before=(\d+) connected_after=(\d+) connect_return=(\d+)$',text,re.M)]
    clean=[tuple(map(int,m)) for m in re.findall(r'^\[audio-close\] clean complete irq_was_connected=(\d+) disconnected=(\d+) dpc_removed=(\d+) locks_released=(\d+) codec_reset=(\d+) analog_control=(\d+) digital_control=(\d+)$',text,re.M)]
    extras=record.get('extra_logs_present',[])
    error_path=evidence/f'run-{run:02d}-audio-reopen-error.log'
    error_bytes=error_path.stat().st_size if error_path.exists() else None
    reasons=[]
    init_configs=re.findall(r'^\[audio-init-protocol\] configuration=(\w+) requested=([\w-]+) experimental=1$',text,re.M)
    init_rows=re.findall(r'^\[audio-init-protocol\] requested=(reset|preserve-ready) cold_resets=(\d+) preserved_links=(\d+) before_control=[0-9a-f]{8} before_status=[0-9a-f]{8} after_control=[0-9a-f]{8} after_status=[0-9a-f]{8}$',text,re.M)
    if info.get('kind')=='probe/audio-init-protocol':
        if len(init_configs)!=1 or init_configs[0] not in [('valid','reset'),('valid','preserve-ready')]:reasons.append('init-protocol-mode-not-confirmed')
        elif len(init_rows)!=len(callbacks) or any(mode!=init_configs[0][1] or int(cold)+int(preserved)!=i for i,(mode,cold,preserved) in enumerate(init_rows,1)):reasons.append('init-protocol-ledger-incomplete')
        elif init_configs[0][1]=='preserve-ready' and (not init_rows or int(init_rows[-1][2])==0):reasons.append('preserve-ready-path-not-exercised')
        elif init_configs[0][1]=='reset' and any(int(preserved) for mode,cold,preserved in init_rows):reasons.append('reset-control-preserved-link')
    if any('watchdog' in name or name=='audio-close-failure.log' for name in extras):reasons.append('watchdog-or-cleanup-failure')
    if error_bytes!=0:reasons.append('stderr-not-confirmed-empty')
    if [r[0] for r in callbacks]!=list(range(1,expected_cycles+1)) or any(before<=0 or after<=before for _,before,after in callbacks):reasons.append('not-requested-progressing-cycles')
    if [r[0] for r in closes]!=list(range(1,expected_cycles+1)):reasons.append('not-requested-returned-closes')
    if len(connects)!=expected_cycles or any(r[2:]!=(1,1) for r in connects):reasons.append('interrupt-connection-not-confirmed')
    if f'result={expected_cycles}-open-close-cycles-progressed\n' not in text:reasons.append('terminal-success-missing')
    if modes[0]=='clean' and any(r!=(1,0,1,1) for r in connects):reasons.append('clean-reopen-ownership-not-confirmed')
    if modes[0]=='clean' and (len(clean)!=expected_cycles or any((a,b,d,e,f,g)!=(1,1,1,0,0,0) for a,b,c,d,e,f,g in clean)):reasons.append('clean-shutdown-ledger-incomplete')
    return {'qualifier_source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'xbe_sha256':info['xbe_sha256'],'source_commit':info['source_commit'],'mode':modes[0],
        'configuration_source':configurations[0][0],
        'init_protocol_configuration':re.findall(r'^\[audio-init-protocol\] configuration=(\w+) requested=([\w-]+) experimental=1$',text,re.M),
        'init_protocol_observations':[l for l in text.splitlines() if l.startswith('[audio-init-protocol] requested=')],
        'hardware_tested':True,'emulator_tested':False,'audible_quality_tested':False,
        'expected_cycles':expected_cycles,'watchdog_ms':int(headers[0][1]),'lifecycle_pass':not reasons,'twenty_cycle_lifecycle_pass':not reasons and expected_cycles==20,'rejection_reasons':reasons,
        'callbacks':callbacks,'closes':closes,'connection_returns':connects,
        'stderr_bytes':error_bytes,'extra_logs_present':extras,
        'available_pages_after_each_close':[r[3] for r in closes if r[2]<0x80000000],
        'scope':'Silent in-process lifecycle only; memory samples reported, not a proof of no leaks or prevention of cross-application startup failures.'}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--evidence',required=True,type=Path)
    p.add_argument('--run',type=int,default=1)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args()
    result=analyze(a.evidence,a.run)
    with a.output.open('x') as f:f.write(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
