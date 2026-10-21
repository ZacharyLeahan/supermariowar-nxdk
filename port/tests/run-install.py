#!/usr/bin/env python3
"""Exercise new-folder installation, checksum refusal and launch gating with fake FTP."""
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
spec=importlib.util.spec_from_file_location('installer', Path(__file__).resolve().parents[1]/'tools/install-package.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class FakeFTP:
    def __init__(self, existing=False, corrupt=False):
        self.dirs={'/F'} | ({'/F/test'} if existing else set());self.files={};self.current='/';self.launches=[];self.corrupt=corrupt
    def path(self,name):return str(PurePosixPath(name) if name.startswith('/') else PurePosixPath(self.current)/name)
    def login(self,*args):pass
    def cwd(self,name):
        target=self.path(name);assert target in self.dirs;self.current=target
    def nlst(self):return [p for p in self.dirs if str(PurePosixPath(p).parent)==self.current]
    def mkd(self,name):
        target=self.path(name);assert target not in self.dirs;assert str(PurePosixPath(target).parent) in self.dirs;self.dirs.add(target)
    def storbinary(self,cmd,stream,**kwargs):self.files[self.path(cmd[5:])]=stream.read()
    def retrbinary(self,cmd,callback,**kwargs):callback(self.files[self.path(cmd[5:])]+(b'bad' if self.corrupt else b''))
    def sendcmd(self,cmd):self.launches.append(cmd);return '200 launched'
    def close(self):pass
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);package=root/'package';package.mkdir();(package/'data').mkdir()
    xbe=b'XBEH fixture';(package/'default.xbe').write_bytes(xbe)
    (package/'data/test.dat').write_bytes(b'asset fixture')
    (package/'BUILD-INFO.json').write_text(json.dumps({'kind':'game','xbe_sha256':hashlib.sha256(xbe).hexdigest(),'source_commit':'fixture'}))
    files=sorted(p for p in package.rglob('*') if p.is_file())
    (package/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(package).as_posix()+'\n' for p in files))
    f=FakeFTP();r=module.install(package,root/'good','host','/F/test',True,lambda *a,**kw:f)
    assert r['installation_verified'] and len(r['verified_files'])==4
    assert not r['hardware_boot_verified'] and not r['existing_title_overwritten']
    assert f.launches==['SITE RunXBE F:\\test\\default.xbe']
    for p in package.rglob('*'):
        if p.is_file():assert f.files['/F/test/'+p.relative_to(package).as_posix()]==p.read_bytes()
    f=FakeFTP(existing=True)
    try:module.install(package,root/'existing','host','/F/test',True,lambda *a,**kw:f)
    except ValueError:pass
    else:raise AssertionError('Existing folder accepted')
    assert not f.files and not f.launches
    f=FakeFTP(corrupt=True)
    try:module.install(package,root/'corrupt','host','/F/test',True,lambda *a,**kw:f)
    except RuntimeError:pass
    else:raise AssertionError('Bad remote readback accepted')
    assert not f.launches and not json.loads((root/'corrupt/installed.json').read_text())['installation_verified']
    (package/'default.xbe').write_bytes(b'changed')
    def forbidden(*args,**kwargs):raise AssertionError('Connected before local validation')
    try:module.install(package,root/'bad-local','host','/F/test',True,forbidden)
    except ValueError:pass
    else:raise AssertionError('Bad local checksum accepted')
    assert not (root/'bad-local').exists()
print('PASS: exact uploads/readback, existing-folder refusal, local/remote corruption refusal and launch gating')
