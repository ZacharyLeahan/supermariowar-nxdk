#!/usr/bin/env python3
"""Fake-FTP paired protocol tests: same image/order, restoration and recovery gate."""
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
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import hardware_recovery
script=Path(__file__).resolve().parents[1]/'tools/run-audio-settle.py'
originals=(ftplib.FTP,subprocess.run,hardware_recovery.require_recovery_archived,hardware_recovery.recovery_path,sys.argv)
try:
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp);build=root/'build';build.mkdir();payload=b'fixture [audio-settle]'
        (build/'default.xbe').write_bytes(payload);sha=hashlib.sha256(payload).hexdigest()
        (build/'BUILD-INFO.json').write_text(json.dumps({'xbe_sha256':sha,'source_commit':'fixture'}))
        dummy=root/'dummy';dummy.write_text('fixture')
        for scenario in ['absent','present','missing-log','bad-marker','zero-counter']:
            remote={} if scenario!='present' else {'audio-settle.txt':b'original setting'}
            launches=[];writes=[];gate=root/(scenario+'-gate.json');out=root/scenario
            class FakeFTP:
                def __init__(self,*args,**kwargs):pass
                def login(self,*args):pass
                def cwd(self,path):assert path=='/F/SMW-nxdk-test'
                def nlst(self):return list(remote)
                def retrbinary(self,cmd,callback):callback(remote[cmd[5:]])
                def storbinary(self,cmd,stream):remote[cmd[5:]]=stream.read();writes.append(remote[cmd[5:]])
                def delete(self,name):del remote[name]
                def close(self):pass
                def __enter__(self):return self
                def __exit__(self,*args):self.close()
            def fake_run(cmd,**kwargs):
                us=int(remote['audio-settle.txt']);launches.append(us)
                target=Path(cmd[cmd.index('--output')+1]);target.mkdir()
                assert Path(cmd[cmd.index('--xbe')+1]).read_bytes()==payload
                if scenario=='missing-log':return subprocess.CompletedProcess(cmd,1)
                actual=999 if scenario=='bad-marker' else us
                text=f'[audio-settle] configuration=valid requested_us={actual} experimental=1\n[audio-settle] start requested_us={us} elapsed_ticks={3375 if us else 8} frequency_hz={0 if scenario=="zero-counter" else 3375000}\n[audio] startup callbacks={5 if us else 0}\n[audio] callbacks={1400 if us else 0}\n'
                (target/'run-01-smw.log').write_text(text)
                (target/'run-01-smw-error.log').write_bytes(b'')
                (target/'run-01.json').write_text(json.dumps({'startup_watchdog_fired':False}))
                return subprocess.CompletedProcess(cmd,0)
            ftplib.FTP=FakeFTP;subprocess.run=fake_run
            hardware_recovery.require_recovery_archived=lambda *args:None
            hardware_recovery.recovery_path=lambda *args:gate
            sys.argv=[str(script),'--build-dir',str(build),'--output',str(out),'--node',str(dummy),'--reset-helper',str(dummy)]
            caught=None
            with contextlib.redirect_stdout(io.StringIO()):
                try:runpy.run_path(str(script),run_name='__main__')
                except RuntimeError as error:caught=error
            result=json.loads((out/'result.json').read_text())
            if scenario in ['absent','present']:
                assert caught is None and launches==[0,1000,1000,0]
                assert result['sidecar_restored'] and len(result['runs'])==4
                assert {r['requested_us'] for r in result['runs']}=={0,1000}
                assert not result['audio_startup_resolved']
                assert remote==({} if scenario=='absent' else {'audio-settle.txt':b'original setting'})
            elif scenario=='missing-log':
                assert caught and launches==[0] and not result['sidecar_restored']
                record=json.loads(gate.read_text());assert record['requires_log_preservation'] and record['expected_xbe_sha256']==sha
                assert remote['audio-settle.txt']==b'0\n' and len(writes)==1
            else:
                assert caught and launches==[0] and result['sidecar_restored'] and not remote
finally:
    ftplib.FTP,subprocess.run,hardware_recovery.require_recovery_archived,hardware_recovery.recovery_path,sys.argv=originals
print('PASS: same-image ABBA protocol, present/absent sidecar restoration, marker refusal and missing-log recovery gate')
