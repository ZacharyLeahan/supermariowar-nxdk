#!/usr/bin/env python3
"""Fake children: same image, exact menu command order, stop/gate on failures."""
import contextlib
import io
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
port=Path(__file__).resolve().parents[1];sys.path.insert(0,str(port/'tools'))
spec=importlib.util.spec_from_file_location('warm',port/'tools/run-game-warm-launches.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.require_recovery_archived=lambda *args:None
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);build=root/'fixture';build.mkdir();(build/'provenance').mkdir()
    payload=b'XBEH [audio-close] configuration=compiled mode=clean experimental=0'
    sha=hashlib.sha256(payload).hexdigest();(build/'default.xbe').write_bytes(payload)
    info={'kind':'game','xbe_sha256':sha,'source_commit':'fixture','effective_game_config':'-DNXDK_STARTUP_WATCHDOG_MS=45000 -DNXDK_EXIT_WATCHDOG_MS=10000'}
    (build/'BUILD-INFO.json').write_text(json.dumps(info));node=root/'node';node.touch()
    for scenario in ['success','audio-stall','watchdog','input-failure','missing-logs','late-input']:
        out=root/scenario;gate=root/(scenario+'-gate.json');m.recovery_path=lambda *args:gate
        a=SimpleNamespace(build_dir=build,output=out,node=node,count=2,host='fixture.invalid',directory='/F/test',kvm='http://fixture.invalid')
        launches=[];buttons=[];clock=[0]
        def fake_run(cmd,**kwargs):
            name=Path(cmd[1]).name
            if name=='launch-game.py':
                launches.append(cmd);target=Path(cmd[cmd.index('--output')+1]);target.mkdir()
                selected=Path(cmd[cmd.index('--build-dir')+1]);assert (selected/'default.xbe').read_bytes()==payload
                if scenario=='late-input':clock[0]=100
            elif name=='kvm-button.mjs':
                assert cmd[2:]==['http://fixture.invalid',cmd[3],'180'];buttons.append(cmd[3])
                if scenario=='input-failure':raise subprocess.CalledProcessError(1,cmd)
            elif name=='collect-game-logs.py':
                if scenario=='missing-logs':raise subprocess.CalledProcessError(1,cmd)
                target=Path(cmd[cmd.index('--output')+1]);target.mkdir()
                count=0 if scenario=='audio-stall' else 4
                raw=(f'[audio-close] configuration=compiled mode=clean experimental=0\n[audio] startup callbacks={count}\n[audio-close] startup init_calls=1 connected_before=0 connected_after=1 connect_return=1\n[audio] Ogg memory stream Standard\\Menu\\menu.ogg\n[audio] callbacks={100 if count else 0}\n[audio-close] clean complete irq_was_connected=1 disconnected=1 dpc_removed=0 locks_released=1 codec_reset=0 analog_control=0 digital_control=0\n').encode()
                (target/'smw.log').write_bytes(raw);(target/'smw-error.log').write_bytes(b'')
                files={'smw.log':{'sha256':hashlib.sha256(raw).hexdigest()}}
                for n in ['startup-watchdog.log','exit-watchdog.log','audio-close-failure.log']:files[n]={'present':scenario=='watchdog' and n=='exit-watchdog.log'}
                (target/'retrieved.json').write_text(json.dumps({'xbe_sha256':sha,'files':files}))
            else:raise AssertionError(cmd)
            return subprocess.CompletedProcess(cmd,0)
        caught=None
        try:
            with contextlib.redirect_stdout(io.StringIO()):r=m.run_batch(a,run=fake_run,sleep=lambda seconds:None,now=lambda:clock[0])
        except (RuntimeError,subprocess.CalledProcessError) as error:caught=error
        r=json.loads((out/'result.json').read_text())
        if scenario=='success':
            assert not caught and r['complete'] and len(launches)==2
            assert buttons==['a','down','down','down','a']*2 and not gate.exists()
            assert all(row['pass'] for row in r['runs'])
        else:
            assert caught and not r['complete'] and len(launches)==1
            if scenario in ['input-failure','missing-logs','late-input']:
                g=json.loads(gate.read_text());assert g['requires_log_preservation'] and g['expected_xbe_sha256']==sha
                if scenario=='late-input':assert not buttons
                if scenario=='input-failure':assert buttons==['a'] and r['stopped_phase']=='input'
            else:assert not r['runs'][0]['pass'] and not gate.exists()
print('PASS: immutable-XBE warm launches, bounded menu inputs, archived failure stop and missing-log recovery gate')
