#!/usr/bin/env python3
"""Fake-FTP integration: preserve old evidence and unlock only for the expected XBE."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys
import tempfile
port=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(port/'tools'))
from hardware_recovery import require_recovery_archived
spec=importlib.util.spec_from_file_location('recover_game_logs',port/'tools/recover-game-logs.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
files={'smw.log':b'last startup line\n','smw-error.log':b'', 'smw-pref0.bin':bytes(range(256)), 'audio-observer.xbe':b'fixture executable', 'audio-reopen.log':b'cycle=2 stage=open-enter\n', 'audio-reopen-error.log':b'', 'reopen-watchdog.log':b'reopen probe watchdog\n', 'audio-close-failure.log':b'cleanup failure\n'}
class FakeFTP:
    closed=False
    def __init__(self,host,timeout):assert host=='example.invalid' and timeout==10
    def login(self,*args):pass
    def cwd(self,path):assert path=='/F/SMW-nxdk-test'
    def nlst(self):return list(files)
    def retrbinary(self,command,callback,blocksize):
        assert command.startswith('RETR ')
        raw=files[command[5:]]
        for i in range(0,len(raw),7):callback(raw[i:i+7])
    def close(self):FakeFTP.closed=True
    def storbinary(self,*args,**kwargs):raise AssertionError('Remote write forbidden')
    def delete(self,*args):raise AssertionError('Remote delete forbidden')
    def sendcmd(self,*args):raise AssertionError('Remote launch forbidden')
with tempfile.TemporaryDirectory(prefix='smw-recovery-') as tmp:
    root=Path(tmp);modern=root/'repos/modern';modern.mkdir(parents=True)
    gate=root/'logs/hardware/RECOVERY-REQUIRED.json';gate.parent.mkdir(parents=True)
    original={'host':'example.invalid','directory':'/F/SMW-nxdk-test','requires_log_preservation':True,
        'expected_remote_xbe':'audio-observer.xbe','expected_xbe_sha256':hashlib.sha256(files['audio-observer.xbe']).hexdigest()}
    gate.write_text(json.dumps(original))
    try:require_recovery_archived(modern,'example.invalid','/f/smw-nxdk-test/')
    except ValueError:pass
    else:raise AssertionError('Pending recovery did not block same title')
    try:require_recovery_archived(modern,'example.invalid',None)
    except ValueError:pass
    else:raise AssertionError('Pending recovery did not block a probe on the same host')
    require_recovery_archived(modern,'different.invalid','/F/SMW-nxdk-test')
    result=module.recover_pending_logs(gate,root/'archive',FakeFTP)
    assert FakeFTP.closed and result['expected_program_verified'] and not result['remote_files_modified']
    assert not result['files']['smw-pref1.bin']['present']
    for name in ['smw.log','smw-error.log','smw-pref0.bin','audio-reopen.log','audio-reopen-error.log','reopen-watchdog.log','audio-close-failure.log']:
        assert (root/'archive'/name).read_bytes()==files[name]
    assert not json.loads(gate.read_text())['requires_log_preservation']
    require_recovery_archived(modern,'example.invalid','/F/SMW-nxdk-test')
    original['expected_xbe_sha256']='0'*64;gate.write_text(json.dumps(original));before=gate.read_bytes()
    try:module.recover_pending_logs(gate,root/'wrong-program',FakeFTP)
    except RuntimeError:pass
    else:raise AssertionError('Wrong program unlocked recovery')
    assert gate.read_bytes()==before and (root/'wrong-program/smw.log').read_bytes()==files['smw.log']
print('PASS: pending-title gate, byte-exact read-only recovery, missing-file reporting and wrong-XBE refusal')
