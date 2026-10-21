"""Structural preflight for native save fixtures; not game semantic acceptance."""
import struct
import zlib

def validate_preference_record(raw):
    # Structural preflight only. Actual payload semantics must be checked by
    # the exact game build; this prevents unsafe offsets before any mutation.
    def reject():
        raise ValueError('Requires bounded, CRC-valid schema-3..8 preference records')
    if (len(raw) < 36 or len(raw) > 24 + 16384 or raw[:8] != b'SMWNXCFG' or
        struct.unpack_from('<I', raw, 8)[0] != 1 or
        struct.unpack_from('<I', raw, 16)[0] != len(raw) - 24 or
        zlib.crc32(raw[:20] + raw[24:]) != struct.unpack_from('<I', raw, 20)[0]):
        reject()
    schema = struct.unpack_from('<I', raw, 24)[0]
    count = {3: 499, 4: 604, 5: 626, 6: 630, 7: 774, 8: 774}.get(schema)
    if count is None or struct.unpack_from('<I', raw, 32)[0] != count:
        reject()
    at = 24 + 12 + count * 4
    if at > len(raw): reject()
    if schema >= 6:
        if len(raw) - at < 4 or struct.unpack_from('<I', raw, at)[0] != 11: reject()
        at += 4
        for _ in range(11):
            if len(raw) - at < 4: reject()
            length = struct.unpack_from('<I', raw, at)[0]; at += 4
            if not 1 <= length <= 255 or length > len(raw) - at: reject()
            name = raw[at:at + length]; at += length
            if name in (b'.', b'..') or any(c < 32 or c == 127 or c in b'/\\:*?"<>|' for c in name): reject()
    if at != len(raw): reject()
    return struct.unpack_from('<I', raw, 12)[0]


def validate_fixture_structure(raw, kind):
    """Semantic fixtures stay structurally valid; the game must reject their values."""
    if kind in ('invalid-choice', 'invalid-controller'):
        validate_preference_record(raw)
        return
    if kind not in ('bad-crc', 'both-bad-crc', 'truncated', 'unsupported-schema'):
        raise ValueError('Unknown fixture kind')
    try:
        validate_preference_record(raw)
    except ValueError:
        return
    raise ValueError('Structural fixture unexpectedly valid; no files changed')
