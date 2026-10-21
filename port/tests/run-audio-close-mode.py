#!/usr/bin/env python3
"""Fake-FTP verification of exact original restoration and outside-change refusal."""
import importlib.util
from pathlib import Path
import sys,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
spec=importlib.util.spec_from_file_location('mode',Path(__file__).resolve().parents[1]/'tools/audio-close-mode.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as tmp:
    for filename,first,second in [('audio-close.txt','legacy','clean'),('audio-init.txt','reset','preserve-ready')]:
      for original in [None,b'original bytes\x00\xff']:
        remote={} if original is None else {filename:original}
        class Fake:
            def __init__(self,*args,**kwargs):pass
            def login(self,*args):pass
            def cwd(self,path):assert path=='/F/test'
            def nlst(self):return list(remote)
            def retrbinary(self,cmd,callback):assert cmd=='RETR '+filename;callback(remote[filename])
            def storbinary(self,cmd,src):assert cmd=='STOR '+filename;remote[filename]=src.read()
            def delete(self,name):assert name==filename;del remote[name]
            def close(self):pass
        state=Path(tmp)/filename/('absent' if original is None else 'present')
        module.set_mode(state,first,'host','/F/test',Fake,filename);assert remote[filename]==(first+'\n').encode()
        other='audio-init.txt' if filename=='audio-close.txt' else 'audio-close.txt'
        try:module.set_mode(state,'reset' if other=='audio-init.txt' else 'legacy','host','/F/test',Fake,other)
        except ValueError:pass
        else:raise AssertionError('State for another sidecar accepted')
        module.set_mode(state,second,'host','/F/test',Fake,filename);assert remote[filename]==(second+'\n').encode()
        remote[filename]=b'outside change'
        try:module.set_mode(state,'restore','host','/F/test',Fake,filename)
        except ValueError:pass
        else:raise AssertionError('Outside edit overwritten')
        assert remote[filename]==b'outside change'
        remote[filename]=(second+'\n').encode()
        r=module.set_mode(state,'restore','host','/F/test',Fake,filename);assert not r['needs_restore']
        assert remote==({} if original is None else {filename:original})
print('PASS: close/init sidecar readback, immutable originals, cross-setting/outside-edit refusal and exact restoration')
