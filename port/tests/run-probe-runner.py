#!/usr/bin/env python3
"""Fake-FTP probe runner: readback, fresh marker archival and timeout gating."""
import contextlib
import ftplib
import hashlib
import io
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import time
port=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(port/'tools'))
import hardware_recovery
script=port/'tools/run-audio-probe.py'
if not (port/'probes/audio-reopen/bin/BUILD-INFO.json').is_file():
    raise SystemExit('Build the audio-reopen probe first; this test never launches hardware')
originals=(ftplib.FTP,subprocess.check_output,time.sleep,time.monotonic,hardware_recovery.require_recovery_archived,hardware_recovery.recovery_path,sys.argv)
try:
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        for scenario in ['success','watchdog','missing-log','bad-readback']:
            files={};launches=[];elapsed=[0];out=root/scenario;gate=root/(scenario+'-gate.json')
            class FakeFTP:
                def __init__(self,*args,**kwargs):pass
                def login(self,*args):pass
                def cwd(self,*args):pass
                def nlst(self):return list(files)
                def mkd(self,name):files[name]=b''
                def storbinary(self,cmd,stream,blocksize):files[cmd[5:]]=stream.read()
                def retrbinary(self,cmd,callback,blocksize=None):
                    name=cmd[5:]
                    if name not in files:raise ftplib.error_perm('550 missing')
                    raw=files[name]
                    if scenario=='bad-readback' and name=='default.xbe':raw=b'wrong executable'
                    callback(raw)
                def delete(self,name):
                    if name not in files:raise ftplib.error_perm('550 missing')
                    del files[name]
                def sendcmd(self,cmd):
                    launches.append(cmd)
                    if scenario in ['success','watchdog']:
                        files['audio-reopen.log']=b'result=fixture-only\n'
                        files['audio-reopen-error.log']=b''
                        if scenario=='watchdog':files['reopen-watchdog.log']=b'fixture watchdog\n'
                    return '200 fixture launch'
                def close(self):pass
                def __enter__(self):return self
                def __exit__(self,*args):pass
            ftplib.FTP=FakeFTP
            subprocess.check_output=lambda *args,**kwargs:'fixture' if kwargs.get('text') else b'fixture'
            time.monotonic=lambda:elapsed[0]
            time.sleep=lambda seconds:elapsed.__setitem__(0,elapsed[0]+seconds)
            hardware_recovery.require_recovery_archived=lambda *args:None
            hardware_recovery.recovery_path=lambda *args:gate
            sys.argv=[str(script),'audio-reopen','--count','2','--wait-seconds','30','--output',str(out)]
            error=None
            with contextlib.redirect_stdout(io.StringIO()):
                try:runpy.run_path(str(script),run_name='__main__')
                except SystemExit as e:error=e
            if scenario=='success':
                assert error is None and len(launches)==2 and not gate.exists()
                for i in [1,2]:
                    record=json.loads((out/f'run-{i:02d}.json').read_text())
                    assert record['source_commit']==record['build_info']['source_commit']
                    assert record['nxdk_revision']==record['build_info']['repositories']['repos/nxdk']['revision']
                    assert record['runner_checkout_commit']=='fixture'
                    assert record['runner_sha256']==hashlib.sha256((out/'runner.py').read_bytes()).hexdigest()
                    assert record['log_retrieved'] and not record['audible_quality_tested']
                    assert record['extra_logs_present']==['audio-reopen-error.log']
            elif scenario=='watchdog':
                assert error and len(launches)==1 and not gate.exists()
                assert (out/'run-01-reopen-watchdog.log').read_bytes()==b'fixture watchdog\n'
            elif scenario=='missing-log':
                assert error and len(launches)==1
                record=json.loads(gate.read_text())
                assert record['requires_log_preservation'] and record['directory']=='/F/SMW-audio-reopen-probe'
                assert record['expected_remote_xbe']=='default.xbe'
            else:assert error and not launches
finally:
    ftplib.FTP,subprocess.check_output,time.sleep,time.monotonic,hardware_recovery.require_recovery_archived,hardware_recovery.recovery_path,sys.argv=originals
print('PASS: executable readback refusal, fresh probe/marker archival and missing-log stop/recovery gate')
