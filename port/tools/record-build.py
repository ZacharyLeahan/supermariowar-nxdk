#!/usr/bin/env python3
"""Bind a successfully built XBE to source revisions, local patches and SDK archives."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import struct
from pathlib import Path
import shutil
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--program', type=Path, required=True)
p.add_argument('--kind', required=True)
p.add_argument('--config', type=Path)
p.add_argument('make_arguments', nargs=argparse.REMAINDER)
a = p.parse_args()
modern = Path(__file__).resolve().parents[3]
workspace = modern.parents[1]
sdk = modern.parent / 'nxdk'
program = a.program.resolve()
if program.read_bytes()[:4] != b'XBEH':
    raise SystemExit('Successful make did not produce an XBEH executable')
out = program.parent / 'provenance'
out.mkdir(exist_ok=True)
def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()
def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])
repos = {}
visited = set()
def snapshot(repo):
    repo = repo.resolve()
    if repo in visited: return
    visited.add(repo)
    label = str(repo.relative_to(workspace))
    diff = git(repo, 'diff', '--binary', '--ignore-submodules=untracked', 'HEAD', '--')
    diff_name = 'repo-' + hashlib.sha256(label.encode()).hexdigest()[:16] + '.diff'
    (out / diff_name).write_bytes(diff)
    untracked = git(repo, 'ls-files', '--others', '--exclude-standard', '-z').decode().split('\0')
    source_suffixes = {'.c', '.cpp', '.h', '.hpp', '.inc', '.py', '.sh', '.mjs', '.patch'}
    sources = {name: digest(repo / name) for name in untracked if name and
               (repo / name).is_file() and ((repo / name).suffix in source_suffixes or (repo / name).name == 'Makefile')}
    repos[label] = {
        'revision': git(repo, 'rev-parse', 'HEAD').decode().strip(),
        'tracked_status': git(repo, 'status', '--porcelain', '--untracked-files=no', '--ignore-submodules=untracked').decode(),
        'diff_file': 'provenance/' + diff_name,
        'diff_sha256': hashlib.sha256(diff).hexdigest(),
        'untracked_source_sha256': sources,
    }
    modules = repo / '.gitmodules'
    if modules.exists():
        paths = subprocess.run(['git', '-C', str(repo), 'config', '--file', str(modules),
                                '--get-regexp', r'^submodule\..*\.path$'], capture_output=True, text=True)
        if paths.returncode not in (0, 1): raise RuntimeError(paths.stderr)
        for line in paths.stdout.splitlines():
            child = repo / line.split(None, 1)[1]
            if not (child / '.git').exists(): raise RuntimeError('Uninitialized dependency: ' + str(child))
            child.resolve().relative_to(repo)  # submodules must remain within this repository
            snapshot(child)
for repo in [modern, sdk, modern.parent / 'historical']:
    if (repo / '.git').exists(): snapshot(repo)

def version(name):
    path = shutil.which(name)
    if not path: return None
    r = subprocess.run([path, '--version'], capture_output=True, text=True, timeout=10)
    return {'path': path, 'version': (r.stdout + r.stderr).strip()}
xbe = program.read_bytes()
base = struct.unpack_from('<I', xbe, 0x104)[0]
certificate = struct.unpack_from('<I', xbe, 0x118)[0]
stamps = [struct.unpack_from('<I', xbe, 0x114)[0],
          struct.unpack_from('<I', xbe, certificate - base + 4)[0]]
epoch = os.environ.get('SOURCE_DATE_EPOCH')
if epoch is not None and (not epoch.isascii() or not epoch.isdecimal() or
                          int(epoch) > 0xffffffff or stamps != [int(epoch)] * 2):
    raise SystemExit('XBE timestamps do not match valid SOURCE_DATE_EPOCH; rebuild cxbe and the XBE')
record = {
    'format_version': 1, 'kind': a.kind,
    'source_date_epoch': epoch, 'xbe_timestamps': stamps,
    'cxbe_sha256': digest(sdk / 'tools/cxbe/cxbe'),
    'recorded_at': datetime.now(timezone.utc).isoformat(),
    'source_commit': repos['repos/modern']['revision'],
    'source_dirty': bool(repos['repos/modern']['tracked_status'] or repos['repos/modern']['untracked_source_sha256']),
    'xbe_sha256': digest(program), 'xbe_bytes': program.stat().st_size,
    'wrapper_arguments': a.make_arguments[1:] if a.make_arguments[:1] == ['--'] else a.make_arguments,
    'effective_game_config': a.config.read_text() if a.config and a.config.exists() else None,
    'repositories': repos,
    'sdk_archive_catalog': {path.name: {'sha256': digest(path), 'bytes': path.stat().st_size}
                            for path in sorted((sdk / 'lib').glob('*.lib'))},
    'toolchain': {name: version(name) for name in ['clang', 'lld-link']},
    'compiled': True, 'runtime_validation': 'Not performed by this build command; join separate results by XBE SHA256.',
    'scope': 'Post-make source/SDK snapshot. SDK archive catalog includes unused libraries. Untracked source bytes are not archived; commit them before a release.',
}
manifest = program.parent / 'BUILD-INFO.json'
tmp = manifest.with_suffix('.json.tmp')
tmp.write_text(json.dumps(record, indent=2) + '\n')
tmp.replace(manifest)
print('Build provenance:', manifest, 'XBE', record['xbe_sha256'])
