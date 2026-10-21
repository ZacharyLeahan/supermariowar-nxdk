#!/usr/bin/env python3
"""Run bounded, auto-rebooting Xbox platform probes; archive every launch."""
import argparse
from hardware_recovery import require_recovery_archived, recovery_path
from datetime import datetime, timezone
from ftplib import FTP, error_perm
import hashlib
import json
import os
from pathlib import Path
import socket
import shutil
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('probe', choices=['audio-startup', 'audio-hal-drain', 'save-store', 'audio-cost', 'audio-close', 'audio-reopen', 'audio-init-protocol', 'timer-granularity'])
p.add_argument('--host', default='192.168.1.254')
p.add_argument('--count', type=int, default=1)
p.add_argument('--wait-seconds', type=int, default=180)
p.add_argument('--output', type=Path)
a = p.parse_args()
if not 1 <= a.count <= 10 or not 30 <= a.wait_seconds <= 600:
    p.error('count must be 1..10; wait-seconds must be 30..600')
modern = Path(__file__).resolve().parents[3]
try: require_recovery_archived(modern, a.host, None)
except (ValueError, KeyError) as error: p.error(str(error))
nxdk = modern.parent / 'nxdk'
probe = modern / 'ports/nxdk/probes' / a.probe
xbe = probe / 'bin/default.xbe'
payload = xbe.read_bytes()
sha = hashlib.sha256(payload).hexdigest()
build_path = xbe.parent / 'BUILD-INFO.json'
if not build_path.exists():
    raise SystemExit('Build with ports/nxdk/tools/build-probe.sh first to record executable-bound provenance')
build_info = json.loads(build_path.read_text())
if build_info.get('xbe_sha256') != sha or build_info.get('kind') != 'probe/' + a.probe:
    raise SystemExit('Probe build provenance is stale or mismatched; rebuild before launching')
remote, logname = {
    'audio-startup': ('SMW-audio-probe', 'audio-startup.log'),
    'audio-hal-drain': ('SMW-hal-drain-probe', 'hal-drain.log'),
    'save-store': ('SMW-save-probe', 'save-store.log'),
    'audio-cost': ('SMW-audio-cost-probe', 'audio-cost.log'),
    'audio-close': ('SMW-audio-close-probe', 'audio-close.log'),
    'audio-reopen': ('SMW-audio-reopen-probe', 'audio-reopen.log'),
    'audio-init-protocol': ('SMW-audio-init-protocol', 'audio-reopen.log'),
    'timer-granularity': ('SMW-timer-probe', 'timer-granularity.log'),
}[a.probe]
extra_logs = ['reopen-watchdog.log', 'audio-close-failure.log', 'audio-reopen-error.log'] if a.probe in ['audio-reopen','audio-init-protocol'] else ['close-watchdog.log'] if a.probe == 'audio-close' else (['timer-watchdog.log'] if a.probe == 'timer-granularity' else [])
assets = {
    'menu.ogg': modern/'data/music/game/Standard/Menu/menu.ogg',
    'overground.ogg': modern/'data/music/game/Standard/SMB_Overground.ogg',
    'stageclear.ogg': modern/'data/music/game/Standard/Special/stageclear.ogg',
} if a.probe == 'audio-cost' else {}
source = probe / ('main.cpp' if (probe/'main.cpp').exists() else 'main.c')
output = a.output or modern.parents[1] / 'logs/hardware' / a.probe / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
output.mkdir(parents=True, exist_ok=True)
(output/'BUILD-INFO.json').write_text(json.dumps(build_info, indent=2)+'\n')
shutil.copytree(xbe.parent/'provenance', output/'provenance', dirs_exist_ok=True)
def git(where, *args):
    return subprocess.check_output(['git', '-C', str(where), *args], text=True).strip()
def connect():
    f = FTP(a.host, timeout=5)
    try:
        f.login(os.getenv('XBOX_FTP_USER', 'xbox'), os.getenv('XBOX_FTP_PASSWORD', 'xbox'))
        return f
    except BaseException:
        f.close()
        raise
with connect() as f:
    f.cwd('/F')
    if remote not in f.nlst():
        f.mkd(remote)
    f.cwd(remote)
    with xbe.open('rb') as src:
        f.storbinary('STOR default.xbe', src, blocksize=65536)
    actual = hashlib.sha256()
    f.retrbinary('RETR default.xbe', actual.update, blocksize=65536)
    if actual.hexdigest() != sha:
        raise SystemExit('Executable readback mismatch; will not launch')
    for name, asset in assets.items():
        with asset.open('rb') as src:
            f.storbinary('STOR '+name, src, blocksize=65536)
        actual = hashlib.sha256()
        f.retrbinary('RETR '+name, actual.update, blocksize=65536)
        if actual.hexdigest() != hashlib.sha256(asset.read_bytes()).hexdigest():
            raise SystemExit('Probe asset readback mismatch; will not launch')
