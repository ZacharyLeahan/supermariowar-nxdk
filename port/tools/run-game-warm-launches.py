#!/usr/bin/env python3
"""Bounded same-XBE game starts and normal menu Exits through the authorized KVM."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import time
from hardware_recovery import require_recovery_archived,recovery_path


def run_batch(a, run=subprocess.run, sleep=time.sleep, now=time.monotonic):
    modern=Path(__file__).resolve().parents[3];tools=modern/'ports/nxdk/tools'
    require_recovery_archived(modern,a.host,None)
    if a.output.exists() or not 1<=a.count<=10 or not a.node.is_file():raise ValueError('Use a new output directory, valid Node path and 1..10 runs')
    payload=(a.build_dir/'default.xbe').read_bytes();info=json.loads((a.build_dir/'BUILD-INFO.json').read_text())
    digest=lambda data:hashlib.sha256(data).hexdigest()
    sha=digest(payload);config=info.get('effective_game_config') or ''
    if sha!=info['xbe_sha256'] or info['kind']!='game':raise ValueError('Executable/manifest mismatch')
    if b'[audio-close] configuration=compiled mode=clean experimental=0' not in payload:raise ValueError('Need compiled clean-shutdown candidate')
    if ('-DNXDK_STARTUP_WATCHDOG_MS=45000' not in config or '-DNXDK_EXIT_WATCHDOG_MS=10000' not in config or '-DNXDK_STARTUP_FAULT_STAGE=' in config):raise ValueError('Need 45-second startup/10-second exit guards without fault injection')
    a.output.mkdir(parents=True);build=a.output/'build';build.mkdir()
    (build/'default.xbe').write_bytes(payload);(build/'BUILD-INFO.json').write_text(json.dumps(info,indent=2)+'\n')
    shutil.copytree(a.build_dir/'provenance',build/'provenance')
    helper=a.output/'kvm-button.mjs';helper.write_bytes((tools/'kvm-button.mjs').read_bytes())
    children={name:digest((tools/name).read_bytes()) for name in ['launch-game.py','collect-game-logs.py']}
    for name in children:(a.output/name).write_bytes((tools/name).read_bytes())
    runner=Path(__file__).read_bytes();(a.output/'runner.py').write_bytes(runner)
    result={'xbe_sha256':sha,'source_commit':info['source_commit'],'host':a.host,'directory':a.directory,
            'started_at':datetime.now(timezone.utc).isoformat(),'runner_sha256':digest(runner),'child_sha256':children,
            'button_helper_sha256':digest(helper.read_bytes()),'count_requested':a.count,'runs':[],
            'audible_quality_confirmed':False,'emulator_tested':False,'complete':False,
            'scope':'Same-XBE startup/menu/normal Exit observations; no match, listening, cold power or four-pad acceptance.'}
    def save():(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    def child(name,args,log):
        if digest((tools/name).read_bytes())!=children[name]:raise RuntimeError('Child tool changed during batch')
        with log.open('w') as f:run(['python3',str(tools/name),*args],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=240)
    active=False;phase='not-started';save()
    try:
        for index in range(1,a.count+1):
            out=a.output/f'run-{index:02d}';out.mkdir();deadline=now()+40
            print(f'Warm launch {index}/{a.count}: {sha}',flush=True)
            active=True;phase='staging'
            child('launch-game.py',['--host',a.host,'--directory',a.directory,'--build-dir',str(build.resolve()),'--output',str((out/'launch').resolve())],out/'launcher.txt')
            phase='boot-wait';sleep(20)
            def button(key):
                if now()>=deadline:raise RuntimeError('Input deadline exceeded; refusing late dashboard navigation')
                with (out/'input.txt').open('a') as f:run([str(a.node),str(helper.resolve()),a.kvm,key,'180'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=6)
                sleep(.8)
            phase='input';button('a');sleep(3)
            for key in ['down','down','down','a']:button(key)
            phase='collecting'
            child('collect-game-logs.py',['--host',a.host,'--directory',a.directory,'--program','hardware-session.xbe','--reference-manifest',str((build/'BUILD-INFO.json').resolve()),'--output',str((out/'after').resolve())],out/'collector.txt')
            active=False;phase='classifying'
            r=json.loads((out/'after/retrieved.json').read_text());raw=(out/'after/smw.log').read_bytes();text=raw.decode()
            if r['xbe_sha256']!=sha or digest(raw)!=r['files']['smw.log']['sha256']:raise RuntimeError('Collected identity/hash mismatch')
            startup=re.search(r'\[audio\] startup callbacks=(\d+)',text);last=re.findall(r'\[audio\] callbacks=(\d+)',text)
            failures=[n for n in ['startup-watchdog.log','exit-watchdog.log','audio-close-failure.log'] if r['files'][n]['present']]
            clean=bool(re.search(r'^\[audio-close\] clean complete irq_was_connected=1 disconnected=1 dpc_removed=[01] locks_released=1 codec_reset=0 analog_control=0 digital_control=0$',text,re.M))
            errors=(out/'after/smw-error.log').stat().st_size
            row={'run':index,'startup_callbacks':int(startup[1]) if startup else None,'last_callbacks':int(last[-1]) if last else None,
                 'log_sha256':digest(raw),'stderr_bytes':errors,'failure_markers':failures,'clean_close_observed':clean,
                 'startup_interrupt_connected':'[audio-close] startup init_calls=1 connected_before=0 connected_after=1 connect_return=1' in text,
                 'compiled_clean_mode_logged':'[audio-close] configuration=compiled mode=clean experimental=0' in text,
                 'menu_music_loaded':'Standard\\Menu\\menu.ogg' in text}
            row['pass']=bool(startup and int(startup[1])>0 and last and int(last[-1])>int(startup[1]) and not failures and not errors and clean and row['startup_interrupt_connected'] and row['compiled_clean_mode_logged'] and row['menu_music_loaded'])
            result['runs'].append(row);save();print(json.dumps(row),flush=True)
            if not row['pass']:raise RuntimeError('Archived warm-launch failure; stopping further launches')
        result['complete']=True;save();return result
    except BaseException as error:
        result['stopped_reason']=str(error);result['stopped_phase']=phase;result['incomplete_trial_index']=index;save()
        if active:
            gate=recovery_path(modern);gate.parent.mkdir(parents=True,exist_ok=True)
            record={'format_version':1,'host':a.host,'directory':a.directory,'requires_log_preservation':True,
                    'expected_remote_xbe':'hardware-session.xbe','expected_xbe_sha256':sha,'source_commit':info['source_commit'],
                    'reason':'Warm-launch batch did not archive current title logs; preserve before another launch',
                    'failed_run_directory':str(out.resolve()),'instruction':'After dashboard recovery, run recover-game-logs.py before any title mutation'}
            temporary=gate.with_suffix('.tmp');temporary.write_text(json.dumps(record,indent=2)+'\n');temporary.replace(gate)
        raise

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-dir',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
    p.add_argument('--node',required=True,type=Path);p.add_argument('--count',type=int,default=1)
    p.add_argument('--host',default='192.168.1.254');p.add_argument('--kvm',default='http://192.168.1.169')
    p.add_argument('--directory',default='/F/SMW-nxdk-test');a=p.parse_args()
    if not a.directory.startswith('/') or '..' in a.directory.split('/') or '\\' in a.directory:p.error('Use an absolute Xbox title directory')
    run_batch(a)
