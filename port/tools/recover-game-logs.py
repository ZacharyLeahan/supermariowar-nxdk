#!/usr/bin/env python3
"""Archive a pending failed-startup title without uploading or deleting Xbox files."""
import argparse
from ftplib import FTP
import hashlib
import json
import os
from pathlib import Path
from datetime import datetime, timezone
from hardware_recovery import recovery_path

def recover_pending_logs(gate, output, ftp_factory=FTP):
    record=json.loads(gate.read_text())
    if not record.get('requires_log_preservation'): raise ValueError('Recovery has already been archived')
    if output.exists(): raise ValueError('Use a new archive directory')
    remote=record['expected_remote_xbe']
    if remote not in ['audio-observer.xbe','hardware-session.xbe','default.xbe']:
        raise ValueError('Unexpected title filename in recovery record')
    f=ftp_factory(record['host'],timeout=10)
    try:
        f.login(os.getenv('XBOX_FTP_USER','xbox'),os.getenv('XBOX_FTP_PASSWORD','xbox'))
        f.cwd(record['directory'])
        names=f.nlst()
        output.mkdir(parents=True)
        (output/'RECOVERY-BEFORE.json').write_text(json.dumps(record,indent=2)+'\n')
        result={'host':record['host'],'directory':record['directory'],
            'retrieved_at':datetime.now(timezone.utc).isoformat(),'files':{},
            'remote_files_modified':False,'expected_program_verified':False}
        for name in ['smw.log','smw-error.log','smw-pref0.bin','smw-pref1.bin','exit-watchdog.log','startup-watchdog.log','audio-close-failure.log', 'audio-reopen.log', 'audio-reopen-error.log', 'reopen-watchdog.log', 'audio-startup.log', 'hal-drain.log', 'save-store.log', 'audio-cost.log', 'audio-close.log', 'close-watchdog.log', 'timer-granularity.log', 'timer-watchdog.log']:
            if name not in names:
                result['files'][name]={'present':False}
                continue
            digest=hashlib.sha256()
            with (output/name).open('wb') as target:
                def chunk(raw):
                    target.write(raw);digest.update(raw)
                f.retrbinary('RETR '+name,chunk,blocksize=65536)
                target.flush();os.fsync(target.fileno())
            result['files'][name]={'present':True,'bytes':(output/name).stat().st_size,'sha256':digest.hexdigest()}
        if remote in names:
            digest=hashlib.sha256();f.retrbinary('RETR '+remote,digest.update,blocksize=65536)
            result['observed_xbe_sha256']=digest.hexdigest()
            result['expected_program_verified']=digest.hexdigest()==record['expected_xbe_sha256']
        result_path=output/'retrieved.json'
        with result_path.open('w') as target:
            target.write(json.dumps(result,indent=2)+'\n');target.flush();os.fsync(target.fileno())
        if not result['expected_program_verified']:
            raise RuntimeError('Files archived, but program identity differs; recovery gate remains set')
        record.update(requires_log_preservation=False,archived_to=str(output.resolve()),
            archive_manifest_sha256=hashlib.sha256(result_path.read_bytes()).hexdigest())
        temporary=gate.with_suffix('.tmp');temporary.write_text(json.dumps(record,indent=2)+'\n');temporary.replace(gate)
        return result
    finally:f.close()

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',required=True,type=Path,help='New archive directory')
    a=p.parse_args()
    gate=recovery_path(Path(__file__).resolve().parents[3])
    if not gate.exists(): p.error('No pending hardware recovery record')
    try: result=recover_pending_logs(gate,a.output)
    except (ValueError,RuntimeError) as error:p.error(str(error))
    print(json.dumps(result,indent=2))
    print('Old evidence archived; no Xbox files were changed. Future launches may now clear stale logs.')
