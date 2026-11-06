#!/usr/bin/env python3
"""Verify explicit-build packaging and rejection paths; requires current build provenance."""
from pathlib import Path
import hashlib,subprocess,tempfile,zipfile,json,shutil
modern=Path(__file__).resolve().parents[3]
script=modern/'ports/nxdk/tools/package-game.py'
build=modern/'ports/nxdk/bin'
with tempfile.TemporaryDirectory(prefix='smw-package-qa-') as tmp:
 root=Path(tmp)
 for side in ['a','b']:
  target=root/side/'candidate'
  subprocess.run(['python3',str(script),'--build-dir',str(build),'--output',str(target)],check=True,stdout=subprocess.DEVNULL)
  for line in (target/'SHA256SUMS.txt').read_text().splitlines():
   expected,rel=line.split('  ',1)
   assert hashlib.sha256((target/rel).read_bytes()).hexdigest()==expected
  with zipfile.ZipFile(target.with_suffix('.zip')) as z:
   assert z.testzip() is None
   assert all(z.read('candidate/'+p.relative_to(target).as_posix())==p.read_bytes() for p in target.rglob('*') if p.is_file())
  assert (target/'BUILD-INFO.json').read_bytes()==(build/'BUILD-INFO.json').read_bytes()
  # Refuse overwrite even after a complete package exists.
  retry=subprocess.run(['python3',str(script),'--build-dir',str(build),'--output',str(target)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  assert retry.returncode!=0
 assert (root/'a/candidate.zip').read_bytes()==(root/'b/candidate.zip').read_bytes()
 # Binary-only archive must retain provenance/checksums, require external data,
 # and never silently ship media or claim to boot without the missing data.
 bare=root/'binary-only'
 subprocess.run(['python3',str(script),'--build-dir',str(build),'--output',str(bare),'--without-data'],check=True,stdout=subprocess.DEVNULL)
 assert not (bare/'data').exists()
 assert not json.loads((bare/'PACKAGE-INFO.json').read_text())['assets_included']
 assert 'will not boot by itself' in (bare/'README.txt').read_text()
 assert '1139d89ef7e38368536317afd7db54cea2488d5b' in (bare/'README.txt').read_text()
 with zipfile.ZipFile(bare.with_suffix('.zip')) as z:
  assert z.testzip() is None
  assert all('/data/' not in n for n in z.namelist())
 # A revision-bound notice bundle must be copied exactly, while changed text
 # and a bundle for another executable must be rejected before output creation.
 bundle=modern/'ports/nxdk/notices'
 noted=root/'with-notices'
 subprocess.run(['python3',str(script),'--build-dir',str(build),'--output',str(noted),'--without-data','--notices',str(bundle)],check=True,stdout=subprocess.DEVNULL)
 assert json.loads((noted/'PACKAGE-INFO.json').read_text())['notice_files']==len(list(bundle.iterdir()))
 for source in bundle.iterdir():assert (noted/'notices'/source.name).read_bytes()==source.read_bytes()
 bad_notices=root/'bad-notices';shutil.copytree(bundle,bad_notices)
 target_file=json.loads((bad_notices/'INVENTORY.json').read_text())['files'][0]['file']
 original=(bad_notices/target_file).read_bytes();(bad_notices/target_file).write_bytes(original+b'changed')
 rejected_notices=root/'rejected-notices'
 command=['python3',str(script),'--build-dir',str(build),'--output',str(rejected_notices),'--without-data','--notices',str(bad_notices)]
 assert subprocess.run(command,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode and not rejected_notices.exists()
 (bad_notices/target_file).write_bytes(original)
 record=json.loads((bad_notices/'INVENTORY.json').read_text());record['reference_xbe_sha256']='0'*64;(bad_notices/'INVENTORY.json').write_text(json.dumps(record))
 assert subprocess.run(command,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode and not rejected_notices.exists()
 # A manifest/executable mismatch must fail before creating the output.
 broken=root/'bad-build'; broken.mkdir(); (broken/'default.xbe').write_bytes(b'bad')
 (broken/'BUILD-INFO.json').write_bytes((build/'BUILD-INFO.json').read_bytes())
 rejected=root/'rejected'
 result=subprocess.run(['python3',str(script),'--build-dir',str(broken),'--output',str(rejected)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 assert result.returncode and not rejected.exists()
 print('PASS: complete checksum/ZIP readback, identical archives from identical inputs, overwrite, XBE mismatch, changed notices and notice/XBE mismatch rejection')
