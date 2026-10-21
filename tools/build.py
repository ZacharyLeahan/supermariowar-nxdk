#!/usr/bin/env python3
"""Build only in the external workspace; never create assets/builds in this repo."""
import argparse,hashlib,json,os,pathlib,shutil,subprocess
from datetime import datetime,timezone
root=pathlib.Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--workspace',required=True,type=pathlib.Path);p.add_argument('--clean',action='store_true');p.add_argument('--jobs',type=int,default=4);a=p.parse_args();w=a.workspace.expanduser().resolve()
if root==w or root in w.parents or any(c.isspace() for c in str(w)):p.error('Use an external workspace without whitespace')
if not 1<=a.jobs<=64:p.error('jobs must be 1..64')
prepared=json.loads((w/'PREPARED.json').read_text())
if not prepared['dependencies_initialized']:p.error('Dependencies were skipped; prepare a full workspace first')
modern=w/'repos/modern';sdk=w/'repos/nxdk';port=modern/'ports/nxdk'
env=os.environ.copy();env['NXDK_DIR']=str(sdk);env['SOURCE_DATE_EPOCH']=json.loads((root/'upstream-lock.json').read_text())['source_date_epoch']
env['PATH']=os.pathsep.join([str(sdk/'bin'),'/opt/homebrew/opt/llvm/bin','/usr/local/opt/llvm/bin','/opt/homebrew/opt/bison/bin','/opt/homebrew/opt/flex/bin',env.get('PATH','')])
flags=['PREFERENCES=1','AUDIO_FAILURE_DMA_LOG=1','AUDIO_CLEAN_SHUTDOWN=1','STARTUP_WATCHDOG_MS=45000','EXIT_WATCHDOG_MS=10000']
base=['make','-C',str(port),'CURDIR='+str(port),'-j'+str(a.jobs)]
(w/'logs').mkdir(exist_ok=True)
with (w/'logs/build.log').open('w') as log:
 for cmd in ([base+['clean']+flags] if a.clean else [])+[base+flags]:
  child=subprocess.Popen(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  for line in child.stdout:print(line,end='',flush=True);log.write(line)
  if child.wait():raise SystemExit(child.returncode)
xbe=port/'bin/default.xbe'
if xbe.read_bytes()[:4]!=b'XBEH':raise SystemExit('Output is not an XBE')
def git(repo,*args):return subprocess.check_output(['git','-C',str(repo),*args],text=True).strip()
record={'recorded_utc':datetime.now(timezone.utc).isoformat(),'compiled':True,'emulator_tested':False,'hardware_tested':False,'xbe_sha256':hashlib.sha256(xbe.read_bytes()).hexdigest(),'xbe_bytes':xbe.stat().st_size,'recipe_revision':git(root,'rev-parse','HEAD'),'recipe_status':git(root,'status','--porcelain'),'recipe_diff_sha256':hashlib.sha256(subprocess.check_output(['git','-C',str(root),'diff','HEAD','--'])).hexdigest(),'source_revision':git(modern,'rev-parse','HEAD'),'source_status':git(modern,'status','--porcelain','--untracked-files=no'),'sdk_revision':git(sdk,'rev-parse','HEAD'),'sdk_diff_sha256':hashlib.sha256(subprocess.check_output(['git','-C',str(sdk),'diff','HEAD','--'])).hexdigest(),'sdl2_revision':git(sdk/'lib/sdl/SDL2','rev-parse','HEAD'),'sdl2_diff_sha256':hashlib.sha256(subprocess.check_output(['git','-C',str(sdk/'lib/sdl/SDL2'),'diff','HEAD','--'])).hexdigest(),'flags':flags,'source_date_epoch':env['SOURCE_DATE_EPOCH'],'data_initialized':(modern/'data/.git').exists(),'compiler':subprocess.check_output(['clang','--version'],env=env,text=True).splitlines()[0]}
(port/'bin/KIT-BUILD-INFO.json').write_text(json.dumps(record,indent=2)+'\n')
# Legacy package tools require the data revision; code-only compilation does not.
if (modern/'data/.git').exists():
 subprocess.run(['python3',str(port/'tools/record-build.py'),'--program',str(xbe),'--kind','game','--config',str(port/'.build-config'),'--',*flags],env=env,check=True)
print('Built:',port/'bin/default.xbe','; no emulator/hardware acceptance inferred')
