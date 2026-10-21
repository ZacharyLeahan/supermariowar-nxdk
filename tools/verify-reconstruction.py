#!/usr/bin/env python3
"""Verify the reconstructed integration files and port overlay against the lock."""
import argparse,hashlib,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--workspace',type=pathlib.Path,required=True);a=p.parse_args();w=a.workspace.resolve();modern=w/'repos/modern';lock=json.loads((root/'upstream-lock.json').read_text());errors=[];checked=0
for rel,h in lock['integration_result_sha256'].items():
 f=modern/rel
 if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=h:errors.append(rel)
 checked+=1
for rel,h in lock['overlay_sha256'].items():
 f=modern/'ports/nxdk'/pathlib.Path(rel).relative_to('port')
 if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=h:errors.append(str(f))
 checked+=1
for link in lock['library_submodules']:
 line=subprocess.check_output(['git','-C',str(modern),'ls-tree','HEAD',link['path']],text=True)
 if link['revision'] not in line:errors.append(link['path']+': gitlink revision')
# Guard the deliberate final-pre-SDL3 game baseline and separate Xbox SDL2 pin.
parent=subprocess.check_output(['git','-C',str(modern),'rev-parse',lock['upstream']['first_sdl3_commit']+'^'],text=True).strip()
if parent!=lock['upstream']['revision']:errors.append('Game pin is not the immediate pre-SDL3 baseline')
sdl=w/'repos/nxdk/lib/sdl/SDL2'
if (sdl/'.git').exists():
 actual=subprocess.check_output(['git','-C',str(sdl),'rev-parse','HEAD'],text=True).strip()
 if actual!=lock['sdl2_revision']:errors.append('Xbox SDL2 library revision mismatch')
 import re
 header=(sdl/'include/SDL_version.h').read_text()
 values=[re.search(r'^#define SDL_'+key+r'\s+(\d+)',header,re.M).group(1) for key in ['MAJOR_VERSION','MINOR_VERSION','PATCHLEVEL']]
 if '.'.join(values)!=lock['sdl2_version']:errors.append('Xbox SDL2 version mismatch')
print(json.dumps({'files_checked' :checked,'errors':errors,'compiled_by_this_check':False,'hardware_tested_by_this_check':False},indent=2))
raise SystemExit(bool(errors))
