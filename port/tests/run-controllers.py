#!/usr/bin/env python3
"""Sanitize production nxdk input processing against host SDL virtual devices."""
import os, shlex, subprocess, tempfile
from pathlib import Path
port = Path(__file__).resolve().parents[1]
root = port.parents[1]
cxx = os.environ.get('CXX', '/opt/homebrew/opt/llvm/bin/clang++')
sdl = shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl2'], text=True))
prefix = subprocess.check_output(['pkg-config','--variable=prefix','sdl2'], text=True).strip()
env = dict(os.environ, SDL_VIDEODRIVER='dummy',
    DYLD_LIBRARY_PATH=str(Path(prefix)/'lib')+':/opt/homebrew/lib:'+os.environ.get('DYLD_LIBRARY_PATH',''))
with tempfile.TemporaryDirectory(prefix='smw-controller-test-') as tmp:
    exe = Path(tmp)/'controllers'
    subprocess.run([cxx, '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined', '-DNXDK',
        '-I'+str(root/'src/common'), str(port/'tests/controllers.cpp'),
        str(root/'src/common/input.cpp'), str(root/'src/common/GameModeSettings.cpp'),
        *sdl, '-o', str(exe)], check=True)
    subprocess.run([str(exe)], env=env, check=True, timeout=30)
