#!/usr/bin/env python3
"""Qualification rejects incomplete, stalled, watchdog and hash-mismatched runs."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
port=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('reopen_analysis',port/'tools/analyze-audio-reopen.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def fixture(n):
    text=f'Silent SDL reopen probe cycles={n} watchdog_ms=180000\n[audio-close] configuration=valid mode=clean experimental=1\n'
    for i in range(1,n+1):
        text+=f'[audio-close] startup init_calls=1 connected_before=0 connected_after=1 connect_return=1\ncycle={i} callbacks_before=9 callbacks_after=19\n[audio-close] clean complete irq_was_connected=1 disconnected=1 dpc_removed=0 locks_released=1 codec_reset=0 analog_control=0 digital_control=0\ncycle={i} stage=close-return elapsed_ms=3 memory_status=00000000 available_pages=8000\n'
    return text+f'result={n}-open-close-cycles-progressed\n'
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);payload=b'fixture executable';sha=hashlib.sha256(payload).hexdigest()
    (root/'default.xbe').write_bytes(payload)
    for scenario in ['good','bad-watchdog','200-good','200-incomplete','compiled','init-good','init-missing','init-unexercised','truncated','stalled','watchdog','wrong-hash','duplicate','connect-failed','missing-stderr']:
        raw=fixture(200 if scenario=='200-good' else 20)
        (root/'run-01-audio-reopen-error.log').write_bytes(b'')
        info={'xbe_sha256':sha,'source_commit':'fixture','kind':'probe/audio-init-protocol' if scenario.startswith('init-') else 'probe/audio-reopen'}
        (root/'BUILD-INFO.json').write_text(json.dumps(info))
        if scenario=='bad-watchdog':raw=raw.replace('watchdog_ms=180000','watchdog_ms=0')
        if scenario=='200-incomplete':raw=raw.replace('cycles=20 ','cycles=200 ')
        if scenario=='compiled':raw=raw.replace('configuration=valid mode=clean experimental=1','configuration=compiled mode=clean experimental=0')
        if scenario in ['init-good','init-unexercised']:
            raw+='[audio-init-protocol] configuration=valid requested=preserve-ready experimental=1\n'
            for i in range(1,21):
                cold,preserved=(0,i) if scenario=='init-good' else (i,0)
                raw+=f'[audio-init-protocol] requested=preserve-ready cold_resets={cold} preserved_links={preserved} before_control=00000002 before_status=00300100 after_control=00000002 after_status=00300100\n'
        if scenario=='truncated':raw=raw[:len(raw)//2]
        if scenario=='stalled':raw=raw.replace('callbacks_after=19','callbacks_after=9',1)
        if scenario=='duplicate':raw=raw.replace('cycle=20 ','cycle=19 ')
        if scenario=='connect-failed':raw=raw.replace('connect_return=1','connect_return=0',1)
        (root/'run-01.log').write_text(raw)
        record={'xbe_sha256':sha,'log_sha256':hashlib.sha256(raw.encode()).hexdigest(),'extra_logs_present':['reopen-watchdog.log'] if scenario=='watchdog' else ['audio-reopen-error.log']}
        if scenario=='wrong-hash':record['log_sha256']='0'*64
        if scenario=='missing-stderr':(root/'run-01-audio-reopen-error.log').unlink()
        (root/'run-01.json').write_text(json.dumps(record))
        try:result=m.analyze(root)
        except ValueError:assert scenario in ['wrong-hash','bad-watchdog']
        else:
            assert result['lifecycle_pass']==(scenario in ['good','200-good','compiled','init-good']),scenario
            if scenario=='200-good':assert result['expected_cycles']==200 and not result['twenty_cycle_lifecycle_pass']
print('PASS: 20/200-cycle qualification; incomplete, stalled, duplicate, connection, mode, watchdog and hash/stderr checks')
