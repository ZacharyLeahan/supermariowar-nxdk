#!/usr/bin/env python3
"""Set/restore only an owned audio diagnostic sidecar; never launches a title."""
import argparse
from datetime import datetime, timezone
from ftplib import FTP
import hashlib
import io
import json
import os
from pathlib import Path
from hardware_recovery import require_recovery_archived


def set_mode(state_dir, mode, host, directory, ftp_factory=FTP, filename="audio-close.txt"):
    allowed={"audio-close.txt":{"legacy","clean","restore"},"audio-init.txt":{"reset","preserve-ready","restore"}}
    if filename not in allowed or mode not in allowed[filename]:raise ValueError("Invalid sidecar/mode combination")
    state_path=state_dir/'state.json'
    existing=json.loads(state_path.read_text()) if state_path.exists() else None
    if existing and (existing['host']!=host or existing['directory']!=directory or existing.get('filename','audio-close.txt')!=filename or not existing['needs_restore']):
        raise ValueError('State belongs to another or completed experiment')
    if not existing and mode=='restore':raise ValueError('No original sidecar snapshot to restore')
    f=ftp_factory(host,timeout=5)
    try:
        f.login(os.getenv('XBOX_FTP_USER','xbox'),os.getenv('XBOX_FTP_PASSWORD','xbox'));f.cwd(directory)
        def read():
            if filename not in f.nlst():return None
            data=[];f.retrbinary('RETR '+filename,data.append);return b''.join(data)
        current=read()
        digest=lambda raw:hashlib.sha256(raw).hexdigest() if raw is not None else None
        if existing:
            if digest(current)!=existing['last_written_sha256']:
                raise ValueError('Diagnostic sidecar changed outside this experiment; refusing overwrite')
            state=existing
        else:
            if state_dir.exists():raise ValueError('Use a new state directory')
            state_dir.mkdir(parents=True)
            if current is not None:(state_dir/('original-'+filename)).write_bytes(current)
            state={'host':host,'directory':directory,'filename':filename,'original_present':current is not None,'original_sha256':digest(current),
                   'last_written_sha256':digest(current),'needs_restore':True,'history':[]}
        def save():
            temporary=state_path.with_suffix('.tmp');temporary.write_text(json.dumps(state,indent=2)+'\n');temporary.replace(state_path)
        save()
        if mode=='restore':
            desired=(state_dir/('original-'+filename)).read_bytes() if state['original_present'] else None
            if digest(desired)!=state['original_sha256']:raise ValueError('Original snapshot checksum mismatch')
        else:desired=(mode+'\n').encode()
        if desired is None:
            if current is not None:f.delete(filename)
        else:f.storbinary('STOR '+filename,io.BytesIO(desired))
        if read()!=desired:raise RuntimeError('Diagnostic sidecar readback mismatch')
        state['last_written_sha256']=digest(desired)
        state['needs_restore']=mode!='restore'
        state['history'].append({'mode':mode,'sha256':digest(desired),'at':datetime.now(timezone.utc).isoformat()})
        save();return state
    finally:f.close()


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--state',required=True,type=Path)
    p.add_argument('--mode',required=True,choices=['legacy','clean','reset','preserve-ready','restore'])
    p.add_argument('--setting',choices=['close','init'],default='close')
    p.add_argument('--host',default='192.168.1.254')
    p.add_argument('--directory',default='/F/SMW-nxdk-test')
    a=p.parse_args()
    if not a.directory.startswith('/') or '..' in a.directory.split('/') or '\\' in a.directory:p.error('Use an absolute Xbox title directory')
    try:
        require_recovery_archived(Path(__file__).resolve().parents[3],a.host,a.directory)
        result=set_mode(a.state,a.mode,a.host,a.directory,filename='audio-'+a.setting+'.txt')
    except (ValueError,RuntimeError) as error:p.error(str(error))
    print(json.dumps(result,indent=2))
