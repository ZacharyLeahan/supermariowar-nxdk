#!/usr/bin/env python3
"""Audit reachable history of this lean patch/overlay repository."""
import json,pathlib,re,subprocess,sys
root=pathlib.Path(__file__).resolve().parent.parent
def git(*args):return subprocess.check_output(['git','-C',str(root),*args])
blocked={'.png','.jpg','.jpeg','.gif','.bmp','.ico','.icns','.xpm','.svg','.webp','.tga','.pcx','.wav','.ogg','.mp3','.flac','.mid','.midi','.mp4','.mov','.avi','.zip','.7z','.rar','.tar','.gz','.bz2','.xz','.xbe','.exe','.dll','.obj','.o','.a','.lib','.pdf','.ttf','.otf','.map','.world','.tour'}
entries=git('rev-list','--objects','--all').decode().splitlines();errors=[];blobs=0
# One cat-file process avoids launching a command for every source blob.
ids=[e.split(' ',1)[0] for e in entries]
r=subprocess.run(['git','-C',str(root),'cat-file','--batch'],input=('\n'.join(ids)+'\n').encode(),capture_output=True,check=True);at=0
for entry in entries:
 oid,_,name=entry.partition(' ');end=r.stdout.index(b'\n',at);header=r.stdout[at:end].decode().split();size=int(header[2]);b=r.stdout[end+1:end+1+size];at=end+size+2
 if header[1]!='blob':continue
 blobs+=1
 if pathlib.PurePosixPath(name).suffix.lower() in blocked:errors.append(name+': media/archive/build extension')
 if b'\0' in b:errors.append(name+': binary NUL')
 try:b.decode('utf-8')
 except UnicodeDecodeError:errors.append(name+': non-UTF8')
 signature=rb'(data:(?:image|audio|video)/|GIT binary' rb' patch|iVBORw0K' rb'Ggo|/9j/4A' rb'AQ|/\* XPM \*/)'
 if re.search(signature,b):errors.append(name+': embedded media marker')
 if name.endswith('.patch'):
  for target in re.findall(rb'^\+\+\+ b/(.+)$',b,re.M):
   path=target.decode()
   if pathlib.PurePosixPath(path).suffix.lower() in blocked or path.startswith(('data/','docs/gfx/','resources/icons/')):errors.append(name+': asset patch target '+path)
for line in git('ls-tree','-r','HEAD').decode().splitlines():
 meta,name=line.split('\t',1)
 if meta.startswith('160000 ') or name.startswith(('data/','workspace/','repos/')):errors.append(name+': complete checkout/data/submodule must not be tracked')
print(json.dumps({'reachable_commits':int(git('rev-list','--count','--all')),'reachable_text_blobs_checked':blobs,'errors':errors},indent=2));sys.exit(bool(errors))
