#!/usr/bin/env python3
"""Verify explicit-build packaging and rejection paths; requires current build provenance."""
from pathlib import Path
import hashlib,subprocess,tempfile,zipfile,json
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
 # A manifest/executable mismatch must fail before creating the output.
 broken=root/'bad-build'; broken.mkdir(); (broken/'default.xbe').write_bytes(b'bad')
 (broken/'BUILD-INFO.json').write_bytes((build/'BUILD-INFO.json').read_bytes())
 rejected=root/'rejected'
 result=subprocess.run(['python3',str(script),'--build-dir',str(broken),'--output',str(rejected)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 assert result.returncode and not rejected.exists()
 print('PASS: complete checksum/ZIP readback, identical archives from identical inputs, overwrite and XBE mismatch rejection')
