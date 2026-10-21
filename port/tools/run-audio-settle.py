#!/usr/bin/env python3
"""Paired same-XBE 0/1000us startup observations; restore the diagnostic sidecar."""
import argparse
from datetime import datetime, timezone
from ftplib import FTP, all_errors
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
from hardware_recovery import require_recovery_archived, recovery_path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-dir',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
p.add_argument('--node',required=True,type=Path)
p.add_argument('--reset-helper',required=True,type=Path)
p.add_argument('--cycles',type=int,default=1,help='1..4 cycles; each uses 0,1000,1000,0 microseconds')
p.add_argument('--host',default='192.168.1.254')
a=p.parse_args()
modern=Path(__file__).resolve().parents[3]
directory='/F/SMW-nxdk-test'
try:require_recovery_archived(modern,a.host,directory)
except ValueError as error:p.error(str(error))
if a.output.exists() or not 1<=a.cycles<=4:p.error('Use a new output directory and 1..4 cycles')
xbe=a.build_dir/'default.xbe';reference=a.build_dir/'BUILD-INFO.json'
info=json.loads(reference.read_text());sha=hashlib.sha256(xbe.read_bytes()).hexdigest()
if sha!=info['xbe_sha256'] or b'[audio-settle]' not in xbe.read_bytes():p.error('Need a hash-bound paired-settle executable')
if not a.node.is_file() or not a.reset_helper.is_file():p.error('Node and reset helper must exist')
def connect():
    f=FTP(a.host,timeout=5)
    try:
        f.login(os.getenv('XBOX_FTP_USER','xbox'),os.getenv('XBOX_FTP_PASSWORD','xbox'));f.cwd(directory);return f
    except BaseException:f.close();raise

def read(f,name):
    chunks=[];f.retrbinary('RETR '+name,chunks.append);return b''.join(chunks)

with connect() as f:
    original=read(f,'audio-settle.txt') if 'audio-settle.txt' in f.nlst() else None
a.output.mkdir(parents=True)
if original is not None:(a.output/'original-audio-settle.txt').write_bytes(original)
result={'xbe_sha256':sha,'source_commit':info['source_commit'],'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'started_at':datetime.now(timezone.utc).isoformat(),'order_us':[0,1000,1000,0]*a.cycles,
        'original_sidecar_present':original is not None,'sidecar_restored':False,'runs':[],
        'audible_quality_confirmed':False,'emulator_tested':False,'audio_startup_resolved':False,
        'scope':'Paired diagnostic only; initialization logging and wrappers affect timing. Require failures in zero-delay control before judging prevention.'}
def save():(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
save();allow_restore=True
try:
    for index,us in enumerate(result['order_us'],1):
        raw=(str(us)+'\n').encode()
        with connect() as f:
            f.storbinary('STOR audio-settle.txt',io.BytesIO(raw))
            if read(f,'audio-settle.txt')!=raw:raise RuntimeError('Sidecar readback mismatch')
        out=a.output/f'run-{index:02d}-{us}us'
        print(f'Paired startup {index}/{len(result["order_us"])} requested_us={us}',flush=True)
        cmd=['python3',str(modern/'ports/nxdk/tools/run-game-startup.py'),'--host',a.host,'--count','1',
             '--xbe',str(xbe.resolve()),'--reference-manifest',str(reference.resolve()),'--node',str(a.node.resolve()),
             '--reset-helper',str(a.reset_helper.resolve()),'--output',str(out.resolve())]
        with (a.output/f'runner-{index:02d}.log').open('w') as log:
            try:child=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=360)
            except subprocess.TimeoutExpired:child=None
        logpath=out/'run-01-smw.log'
        if child is None or child.returncode or not logpath.exists():
            allow_restore=False
            recovery_path(modern).write_text(json.dumps({'format_version':1,'host':a.host,'directory':directory,
                'requires_log_preservation':True,'expected_remote_xbe':'audio-observer.xbe','expected_xbe_sha256':sha,
                'source_commit':info['source_commit'],'reason':'Paired startup failed to archive logs; preserve before another launch',
                'failed_run_directory':str(out.resolve()),'instruction':'Run recover-game-logs.py after dashboard recovery before any title mutation'},indent=2)+'\n')
            raise RuntimeError('Startup logs unavailable; recovery gate set; sidecar left unchanged for evidence')
        text=logpath.read_text();startup=json.loads((out/'run-01.json').read_text())
        marker=f'[audio-settle] configuration=valid requested_us={us} experimental=1'
        measured=re.search(r'\[audio-settle\] start requested_us=(\d+) elapsed_ticks=(\d+) frequency_hz=(\d+)',text)
        if marker not in text or not measured or int(measured[1])!=us:
            raise RuntimeError('Configuration or measured start marker missing; do not classify this trial')
        callbacks=int(re.search(r'\[audio\] startup callbacks=(\d+)',text)[1])
        ticks,freq=int(measured[2]),int(measured[3])
        if freq<=0:raise RuntimeError('Performance-counter frequency unavailable; cannot qualify wait')
        row={'run':index,'requested_us':us,'elapsed_ticks':ticks,'frequency_hz':freq,
             'delay_undershot_with_one_tick_uncertainty':bool(us and (ticks+1)*1000000<us*freq),
             'startup_callbacks':callbacks,'last_callbacks':int(re.findall(r'\[audio\] callbacks=(\d+)',text)[-1]),
             'stderr_bytes':(out/'run-01-smw-error.log').stat().st_size,'watchdog_fired':startup['startup_watchdog_fired'],
             'sidecar_sha256':hashlib.sha256(raw).hexdigest(),'log_sha256':hashlib.sha256(logpath.read_bytes()).hexdigest()}
        result['runs'].append(row);save();print(json.dumps(row),flush=True)
        if row['watchdog_fired'] or row['delay_undershot_with_one_tick_uncertainty']:
            raise RuntimeError('Watchdog or undershoot requires inspection; stopping batch')
finally:
    try:
        if allow_restore:
            with connect() as f:
                if original is None:
                    if 'audio-settle.txt' in f.nlst():f.delete('audio-settle.txt')
                    result['sidecar_restored']='audio-settle.txt' not in f.nlst()
                else:
                    f.storbinary('STOR audio-settle.txt',io.BytesIO(original));result['sidecar_restored']=read(f,'audio-settle.txt')==original
    except all_errors as error:
        result['restoration_error']=type(error).__name__+': '+str(error)
    finally:save()
if not result['sidecar_restored']:raise SystemExit('Diagnostic sidecar restoration unverified')
print('Paired observations archived; no fix inferred automatically.',flush=True)
