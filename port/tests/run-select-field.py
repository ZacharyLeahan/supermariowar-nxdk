#!/usr/bin/env python3
"""Sanitize production selection/slider behavior with graphics excluded."""
import os,shlex,subprocess,tempfile
from pathlib import Path
port=Path(__file__).resolve().parents[1];root=port.parents[1]
cxx=os.environ.get('CXX','/opt/homebrew/opt/llvm/bin/clang++')
sdl=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl2'],text=True))
prefix=subprocess.check_output(['pkg-config','--variable=prefix','sdl2'],text=True).strip()
env=dict(os.environ,DYLD_LIBRARY_PATH=str(Path(prefix)/'lib')+':/opt/homebrew/lib:'+os.environ.get('DYLD_LIBRARY_PATH',''))
with tempfile.TemporaryDirectory(prefix='smw-select-field-') as tmp:
    exe=Path(tmp)/'select-field'
    sources=['input.cpp','GameModeSettings.cpp','uicontrol.cpp','RandomNumberGenerator.cpp','ui/MI_SelectField.cpp','ui/MI_SliderField.cpp','ui/MI_Image.cpp']
    subprocess.run([cxx,'-std=c++20','-O1','-g','-fsanitize=address,undefined','-DNXDK','-I'+str(root/'src/common'),
                    str(port/'tests/select-field.cpp'),*[str(root/'src/common'/s) for s in sources],*sdl,'-o',str(exe)],check=True)
    subprocess.run([str(exe)],env=env,check=True,timeout=30)
