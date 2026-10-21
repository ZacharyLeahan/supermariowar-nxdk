#!/usr/bin/env python3
"""Check native fixture preflight against saved records and malformed variants."""
import argparse
from pathlib import Path
import struct
import sys
import zlib
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from save_preflight import validate_preference_record, validate_fixture_structure
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('records', type=Path, nargs='+')
a = p.parse_args()
def rejected(raw):
    try: validate_preference_record(raw)
    except ValueError: return
    raise AssertionError('Malformed record passed fixture preflight')
def checksum(raw):
    struct.pack_into('<I', raw, 20, zlib.crc32(raw[:20]+raw[24:]))
for path in a.records:
    raw = path.read_bytes()
    assert validate_preference_record(raw) == struct.unpack_from('<I', raw, 12)[0]
    for kind,offset,value in [('invalid-choice',24+1832+25*4,21),('invalid-controller',24+2532+76,4)]:
        if kind=='invalid-controller' and struct.unpack_from('<I',raw,24)[0]<7:continue
        semantic=bytearray(raw);struct.pack_into('<I',semantic,offset,value);checksum(semantic)
        validate_fixture_structure(semantic,kind)
        try:validate_fixture_structure(semantic,'bad-crc')
        except ValueError:pass
        else:raise AssertionError('Semantic fixture mislabeled as structurally invalid')
    crc=bytearray(raw);crc[20]^=1
    validate_fixture_structure(crc,'bad-crc');validate_fixture_structure(raw[:40],'truncated')
    for length in range(len(raw)): rejected(raw[:length])
    rejected(raw+b'\0')
    for i in range(len(raw)):
        corrupt=bytearray(raw);corrupt[i]^=1;rejected(corrupt)
    for offset,value in [(8,2),(16,0xffffffff),(24,99),(32,0xffffffff)]:
        corrupt=bytearray(raw);struct.pack_into('<I',corrupt,offset,value);checksum(corrupt);rejected(corrupt)
    schema=struct.unpack_from('<I',raw,24)[0]
    if schema >= 6:
        count=struct.unpack_from('<I',raw,32)[0]
        first=24+12+count*4
        for offset,value in [(first,12),(first+4,0),(first+4,256),(first+4,0xffffffff)]:
            corrupt=bytearray(raw);struct.pack_into('<I',corrupt,offset,value);checksum(corrupt);rejected(corrupt)
        for value in [0,ord('/'),ord('\\')]:
            corrupt=bytearray(raw);corrupt[first+8]=value;checksum(corrupt);rejected(corrupt)
print('PASS: native save fixture preflight, all truncated prefixes, corruption, schema/count/length and asset-name bounds')
