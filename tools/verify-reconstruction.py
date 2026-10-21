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
print(json.dumps({'files_checked':checked,'errors':errors,'compiled_by_this_check':False,'hardware_tested_by_this_check':False},indent=2))
raise SystemExit(bool(errors))
