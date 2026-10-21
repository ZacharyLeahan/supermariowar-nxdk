#!/usr/bin/env python3
"""Host-only sanitizer checks for Xbox save records and slot selection (not disk persistence)."""
import os
import hashlib
import re
from pathlib import Path
import subprocess
import tempfile
port = Path(__file__).resolve().parents[1]
bundled = '/opt/homebrew/opt/llvm/bin/clang++'
cxx = os.getenv('CXX', bundled if Path(bundled).is_file() else 'clang++')
# Freeze wire field order independently of the shared encode/decode visitor.
for filename, macro, expected in [
    ('PreferenceGameplayFields.inc', 'NXDK_PREF', '25d548eebd79cd9a74f7809db7cc94b57be87993287439891f61b1be98e413b9'),
    ('PreferenceModeFields.inc', 'NXDK_MODE_PREF', '0913c2569fa1a4e4140674eb6468c6eae63c0df68a5b9825567f4881c7ac56ee'),
]:
    fields = re.findall('^' + macro + r'\(([^,]+),', (port / filename).read_text(), re.M)
    actual = hashlib.sha256(('\n'.join(fields) + '\n').encode()).hexdigest()
    if actual != expected:
        raise SystemExit('Save wire field order changed; require an explicit schema migration: ' + filename)
with tempfile.TemporaryDirectory(prefix='smw-save-record-') as tmp:
    for name in ['save-record', 'save-store', 'preferences']:
        exe = Path(tmp) / name
        subprocess.run([cxx, '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined',
                        '-I'+str(port), '-I'+str(port.parents[1]/'src/common'),
                        str(port/'tests'/f'{name}.cpp'),
                        *([str(port.parents[1]/'src/common/GameModeSettings.cpp')] if name == 'preferences' else []),
                        '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
