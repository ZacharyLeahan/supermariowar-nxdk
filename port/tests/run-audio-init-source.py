#!/usr/bin/env python3
"""The experiment must reject changed SDK bytes and unarchived snapshot edits."""
import importlib.util
from pathlib import Path
port=Path(__file__).resolve().parents[1];probe=port/'probes/audio-init-protocol'
spec=importlib.util.spec_from_file_location('verify_init_source',probe/'verify-source.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
original=(port.parents[2]/'nxdk/lib/hal/audio.c').read_bytes()
variant=(probe/'hal-audio.inc').read_bytes();patch=(probe/'hal-audio.patch').read_bytes()
m.verify(original,variant,patch)
for a,b,c in [(original+b'\n',variant,patch),(original,variant+b'\n',patch),(original,variant,patch+b'\n')]:
    try:m.verify(a,b,c)
    except ValueError:pass
    else:raise AssertionError('Changed SDK/snapshot/patch accepted')
print('PASS: pinned HAL baseline and exact initialization-only snapshot patch guard')
