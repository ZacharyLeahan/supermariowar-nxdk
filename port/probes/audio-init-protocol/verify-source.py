#!/usr/bin/env python3
"""Refuse a silent SDK source drift or a snapshot differing from its exact patch."""
from pathlib import Path
import difflib
import hashlib

def verify(original, variant, patch):
    if hashlib.sha256(original).hexdigest()!='6cf57fc5946c614acdae37194a03cb2b850f722e160bbcf43b84d15646b335ec':
        raise ValueError('Selected HAL audio source differs from the pinned experiment baseline')
    actual=''.join(difflib.unified_diff(original.decode().splitlines(keepends=True),variant.decode().splitlines(keepends=True),fromfile='nxdk/lib/hal/audio.c',tofile='probe/hal-audio.inc'))
    if actual.encode()!=patch:raise ValueError('Vendored HAL snapshot differs from its archived diagnostic patch')

if __name__=='__main__':
    p=Path(__file__).resolve().parent
    verify((p.parents[4]/'nxdk/lib/hal/audio.c').read_bytes(),(p/'hal-audio.inc').read_bytes(),(p/'hal-audio.patch').read_bytes())
    print('Pinned HAL and exact initialization-only snapshot patch verified')
