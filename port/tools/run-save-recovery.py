#!/usr/bin/env python3
"""Native schema-3..8 save recovery fixture; backs up and restores both title slots."""
import argparse
from hardware_recovery import require_recovery_archived
from ftplib import FTP, all_errors
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import time
import zlib
from save_preflight import validate_preference_record, validate_fixture_structure

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--host', default='192.168.1.254')
p.add_argument('--xbe', type=Path, required=True)
p.add_argument('--reference-manifest', type=Path, required=True)
p.add_argument('--node', type=Path, required=True)
p.add_argument('--reset-helper', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--kind', choices=['invalid-choice', 'invalid-controller', 'bad-crc', 'both-bad-crc', 'truncated', 'unsupported-schema'], required=True)
a = p.parse_args()
if not a.node.is_file() or not a.reset_helper.is_file():
    p.error('Node and reset helper must exist before touching saves')
if a.output.exists():
    p.error('Use a new output directory to preserve earlier evidence')
reference = json.loads(a.reference_manifest.read_text())
if hashlib.sha256(a.xbe.read_bytes()).hexdigest() != reference['xbe_sha256']:
    p.error('Executable differs from reference manifest')
modern = Path(__file__).resolve().parents[3]
try: require_recovery_archived(modern, a.host, '/F/SMW-nxdk-test')
except (ValueError, KeyError) as error: p.error(str(error))
def connect():
    f = FTP(a.host, timeout=5)
    try:
        f.login(os.getenv('XBOX_FTP_USER', 'xbox'), os.getenv('XBOX_FTP_PASSWORD', 'xbox'))
        f.cwd('/F/SMW-nxdk-test')
        return f
    except BaseException:
        f.close()
        raise

def read(f, name):
    chunks = []
    f.retrbinary('RETR ' + name, chunks.append)
    return b''.join(chunks)


with connect() as f:
    originals = [read(f, f'smw-pref{i}.bin') for i in range(2)]
try:
    generations = [validate_preference_record(raw) for raw in originals]
except ValueError as error:
    raise SystemExit(str(error) + '; no files changed') from error
delta = (generations[1] - generations[0]) & 0xffffffff
if delta in (0, 0x80000000):
    raise SystemExit('Ambiguous generations; no files changed')
newer = 1 if delta < 0x80000000 else 0
older = 1 - newer
fixture = bytearray(originals[newer])
if a.kind == 'invalid-controller':
    if struct.unpack_from('<I', fixture, 24)[0] not in (7, 8):
        raise SystemExit('Invalid-controller fixture requires newer schema-7/8 record; no files changed')
    # P1 joystick device: four players are supported, so index 4 is invalid.
    struct.pack_into('<I', fixture, 24 + 2532 + 76, 4)
elif a.kind == 'invalid-choice':
    # Schemas 3..8 retain respawn at appended global field 25; allowed 0..20.
    struct.pack_into('<I', fixture, 24 + 1832 + 25 * 4, 21)
elif a.kind == 'unsupported-schema':
    struct.pack_into('<I', fixture, 24, 99)
elif a.kind in ['bad-crc', 'both-bad-crc']:
    fixture[20] ^= 1
else:
    fixture = fixture[:40]
if a.kind in ['invalid-choice', 'invalid-controller', 'unsupported-schema']:
    struct.pack_into('<I', fixture, 20, zlib.crc32(fixture[:20] + fixture[24:]))
try:
    validate_fixture_structure(fixture, a.kind)
except ValueError as error:
    raise SystemExit(str(error) + '; no files changed') from error
a.output.mkdir(parents=True)
for i, raw in enumerate(originals):
    (a.output / f'original-smw-pref{i}.bin').write_bytes(raw)
(a.output / 'fixture.bin').write_bytes(fixture)
result = {'kind': a.kind, 'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'reference_build': reference, 'generations': generations,
          'payload_schemas': [struct.unpack_from('<I', raw, 24)[0] for raw in originals],
          'fixture_slot': newer, 'fixture_slots': [0, 1] if a.kind == 'both-bad-crc' else [newer],
          'expected_loaded_generation': None if a.kind == 'both-bad-crc' else generations[older],
          'original_slots_restored': False, 'recovery_verified': False,
          'emulator_tested': False, 'audible_quality_confirmed': False,
          'original_sha256': [hashlib.sha256(raw).hexdigest() for raw in originals],
          'fixture_sha256': hashlib.sha256(fixture).hexdigest()}
(a.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
try:
    with connect() as f:
        for slot in result['fixture_slots']:
            name = f'smw-pref{slot}.bin'
            f.storbinary('STOR ' + name, io.BytesIO(fixture))
            if read(f, name) != fixture:
                raise RuntimeError('Fixture readback mismatch')
    completed = subprocess.run([
        'python3', str(modern / 'ports/nxdk/tools/run-game-startup.py'),
        '--host', a.host, '--count', '1', '--xbe', str(a.xbe.resolve()),
        '--reference-manifest', str(a.reference_manifest.resolve()),
        '--node', str(a.node.resolve()), '--reset-helper', str(a.reset_helper.resolve()),
        '--output', str((a.output / 'startup').resolve())], check=False, timeout=360)
    result['startup_runner_exit_code'] = completed.returncode
    log = a.output / 'startup/run-01-smw.log'
    err = a.output / 'startup/run-01-smw-error.log'
    if log.exists() and err.exists():
        text = log.read_text()
        if a.kind == 'both-bad-crc':
            result['loaded_defaults'] = '[save] defaults: no valid supported preference slot' in text and '[save] preferences loaded generation=' not in text
            result['loaded_expected_state'] = result['loaded_defaults']
        else:
            marker = f'[save] preferences loaded generation={generations[older]} '
            result['loaded_expected_generation'] = marker in text
            result['loaded_expected_state'] = result['loaded_expected_generation']
        result['recovery_verified'] = result['loaded_expected_state'] and err.stat().st_size == 0
        result['stderr_bytes'] = err.stat().st_size
finally:
    # This fixture is temporary even on failure. Never leave the known-bad
    # slot installed intentionally; record any inability to restore explicitly.
    deadline = time.monotonic() + 180
    while time.monotonic() < deadline:
        try:
            with connect() as f:
                for i, raw in enumerate(originals):
                    name = f'smw-pref{i}.bin'
                    f.storbinary('STOR ' + name, io.BytesIO(raw))
                    if read(f, name) != raw:
                        raise OSError('Restore readback mismatch')
            result['original_slots_restored'] = True
            break
        except all_errors:
            time.sleep(10)
    (a.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result), flush=True)
if not result['original_slots_restored'] or not result['recovery_verified']:
    raise SystemExit('Recovery or restoration unverified; inspect result.json before further launches')
