#!/usr/bin/env python3
"""Package an explicit hash-bound game build without inheriting old acceptance claims."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-dir', type=Path, required=True, help='Preserved XBE, BUILD-INFO.json and provenance directory')
p.add_argument('--output', type=Path, required=True, help='New package directory; never overwrites an earlier package')
p.add_argument('--evidence', type=Path, action='append', default=[], help='Exact-XBE JSON results to include verbatim, without inferring acceptance')
p.add_argument('--without-data', action='store_true', help='Build an executable/provenance archive; users obtain pinned game data separately')
a = p.parse_args()
modern = Path(__file__).resolve().parents[3]
build = a.build_dir.resolve()
out = a.output.resolve()
archive = out.parent / (out.name + '.zip')
if out.exists() or archive.exists():
    p.error('Choose a new output name; package or archive already exists')
if not out.name.isascii() or len(out.name) > 42:
    p.error('Package directory must have an ASCII name of at most 42 characters')
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
info = json.loads((build / 'BUILD-INFO.json').read_text())
if info.get('kind') != 'game' or digest(build / 'default.xbe') != info.get('xbe_sha256'):
    p.error('Build manifest must describe this exact game executable')
repositories = info['repositories']
data_info = repositories['repos/modern/data']
data = modern / 'data'
def git(*args):
    return subprocess.check_output(['git', '-C', str(data), *args], text=True).strip()
if git('rev-parse', 'HEAD') != data_info['revision'] or git('status', '--porcelain') or data_info['tracked_status'] or data_info.get('untracked_source_sha256'):
    p.error('Data must match the clean revision recorded by this build')
provenance = []
for entry in repositories.values():
    rel = Path(entry['diff_file'])
    if rel.is_absolute() or '..' in rel.parts or rel.parts[0] != 'provenance':
        p.error('Invalid provenance path')
    source = build / rel
    if digest(source) != entry['diff_sha256']:
        p.error('Archived source diff does not match its recorded hash')
    provenance.append((source, rel))
evidence = []
for source in a.evidence:
    result = json.loads(source.read_text())
    if result.get('xbe_sha256') != info['xbe_sha256']:
        p.error('Every evidence file must explicitly identify this exact XBE hash')
    evidence.append(source)
assets = []
omitted = []
for source in ([] if a.without_data else sorted(data.rglob('*'))):
    rel = source.relative_to(data)
    if any(part.startswith('.') for part in rel.parts) or not source.is_file():
        continue
    if rel.parts[0] == 'worlds' and (rel.parts[1:2] == ('Contest',) or rel.name.startswith('Contest_')):
        omitted.append(rel.as_posix())
        continue
    assets.append((source, Path('data') / rel))
# Validate before creating the destination or copying anything.
seen = set()
for _, rel in assets + provenance:
    if any(not part.isascii() or len(part) > 42 or any(c in part for c in '\\/:*?"<>|') for part in rel.parts):
        p.error('FATX-incompatible package path: ' + str(rel))
    if len('E:\\Games\\' + out.name + '\\' + str(rel).replace('/', '\\')) >= 260:
        p.error('Package path exceeds the tested title-path bound: ' + str(rel))
    if rel.as_posix().lower() in seen:
        p.error('Case-insensitive path collision: ' + str(rel))
    seen.add(rel.as_posix().lower())
out.mkdir(parents=True)
for source, rel in [(build/'default.xbe', Path('default.xbe')), (build/'BUILD-INFO.json', Path('BUILD-INFO.json')), *assets, *provenance]:
    target = out / rel
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
for i, source in enumerate(evidence):
    target = out / 'evidence' / f'result-{i:03d}.json'
    target.parent.mkdir(exist_ok=True)
    shutil.copyfile(source, target)
(out / 'PACKAGE-INFO.json').write_text(json.dumps({
    'format_version': 1, 'xbe_sha256': info['xbe_sha256'],
    'packager_sha256': digest(Path(__file__)),
    'source_commit': info['source_commit'], 'data_revision': data_info['revision'],
    'status': 'experimental candidate; full release acceptance remains open',
    'acceptance_inferred': False, 'evidence_files': len(evidence),
    'assets_included': not a.without_data,
    'external_data_url': 'https://github.com/mmatyas/supermariowar-data',
    'omitted_assets': omitted, 'omission_reason': 'Optional Contest world package contains a FATX-incompatible filename',
}, indent=2) + '\n')
(out / 'README.txt').write_text('''SUPER MARIO WAR / NXDK EXPERIMENTAL CANDIDATE

Copy this entire folder to a new writable directory on your homebrew Xbox HDD.
Keep data beside default.xbe, then launch default.xbe from the dashboard.
D-pad moves/navigates; A selects/jumps; B cancels; X runs/grabs;
Y uses a stored item; Start pauses. Hold Back+Start for two seconds to reboot.

BUILD-INFO.json describes this exact executable, compiler, build arguments,
SDK/library revisions and archived source diffs. PACKAGE-INFO.json records the
asset revision and omissions. SHA256SUMS.txt covers every packaged file.
Any supplied evidence is copied verbatim; inclusion is not full acceptance.
No previous executable's hardware results are transferred to this build.

The port targets a 64 MB application budget and four local players. Physical
stock-64-MB hardware and four physical controllers still require qualification.
Audio startup qualification depends on the exact build and included evidence.
Cold boots and other firmware need coverage. Speaker quality needs listening.
Networking is disabled. Preferences are enabled only when the recorded build
arguments/configuration enable them; then smw-pref0.bin and smw-pref1.bin are
written beside the executable. Preserve both files when upgrading this title.
Do not copy them between test versions without checking supported save schemas.

Logs are smw.log and smw-error.log beside the executable. Optional Contest worlds
are omitted due to an incompatible asset filename. This package is a candidate,
not a declaration that all modes, audio, saves or hardware combinations passed.
''')
if a.without_data:
    readme = out / 'README.txt'
    text = readme.read_text()
    text = text.replace('Copy this entire folder to a new writable directory on your homebrew Xbox HDD.\nKeep data beside default.xbe, then launch default.xbe from the dashboard.',
        'This archive contains no game data and will not boot by itself.\nObtain the external data revision below and place its contents in a data folder\nbeside default.xbe. Copy the complete local install to a writable Xbox HDD folder.\nThen launch default.xbe from the dashboard.')
    text += ('\nEXTERNAL DATA (NOT INCLUDED)\n'
        'Repository: https://github.com/mmatyas/supermariowar-data\n'
        'Revision: ' + data_info['revision'] + '\n'
        'Keep upstream folder layout. No media is downloaded by this packager.\n'
        'When preparing an Xbox install, omit the optional worlds/Contest folder\n'
        'and worlds/Contest_* entries because of FATX-incompatible names.\n')
    readme.write_text(text)
files = sorted(path for path in out.rglob('*') if path.is_file())
(out / 'SHA256SUMS.txt').write_text(''.join(digest(path)+'  '+path.relative_to(out).as_posix()+'\n' for path in files))
# Fixed ZIP timestamps/permissions make identical explicit inputs produce the
# same archive bytes, regardless of local checkout file modification times.
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as zipped:
    for path in sorted(path for path in out.rglob('*') if path.is_file()):
        entry = zipfile.ZipInfo((Path(out.name)/path.relative_to(out)).as_posix(), (1980,1,1,0,0,0))
        entry.create_system = 3
        entry.external_attr = 0o100644 << 16
        entry.compress_type = zipfile.ZIP_DEFLATED
        zipped.writestr(entry, path.read_bytes(), compresslevel=6)
print(json.dumps({'package': str(out), 'archive': str(archive), 'archive_sha256': digest(archive),
    'xbe_sha256': info['xbe_sha256'], 'files': len(files)+1, 'acceptance_inferred': False}, indent=2))
