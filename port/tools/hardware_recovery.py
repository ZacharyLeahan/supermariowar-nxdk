"""Protect unretrieved failure evidence from later hardware-runner log clearing."""
import json
from pathlib import Path

def recovery_path(modern):
    return Path(modern).parents[1] / 'logs/hardware/RECOVERY-REQUIRED.json'

def require_recovery_archived(modern, host, directory):
    path = recovery_path(modern)
    if not path.exists(): return
    record = json.loads(path.read_text())
    same_title = record['host'] == host and (directory is None or record['directory'].rstrip('/').lower() == directory.rstrip('/').lower())
    if same_title and record.get('requires_log_preservation', True):
        raise ValueError('Failed-startup logs must be archived before another launch. After the Xbox returns to XBMC, run recover-game-logs.py; see '+str(path))
