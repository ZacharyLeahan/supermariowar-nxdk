#!/usr/bin/env python3
"""Check that the asset audit rejects synthetic violations, including old history."""
import pathlib,shutil,subprocess,tempfile
source=pathlib.Path(__file__).with_name('audit.py')
def run(repo,*args):return subprocess.run(['git','-C',str(repo),*args],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
with tempfile.TemporaryDirectory(prefix='nxdk-kit-audit-tests-') as tmp:
 for case in ['media-extension','binary-content','asset-patch','deleted-history']:
  repo=pathlib.Path(tmp)/case;(repo/'tools').mkdir(parents=True);shutil.copyfile(source,repo/'tools/audit.py')
  run(repo,'init','-q');run(repo,'config','user.name','Audit fixture');run(repo,'config','user.email','fixture@localhost.invalid');(repo/'README.md').write_text('Synthetic audit fixture; no real game assets.\n');run(repo,'add','.');run(repo,'commit','-qm','Clean fixture')
  good=subprocess.run(['python3',str(repo/'tools/audit.py')],capture_output=True);assert good.returncode==0,(case,good.stdout)
  name='placeholder.png' if case in ('media-extension','deleted-history') else 'payload' if case=='binary-content' else 'test.patch'
  content=b'text fixture' if case!='binary-content' else bytes([0,1,2,3])
  if case=='asset-patch':content=b'diff --git a/data/fixture.wav b/data/fixture.wav\n--- /dev/null\n+++ b/data/fixture.wav\n'
  (repo/name).write_bytes(content);run(repo,'add','.');run(repo,'commit','-qm','Synthetic violation')
  if case=='deleted-history':(repo/name).unlink();run(repo,'add','-A');run(repo,'commit','-qm','Delete fixture but keep its history')
  bad=subprocess.run(['python3',str(repo/'tools/audit.py')],capture_output=True);assert bad.returncode!=0,(case,bad.stdout)
  print('PASS:',case)
