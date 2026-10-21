#!/usr/bin/env python3
"""Install an exact checked package into a NEW Xbox folder, verifying every file."""
import argparse
from datetime import datetime, timezone
from ftplib import FTP
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
from hardware_recovery import require_recovery_archived


def install(package, output, host, directory, launch=False, ftp_factory=FTP):
    if output.exists():
        raise ValueError('Use a new evidence directory')
    target = PurePosixPath(directory)
    if not directory.startswith('/') or '\\' in directory or '..' in target.parts or len(target.parts) != 3:
        raise ValueError('Use a new title directory directly under an Xbox drive, e.g. /F/SMW-v8-test')
    if len(target.parts[1]) != 1 or not target.parts[1].isalpha():
        raise ValueError('Invalid Xbox drive')
    files = sorted(p for p in package.rglob('*') if p.is_file())
    expected = {}
    for line in (package/'SHA256SUMS.txt').read_text().splitlines():
        digest, name = line.split('  ', 1)
        rel = PurePosixPath(name)
        if rel.is_absolute() or '..' in rel.parts or name in expected:
            raise ValueError('Invalid or duplicate checksum path')
        expected[name] = digest
    expected['SHA256SUMS.txt'] = hashlib.sha256((package/'SHA256SUMS.txt').read_bytes()).hexdigest()
    if set(expected) != {p.relative_to(package).as_posix() for p in files}:
        raise ValueError('Package files differ from checksum inventory')
    for p in files:
        name = p.relative_to(package).as_posix()
        for part in (target.name, *PurePosixPath(name).parts):
            if not part.isascii() or len(part) > 42 or any(ord(c) < 32 or c in '\\/:*?"<>|' for c in part):
                raise ValueError('FATX-incompatible path: '+name)
        if len(directory+'/' + name) >= 250 or p.is_symlink():
            raise ValueError('Unsupported package path: '+name)
        if hashlib.sha256(p.read_bytes()).hexdigest() != expected[name]:
            raise ValueError('Local checksum mismatch: '+name)
    info = json.loads((package/'BUILD-INFO.json').read_text())
    if info.get('kind') != 'game' or info['xbe_sha256'] != expected['default.xbe']:
        raise ValueError('Build manifest does not match executable')
    output.mkdir(parents=True)
    record = {'host':host, 'directory':directory, 'xbe_sha256':info['xbe_sha256'],
              'source_commit':info['source_commit'], 'started_at':datetime.now(timezone.utc).isoformat(),
              'installer_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'verified_files':{}, 'installation_verified':False, 'launch_requested':False,
              'hardware_boot_verified':False, 'existing_title_overwritten':False}
    def save():
        temporary = output/'installed.tmp'
        temporary.write_text(json.dumps(record, indent=2)+'\n')
        temporary.replace(output/'installed.json')
    save()
    f = ftp_factory(host, timeout=10)
    try:
        f.login(os.getenv('XBOX_FTP_USER','xbox'), os.getenv('XBOX_FTP_PASSWORD','xbox'))
        f.cwd(str(target.parent))
        names = {PurePosixPath(n.replace('\\','/')).name.casefold() for n in f.nlst()}
        if target.name.casefold() in names:
            raise ValueError('Remote title directory already exists; refusing overwrite')
        f.mkd(target.name)
        f.cwd(directory)
        dirs = {parent for p in files for parent in p.relative_to(package).parents if str(parent) != '.'}
        for d in sorted(dirs, key=lambda d:(len(d.parts), str(d))):
            f.mkd(d.as_posix())
        for i, p in enumerate(files, 1):
            name = p.relative_to(package).as_posix()
            with p.open('rb') as src:
                f.storbinary('STOR '+name, src, blocksize=65536)
            digest = hashlib.sha256()
            f.retrbinary('RETR '+name, digest.update, blocksize=65536)
            if digest.hexdigest() != expected[name]:
                raise RuntimeError('Xbox readback mismatch: '+name)
            record['verified_files'][name] = digest.hexdigest()
            if i % 50 == 0:
                save()
                print(f'Uploaded and verified {i}/{len(files)} files', flush=True)
        record['installation_verified'] = True
        save()
        if launch:
            record['launch_requested'] = True
            save()
            path = target.parts[1]+':\\'+target.name+'\\default.xbe'
            try:
                record['launch_reply'] = f.sendcmd('SITE RunXBE '+path)
            except (OSError, EOFError) as error:
                record['launch_reply_interrupted'] = type(error).__name__
    finally:
        f.close()
        save()
    return record


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--package', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--host', default='192.168.1.254')
    p.add_argument('--directory', required=True)
    p.add_argument('--launch', action='store_true')
    a = p.parse_args()
    try:
        require_recovery_archived(Path(__file__).resolve().parents[3], a.host, None)
        result = install(a.package, a.output, a.host, a.directory, a.launch)
    except (ValueError, RuntimeError) as error:
        p.error(str(error))
    print(json.dumps({k:v for k,v in result.items() if k != 'verified_files'}, indent=2))
    print('Install verification is not boot, audio, save or gameplay acceptance.')
