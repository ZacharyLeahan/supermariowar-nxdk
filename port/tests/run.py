#!/usr/bin/env python3
"""Sanitized mixer, shipped Vorbis assets, and dummy-SDL playback lifecycle checks."""
import os, shlex, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[3]
port=root/'ports/nxdk'
cxx=os.environ.get('CXX','/opt/homebrew/opt/llvm/bin/clang++')
cc=os.environ.get('CC','/opt/homebrew/opt/llvm/bin/clang')
flags=['-O1','-g','-fsanitize=address,undefined']
def run(args,**kw): subprocess.run(list(map(str,args)),check=True,**kw)
with tempfile.TemporaryDirectory(prefix='smw-audio-test-') as tmp:
    work=Path(tmp)
    run([cxx,*flags,'-std=c++20','-I'+str(port),port/'tests/mixer.cpp','-o',work/'mixer'])
    run([work/'mixer'])
    run([cc,*flags,'-I'+str(port/'vendor/stb'),port/'tests/vorbis.c','-lm','-o',work/'vorbis'])
    run([work/'vorbis',*sorted((root/'data/music').rglob('*.ogg'))])
    run([cc,*flags,'-c',port/'Vorbis.c','-o',work/'vorbis.o'])
    sdl=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl2'],text=True))
    run([cxx,*flags,'-std=c++20','-DNXDK','-I'+str(port),'-I'+str(root/'src/common'),port/'tests/backend.cpp',work/'vorbis.o',*sdl,'-o',work/'backend'])
    for name,path in [('coin.wav','sfx/packs/Classic/coin.wav'),('menu.ogg','music/game/Standard/Menu/menu.ogg'),('stageclear.ogg','music/game/Standard/Special/stageclear.ogg')]:
        (work/name).symlink_to(root/'data'/path)
    prefix=subprocess.check_output(['pkg-config','--variable=prefix','sdl2'],text=True).strip()
    # Homebrew SDL2-compat dlopens SDL3 before main; include Homebrew's common lib dir.
    env=dict(os.environ,SDL_AUDIODRIVER='dummy',DYLD_LIBRARY_PATH=str(Path(prefix)/'lib')+':/opt/homebrew/lib:'+os.environ.get('DYLD_LIBRARY_PATH',''))
    run([work/'backend'],cwd=work,env=env,timeout=40)
print('PASS: audio tests (host execution; not an audible hardware-output test)')
