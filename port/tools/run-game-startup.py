#!/usr/bin/env python3
"""Bounded game startup observations using an explicitly supplied KVM reset helper."""
import argparse
from hardware_recovery import require_recovery_archived
from datetime import datetime, timezone
from ftplib import FTP, error_perm
import hashlib
import json
import os
from pathlib import Path
import subprocess
import shutil
import time
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--host', default='192.168.1.254')
p.add_argument('--count', type=int, default=1)
p.add_argument('--node', required=True, type=Path)
p.add_argument('--reset-helper', required=True, type=Path)
p.add_argument('--output', required=True, type=Path)
p.add_argument('--xbe', type=Path, help='Preserved reference executable; requires --reference-manifest')
p.add_argument('--reference-manifest', type=Path, help='Hash-bound earlier result for reference executable')
a = p.parse_args()
if not 1 <= a.count <= 10 or not a.node.is_file() or not a.reset_helper.is_file():
    p.error('count must be 1..10; node and reset-helper must exist')
modern = Path(__file__).resolve().parents[3]
try: require_recovery_archived(modern, a.host, '/F/SMW-nxdk-test')
except (ValueError, KeyError) as error: p.error(str(error))
sdk = modern.parent / 'nxdk'
if bool(a.xbe) != bool(a.reference_manifest):
    p.error('--xbe and --reference-manifest must be supplied together')
xbe = a.xbe or modern / 'ports/nxdk/bin/default.xbe'
payload = xbe.read_bytes()
build_path = a.reference_manifest or xbe.parent / 'BUILD-INFO.json'
if not build_path.exists():
    p.error('Build with the versioned game wrapper or supply a preserved hash-bound manifest')
reference = json.loads(build_path.read_text())
if reference.get('xbe_sha256') != hashlib.sha256(payload).hexdigest():
    p.error('Reference executable does not match its recorded hash')
if not a.xbe and not any(marker in payload for marker in [b'backend_callbacks=', b'[audio-dma-failure]']):
    p.error('Build an observer with AUDIO_DIAGNOSTICS=1 or AUDIO_FAILURE_DMA_LOG=1 first')
sha = hashlib.sha256(payload).hexdigest()
a.output.mkdir(parents=True, exist_ok=True)
(a.output/'observer.xbe').write_bytes(payload)
(a.output/'BUILD-INFO.json').write_text(json.dumps(reference,indent=2)+'\n')
if (build_path.parent/'provenance').is_dir():
    shutil.copytree(build_path.parent/'provenance', a.output/'provenance', dirs_exist_ok=True)
def git(where,*args):
    return subprocess.check_output(['git','-C',str(where),*args],text=True).strip()
def connect():
    f=FTP(a.host,timeout=5)
    try:
        f.login(os.getenv('XBOX_FTP_USER','xbox'),os.getenv('XBOX_FTP_PASSWORD','xbox'))
        f.cwd('/F/SMW-nxdk-test')
        return f
    except BaseException:
        f.close()
        raise
base={'xbe_sha256':sha,'source_commit':reference['source_commit'] if reference else git(modern,'rev-parse','HEAD'),
      'source_dirty':reference.get('source_dirty'),
      'reference_build_result':reference,
      'diagnostic_variant':'experimental-dma-startup-restart' if b'protocol=v3' in payload else 'experimental-link-recovery' if b'[audio-link-experiment]' in payload else ('backend-observer' if b'backend_callbacks=' in payload else 'failure-only-register-log'),
      'nxdk_revision':git(sdk,'rev-parse','HEAD'),
      'nxdk_sdl_revision':git(sdk/'lib/sdl/SDL2','rev-parse','HEAD'),
      'startup_watchdog_compiled': b'[startup] diagnostic watchdog armed' in payload,
      'reset_helper_sha256':hashlib.sha256(a.reset_helper.read_bytes()).hexdigest(),
      'audible_quality_tested':False,'emulator_tested':False}
with connect() as f:
    if 'data' not in f.nlst():
        raise SystemExit('Game data missing; will not launch')
    with xbe.open('rb') as src:
        f.storbinary('STOR audio-observer.xbe',src,blocksize=65536)
    actual=hashlib.sha256()
    f.retrbinary('RETR audio-observer.xbe',actual.update,blocksize=65536)
    if actual.hexdigest()!=sha:
        raise SystemExit('Observer readback mismatch; will not launch')
for run in range(1,a.count+1):
    with connect() as f:
        for name in ['smw.log','smw-error.log','startup-watchdog.log']:
            try:f.delete(name)
            except error_perm:
                if name in f.nlst():raise
        print(f'Game startup launch {run}/{a.count}: {sha}',flush=True)
        try:f.sendcmd('SITE RunXBE F:\\SMW-nxdk-test\\audio-observer.xbe')
        except (OSError,EOFError):pass
    # Observe startup, not menu input or speaker output. Title-screen silence
    # after its initial effect is expected; callback progress must still occur.
    time.sleep(30)
    subprocess.run([str(a.node),str(a.reset_helper),'42','40'],check=True,timeout=15)
    deadline=time.monotonic()+180
    logs=None
    while time.monotonic()<deadline:
        time.sleep(10)
        try:
            with connect() as f:
                logs={}
                for name in ['smw.log','smw-error.log']:
                    chunks=[]
                    f.retrbinary('RETR '+name,chunks.append)
                    logs[name]=b''.join(chunks)
                if 'startup-watchdog.log' in f.nlst():
                    chunks=[]
                    f.retrbinary('RETR startup-watchdog.log', chunks.append)
                    logs['startup-watchdog.log']=b''.join(chunks)
            break
        except (OSError,EOFError,error_perm):logs=None
    result=dict(base,run=run,retrieved_at=datetime.now(timezone.utc).isoformat(),logs_retrieved=logs is not None)
    if logs:
        result['startup_watchdog_fired'] = 'startup-watchdog.log' in logs
        for name,raw in logs.items():(a.output/f'run-{run:02d}-{name}').write_bytes(raw)
    (a.output/f'run-{run:02d}.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f'Game startup {run}: logs_retrieved={logs is not None}',flush=True)
    if logs is None:raise SystemExit('No logs recovered; stopping further launches')
