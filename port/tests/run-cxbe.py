#!/usr/bin/env python3
"""Exercise the patched host converter against an existing game PE, without hardware."""
import hashlib, json, os, struct, subprocess, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[3]
tool = root.parent / 'nxdk/tools/cxbe/cxbe'
pe = root / 'ports/nxdk/main.exe'
def sha(b): return hashlib.sha256(b).hexdigest()
def stamps(b):
    base, = struct.unpack_from('<I', b, 0x104)
    cert, = struct.unpack_from('<I', b, 0x118)
    return [struct.unpack_from('<I', b, 0x114)[0], struct.unpack_from('<I', b, cert-base+4)[0]]
results = []
with tempfile.TemporaryDirectory() as tmp:
    target = Path(tmp) / 'test.xbe'
    def convert(value):
        env = dict(os.environ, SOURCE_DATE_EPOCH=value)
        return subprocess.run([str(tool), '-OUT:'+str(target), '-TITLE:Super Mario War nxdk test', str(pe)], env=env, capture_output=True)
    for epoch in ['0', '1700000000', '0001700000000', '4294967295']:
        assert convert(epoch).returncode == 0
        first = target.read_bytes()
        assert stamps(first) == [int(epoch)]*2
        assert convert(epoch).returncode == 0
        assert target.read_bytes() == first
        results.append({'epoch': epoch, 'sha256': sha(first), 'identical_reconversion': True})
    original = target.read_bytes()
    for invalid in ['', '-1', '+1', ' 1', '1 ', '1x', '4294967296', '999999999999999999999999999']:
        result = convert(invalid)
        assert result.returncode != 0, (invalid, result.stdout, result.stderr)
        assert target.read_bytes() == original
record = {'scope': 'Host converter only; identical existing PE input. No full-build or runtime qualification.', 'pe_sha256': sha(pe.read_bytes()), 'cxbe_sha256': sha(tool.read_bytes()), 'valid_cases': results, 'invalid_cases_rejected_without_overwrite': 8}
output = root / 'ports/nxdk/test-results/cxbe-source-date-epoch.json'
output.write_text(json.dumps(record, indent=2)+'\n')
print(json.dumps(record, indent=2))