sdl_diff = subprocess.check_output(['git', '-C', str(nxdk/'lib/sdl/SDL2'), 'diff', 'HEAD', '--'], text=True)
(output/'nxdk-sdl.diff').write_text(sdl_diff)
runner_source=Path(__file__).read_bytes()
(output/'runner.py').write_bytes(runner_source)
base = {'runner_sha256':hashlib.sha256(runner_source).hexdigest(), 'probe':a.probe, 'build_info':build_info, 'xbe_sha256':sha, 'source_commit':build_info['source_commit'],
        'source_dirty':build_info['source_dirty'],
        'runner_checkout_commit':git(modern,'rev-parse','HEAD'),
        'runner_checkout_dirty':bool(git(modern,'status','--porcelain','--untracked-files=no')),
        'probe_source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
        'nxdk_revision':build_info['repositories']['repos/nxdk']['revision'],
        'nxdk_sdl_revision':build_info['repositories']['repos/nxdk/lib/sdl/SDL2']['revision'],
        'execution_checkout_nxdk_revision':git(nxdk,'rev-parse','HEAD'),
        'execution_checkout_sdl_revision':git(nxdk/'lib/sdl/SDL2','rev-parse','HEAD'),
        'execution_checkout_sdl_tracked_changes':git(nxdk/'lib/sdl/SDL2','status','--porcelain','--untracked-files=no'),
        'execution_checkout_sdl_diff_sha256':hashlib.sha256(sdl_diff.encode()).hexdigest(),
        'asset_sha256':{name:hashlib.sha256(asset.read_bytes()).hexdigest() for name,asset in assets.items()},
        'data_revision':git(modern/'data','rev-parse','HEAD'),
        'silent_probe':True, 'audible_quality_tested':False, 'emulator_tested':False}
(output/'default.xbe').write_bytes(payload)
for run in range(1, a.count + 1):
    with connect() as f:
        f.cwd('/F/' + remote)
        # Remove the prior diagnostic only in this dedicated probe directory,
        # so a failed launch cannot be mistaken for the last successful run.
        for name in [logname, *extra_logs]:
            try:
                f.delete(name)
            except error_perm:
                if name in f.nlst():
                    raise
        print(f'Launch {run}/{a.count}: {a.probe} {sha}', flush=True)
        try:
            f.sendcmd('SITE RunXBE F:\\' + remote + '\\default.xbe')
        except (OSError, EOFError):
            pass  # disconnect is expected; the new log must still be retrieved
    deadline = time.monotonic() + a.wait_seconds
    raw = None
    extras = {}
    last = None
    time.sleep(10)
    while time.monotonic() < deadline:
        try:
            with connect() as f:
                f.cwd('/F/' + remote)
                chunks = []
                f.retrbinary('RETR ' + logname, chunks.append)
                raw = b''.join(chunks)
                for name in extra_logs:
                    if name in f.nlst():
                        chunks = []
                        f.retrbinary('RETR ' + name, chunks.append)
                        extras[name] = b''.join(chunks)
            break
        except (OSError, EOFError, error_perm) as e:
            last = type(e).__name__
            time.sleep(10)
    result = dict(base, run=run, retrieved_at=datetime.now(timezone.utc).isoformat())
    if raw is None:
        result.update(log_retrieved=False, error=last)
    else:
        (output/f'run-{run:02d}.log').write_bytes(raw)
        result.update(log_retrieved=True, log_sha256=hashlib.sha256(raw).hexdigest(),
                      log_text=raw.decode(errors='replace'))
    if extra_logs:
        result['extra_logs_present'] = list(extras)
        for name, data in extras.items():
            (output/f'run-{run:02d}-{name}').write_bytes(data)
    (output/f'run-{run:02d}.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f'Run {run}: log_retrieved={raw is not None}; evidence={output}', flush=True)
    if any('watchdog' in name or name=='audio-close-failure.log' for name in extras):
        raise SystemExit('Watchdog or cleanup failure archived; stopping the batch for inspection')
    if raw is None:
        record={'format_version':1,'host':a.host,'directory':'/F/'+remote,
            'requires_log_preservation':True,'expected_remote_xbe':'default.xbe',
            'expected_xbe_sha256':sha,'source_commit':build_info['source_commit'],
            'reason':'Platform probe log not archived before timeout; preserve before another launch',
            'failed_run_directory':str(output.resolve()),
            'instruction':'After dashboard recovery, run recover-game-logs.py before another probe launch'}
        gate=recovery_path(modern)
        temporary=gate.with_suffix('.tmp');temporary.write_text(json.dumps(record,indent=2)+'\n');temporary.replace(gate)
        raise SystemExit('Probe log not recovered; recovery gate set; stopping repeated launches')
