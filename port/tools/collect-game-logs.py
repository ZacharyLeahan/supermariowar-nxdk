#!/usr/bin/env python3
"""Read-only archival from an explicit Xbox title folder after dashboard return."""
import argparse
from datetime import datetime, timezone
from ftplib import FTP, all_errors
import hashlib
import json
import os
from pathlib import Path
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--host',default='192.168.1.254')
p.add_argument('--directory',required=True)
p.add_argument('--program',choices=['default.xbe','hardware-session.xbe','audio-observer.xbe'],default='default.xbe')
p.add_argument('--reference-manifest',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
p.add_argument('--wait-seconds',type=int,default=180)
a=p.parse_args()
if a.output.exists():p.error('Use a new evidence directory')
if not a.directory.startswith('/') or '\\' in a.directory or '..' in a.directory.split('/'):
    p.error('Use an absolute title directory')
if not 1 <= a.wait_seconds <= 300:p.error('wait-seconds must be 1..300')
expected=json.loads(a.reference_manifest.read_text())['xbe_sha256']
deadline=time.monotonic()+a.wait_seconds
while True:
    f=None
    try:
        f=FTP(a.host,timeout=5)
        f.login(os.getenv('XBOX_FTP_USER','xbox'),os.getenv('XBOX_FTP_PASSWORD','xbox'))
        f.cwd(a.directory)
        break
    except all_errors:
        if f:f.close()
        if time.monotonic()>=deadline:raise
        time.sleep(5)
try:
    digest=hashlib.sha256()
    f.retrbinary('RETR '+a.program,digest.update,blocksize=65536)
    if digest.hexdigest()!=expected:raise SystemExit('Remote XBE differs from reference; no logs archived')
    a.output.mkdir(parents=True)
    record={'host':a.host,'directory':a.directory,'xbe_sha256':expected,'program':a.program,
            'collector_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'retrieved_at':datetime.now(timezone.utc).isoformat(),'remote_files_modified':False,
            'acceptance_inferred':False,'files':{}}
    names=f.nlst()
    for name in ['smw.log','smw-error.log','startup-watchdog.log','exit-watchdog.log','audio-close-failure.log','smw-pref0.bin','smw-pref1.bin']:
        if name not in names:
            record['files'][name]={'present':False};continue
        digest=hashlib.sha256()
        with (a.output/name).open('wb') as target:
            def chunk(raw):target.write(raw);digest.update(raw)
            f.retrbinary('RETR '+name,chunk,blocksize=65536)
            target.flush();os.fsync(target.fileno())
        record['files'][name]={'present':True,'bytes':(a.output/name).stat().st_size,'sha256':digest.hexdigest()}
    (a.output/'retrieved.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(record,indent=2))
finally:f.close()
