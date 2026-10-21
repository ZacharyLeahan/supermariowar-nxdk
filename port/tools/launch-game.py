#!/usr/bin/env python3
"""Stage an exact game XBE beside the installed executable, preserving prior evidence."""
import argparse
from hardware_recovery import require_recovery_archived
from ftplib import FTP, error_perm
import hashlib
import json
import os
from pathlib import Path
import shutil
from datetime import datetime, timezone

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-dir', type=Path, required=True)
p.add_argument('--output', type=Path, required=True, help='New directory for manifest, prior logs, save backups and launch record')
p.add_argument('--host', default='192.168.1.254')
p.add_argument('--directory', default='/F/SMW-nxdk-test')
a = p.parse_args()
modern = Path(__file__).resolve().parents[3]
try: require_recovery_archived(modern, a.host, a.directory)
except (ValueError, KeyError) as error: p.error(str(error))
if a.output.exists():
    p.error('Use a new output directory to preserve prior results')
info = json.loads((a.build_dir/'BUILD-INFO.json').read_text())
xbe = a.build_dir/'default.xbe'
sha = hashlib.sha256(xbe.read_bytes()).hexdigest()
if info.get('kind') != 'game' or sha != info.get('xbe_sha256'):
    p.error('Build manifest does not identify this exact game XBE')
if not a.directory.startswith('/') or '\\' in a.directory or any(part in ('.','..') for part in a.directory.split('/')):
    p.error('Use an absolute FTP title directory')
a.output.mkdir(parents=True)
shutil.copyfile(a.build_dir/'BUILD-INFO.json', a.output/'BUILD-INFO.json')
if (a.build_dir/'provenance').is_dir():
    shutil.copytree(a.build_dir/'provenance', a.output/'provenance')
record = {'xbe_sha256': sha, 'source_commit': info['source_commit'],
    'host': a.host, 'directory': a.directory, 'remote_name': 'hardware-session.xbe',
    'launcher_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    'executable_readback_verified': False, 'launch_requested': False,
    'boot_verified': False, 'hardware_acceptance': False,
    'prior_file_sha256': {}, 'started_at': datetime.now(timezone.utc).isoformat()}
def save_record():
    (a.output/'launch.json').write_text(json.dumps(record, indent=2)+'\n')
def read(f, name):
    chunks=[]
    f.retrbinary('RETR '+name, chunks.append)
    return b''.join(chunks)
save_record()
f = FTP(a.host, timeout=5)
try:
    f.login(os.getenv('XBOX_FTP_USER','xbox'), os.getenv('XBOX_FTP_PASSWORD','xbox'))
    f.cwd(a.directory)
    names = f.nlst()
    if 'data' not in names:
        raise RuntimeError('Existing game data directory missing; will not launch')
    prior = a.output/'prior'
    prior.mkdir()
    for name in ['smw.log','smw-error.log','exit-watchdog.log','startup-watchdog.log','audio-close-failure.log','smw-pref0.bin','smw-pref1.bin']:
        if name not in names:
            continue
        raw = read(f, name)
        (prior/name).write_bytes(raw)
        record['prior_file_sha256'][name] = hashlib.sha256(raw).hexdigest()
    save_record()
    # Keep default.xbe and the two live save files untouched. The separate
    # session executable is verified before removing stale test logs.
    with xbe.open('rb') as src:
        f.storbinary('STOR hardware-session.xbe',src,blocksize=65536)
    if hashlib.sha256(read(f,'hardware-session.xbe')).hexdigest() != sha:
        raise RuntimeError('XBE readback mismatch; will not launch')
    record['executable_readback_verified'] = True
    for name in ['smw.log','smw-error.log','exit-watchdog.log','startup-watchdog.log','audio-close-failure.log']:
        if name in names:
            f.delete(name)
    path = a.directory.lstrip('/').replace('/','\\')
    path = path[0]+':'+path[1:]+'\\hardware-session.xbe'
    record['launch_requested'] = True
    record['launch_path'] = path
    save_record()
    try:
        record['ftp_reply'] = f.sendcmd('SITE RunXBE '+path)
    except (OSError, EOFError) as error:
        # The dashboard may terminate FTP before replying. This proves no boot.
        record['ftp_reply_interrupted'] = type(error).__name__
finally:
    f.close()
    save_record()
print(json.dumps(record,indent=2))
print('Verify fresh video and logs before recording any runtime pass.')
