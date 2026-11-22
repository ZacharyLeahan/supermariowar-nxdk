#!/usr/bin/env python3
"""Host-test the asset-safe staging path without building or downloading anything."""

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from unittest import mock


root = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("build_xbox", root / "build-xbox.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
lock = json.loads((root / "upstream-lock.json").read_text())


with tempfile.TemporaryDirectory(prefix="smw-build-xbox-test-") as tmp:
    base = Path(tmp)
    upstream = base / "upstream"
    data = upstream / "data"
    (upstream / ".git").mkdir(parents=True)
    (data / ".git").mkdir(parents=True)
    asset = data / "gfx" / "fixture.txt"
    asset.parent.mkdir()
    asset.write_text("synthetic text fixture\n")
    excluded = data / "worlds" / "Contest" / "fixture.txt"
    excluded.parent.mkdir(parents=True)
    excluded.write_text("synthetic excluded fixture\n")
    workspace = base / "workspace"

    def fake_revision(repo, ref="HEAD"):
        return lock["data"]["revision"] if Path(repo) == data else lock["upstream"]["revision"]

    def fake_git(repo, *args):
        if args == ("status", "--porcelain"):
            return b""
        if args == ("ls-files", "-z"):
            return b"gfx/fixture.txt\0worlds/Contest/fixture.txt\0"
        raise AssertionError((repo, args))

    def fake_run(command, check):
        if Path(command[1]).name == "build.py":
            output = workspace / "repos" / "modern" / "ports" / "nxdk" / "bin"
            output.mkdir(parents=True)
            (output / "default.xbe").write_bytes(b"XBEH synthetic fixture")
        return mock.Mock(returncode=0)

    argv = ["build-xbox.py", str(upstream), str(workspace)]
    with mock.patch.object(module.sys, "platform", "linux"), \
         mock.patch.object(module.sys, "argv", argv), \
         mock.patch.object(module, "revision", fake_revision), \
         mock.patch.object(module, "git", fake_git), \
         mock.patch.object(module.subprocess, "run", fake_run):
        module.main()

    output = workspace / "SuperMarioWar"
    assert (output / "default.xbe").read_bytes().startswith(b"XBEH")
    assert (output / "data" / "gfx" / "fixture.txt").read_bytes() == asset.read_bytes()
    assert not (output / "data" / "worlds" / "Contest").exists()
    assert upstream.exists() and asset.exists() and excluded.exists()
    print("PASS: local Xbox folder stages tracked data, omits Contest worlds, and preserves upstream")
