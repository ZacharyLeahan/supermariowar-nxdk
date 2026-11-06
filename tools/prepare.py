#!/usr/bin/env python3
"""Reconstruct the pinned nxdk port in a new workspace outside this repository."""
import argparse,hashlib,json,pathlib,shutil,subprocess
root=pathlib.Path(__file__).resolve().parent.parent
lock=json.loads((root/'upstream-lock.json').read_text())
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--workspace',type=pathlib.Path,required=True)
p.add_argument('--with-data',action='store_true',help='Explicitly obtain external game assets for local use')
p.add_argument('--source-cache',type=pathlib.Path,help='Optional existing modern checkout to clone locally')
p.add_argument('--sdk-cache',type=pathlib.Path,help='Optional existing nxdk checkout to clone locally')
p.add_argument('--skip-dependencies',action='store_true',help='Source/patch verification only; not a buildable workspace')
a=p.parse_args();w=a.workspace.expanduser().resolve()
if a.with_data and a.skip_dependencies:p.error('--with-data requires dependency initialization')
if w.exists():p.error('Use a new workspace; existing directories are never overwritten')
if root==w or root in w.parents:p.error('Full checkouts and data must stay outside the lean repository')
if any(c.isspace() for c in str(w)):p.error('Use a workspace path without whitespace for nxdk Makefiles')
for rel,h in {**lock['overlay_sha256'],**{'patches/'+k:v for k,v in lock['patches_sha256'].items()}}.items():
 if hashlib.sha256((root/rel).read_bytes()).hexdigest()!=h:p.error('Recipe integrity mismatch: '+rel)
w.mkdir(parents=True);(w/'repos').mkdir()
def run(*cmd,cwd=None):subprocess.run([str(x) for x in cmd],cwd=cwd,check=True)
def git(repo,*args):run('git','-C',repo,*args)
def clone(info,dest,cache):
 run('git','clone','--no-checkout',cache.resolve() if cache else info['url'],dest)
 git(dest,'remote','set-url','origin',info['url']);git(dest,'checkout','--detach',info['revision'])
def modules(repo,cache=None,selected=None):
 f=repo/'.gitmodules'
 if not f.exists():return
 r=subprocess.run(['git','config','--file',str(f),'--get-regexp',r'^submodule\..*\.path$'],capture_output=True,text=True)
 if r.returncode not in (0,1):raise RuntimeError(r.stderr)
 for line in r.stdout.splitlines():
  key,path=line.split(None,1)
  if selected is not None and path not in selected:continue
  name=key[len('submodule.'):-len('.path')];urlkey='submodule.'+name+'.url'
  original=subprocess.check_output(['git','config','--file',str(f),'--get',urlkey],text=True).strip()
  local=cache/path if cache else None
  if local and (local/'.git').exists():
   git(repo,'config',urlkey,str(local.resolve()))
   try:git(repo,'-c','protocol.file.allow=always','submodule','update','--init','--',path)
   finally:git(repo,'config',urlkey,original)
  else:git(repo,'submodule','update','--init','--',path)
  modules(repo/path,local)
modern=w/'repos/modern';sdk=w/'repos/nxdk'
clone(lock['upstream'],modern,a.source_cache)
git(modern,'apply','--check',root/'patches/game.patch');git(modern,'apply','--index',root/'patches/game.patch')
shutil.copytree(root/'port',modern/'ports/nxdk',dirs_exist_ok=True)
for link in lock['library_submodules']:(modern/link['path']).mkdir(parents=True,exist_ok=True)
git(modern,'add','ports/nxdk')
# Local integration commit; upstream commits remain in this external checkout only.
run('git','-C',modern,'-c','user.name=nxdk port recipe','-c','user.email=recipe@localhost.invalid','commit','-m','Apply pinned nxdk port overlay and integration patches')
clone(lock['nxdk'],sdk,a.sdk_cache)
for name in ['nxdk-audio-ready.patch','nxdk-cxbe-epoch.patch','nxdk-clean-libjpeg.patch']:
 git(sdk,'apply','--check',root/'patches'/name);git(sdk,'apply',root/'patches'/name)
if not a.skip_dependencies:
 selected=[x['path'] for x in lock['library_submodules']]
 if a.with_data:selected.append('data')
 modules(modern,a.source_cache,selected);modules(sdk,a.sdk_cache)
 sdl=sdk/'lib/sdl/SDL2'
 rev=subprocess.check_output(['git','-C',str(sdl),'rev-parse','HEAD'],text=True).strip()
 if rev!=lock['sdl2_revision']:raise SystemExit('Unexpected SDL2 revision')
 git(sdl,'apply','--check',root/'patches/sdl2-audio-close.patch');git(sdl,'apply',root/'patches/sdl2-audio-close.patch')
elif a.with_data:p.error('--with-data requires dependency initialization')
record={'recipe_revision':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip() if (root/'.git').exists() else 'uncommitted recipe','upstream_revision':lock['upstream']['revision'],'source_port_revision':lock['port_source_revision'],'dependencies_initialized':not a.skip_dependencies,'external_assets_requested':a.with_data,'compiled':False,'hardware_tested':False}
(w/'PREPARED.json').write_text(json.dumps(record,indent=2)+'\n')
print('Prepared:',w,'; external assets requested:',a.with_data,'; not compiled or hardware tested')
