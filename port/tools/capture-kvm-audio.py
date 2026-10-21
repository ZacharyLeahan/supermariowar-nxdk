#!/usr/bin/env python3
"""Record bounded ESPKVM s16le stereo audio without changing its configuration."""
import argparse
import array
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import sys
import time
import urllib.request
import wave

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--url', required=True)
p.add_argument('--seconds', type=int, default=10)
p.add_argument('--output', required=True, type=Path)
a = p.parse_args()
if not 1 <= a.seconds <= 300:
    p.error('seconds must be 1..300')
if a.output.exists():
    p.error('Use a new output directory')
base = a.url.rstrip('/')
def status():
    with urllib.request.urlopen(base + '/api/v1/audio/status', timeout=5) as r:
        return json.load(r)
before = status()
if before.get('streaming'):
    p.error('Audio subscriber already active; leave it untouched and retry later')
if not before.get('available') or not before.get('enabled'):
    p.error('Audio capture unavailable or disabled')
if before.get('rate') != 48000 or before.get('channels') != 2:
    p.error('Expected 48000 Hz stereo; inspect firmware format before recording')
a.output.mkdir(parents=True)
record = {'started_utc': datetime.now(timezone.utc).isoformat(), 'url': base,
          'before': before, 'requested_seconds': a.seconds,
          'subjective_listening_pass': False}
raw = bytearray()
started = time.monotonic()
try:
    with urllib.request.urlopen(base + '/audio.pcm', timeout=12) as r:
        record['headers'] = dict(r.headers)
        if r.headers.get('X-Audio-Format') != 's16le-stereo' or r.headers.get('X-Audio-Rate') != '48000':
            raise ValueError('Unexpected PCM response format')
        while len(raw) < a.seconds * 192000:
            block = r.read(min(19200, a.seconds * 192000 - len(raw)))
            if not block:
                break
            raw.extend(block)
except Exception as e:
    record['error'] = str(e)
record['elapsed_seconds'] = time.monotonic() - started
try:
    record['after'] = status()
except Exception as e:
    record['status_error'] = str(e)
record['received_bytes'] = len(raw)
record['discarded_partial_frame_bytes'] = len(raw) % 4
samples = array.array('h', raw[:len(raw) // 4 * 4])
if sys.byteorder != 'little':
    samples.byteswap()
record['captured_seconds'] = len(samples) / 96000
record['complete'] = len(raw) == a.seconds * 192000 and 'error' not in record
record['channels'] = []
for c in range(2):
    values = samples[c::2]
    record['channels'].append({
        'peak': max(map(abs, values), default=0),
        'rms': math.sqrt(sum(x*x for x in values) / max(1, len(values))),
        'full_scale_samples': sum(abs(x) >= 32767 for x in values),
        'active_samples_over_32': sum(abs(x) > 32 for x in values)})
with wave.open(str(a.output / 'capture.wav'), 'wb') as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(48000)
    w.writeframes(raw[:len(raw) // 4 * 4])
record['wav_sha256'] = hashlib.sha256((a.output / 'capture.wav').read_bytes()).hexdigest()
(a.output / 'capture.json').write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps(record, indent=2))
sys.exit(0 if record['complete'] else 1)
