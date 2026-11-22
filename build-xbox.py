#!/usr/bin/env python3
"""Compile and stage Super Mario War for original Xbox, locally only."""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def revision(repo, ref="HEAD"):
    return git(repo, "rev-parse", "--verify", ref).decode().strip()


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog="Clone https://github.com/mmatyas/supermariowar.git separately, then check out the pinned revision and initialize its data submodule (see the release instructions).",
    )
    parser.add_argument("upstream", type=Path, help="Local upstream Git checkout with its data submodule initialized")
    parser.add_argument("workspace", type=Path, help="New external directory without spaces (use the WSL Linux filesystem on Windows)")
    parser.add_argument("--kit", type=Path, default=Path(__file__).resolve().parent, help="Local source-only port kit checkout")
    parser.add_argument("--sdk-cache", type=Path, help="Optional local nxdk checkout with its submodules")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    upstream, work, kit = (p.expanduser().resolve() for p in (args.upstream, args.workspace, args.kit))
    if sys.platform == "win32":
        parser.error("Run from a Linux shell under WSL, not Windows Python.")
    if work.exists() or any(c.isspace() for c in str(work)) or any(base == work or base in work.parents for base in (kit, upstream)):
        parser.error("Choose a new directory outside both repositories, with no whitespace in its path.")
    if not 1 <= args.jobs <= 64:
        parser.error("--jobs must be 1..64.")

    try:
        lock = json.loads((kit / "upstream-lock.json").read_text())
        base, expected_data = lock["upstream"]["revision"], lock["data"]["revision"]
        if not (upstream / ".git").exists():
            parser.error("Supply a local Git clone of https://github.com/mmatyas/supermariowar.git, not a ZIP or a data-only folder.")
        if revision(upstream, base + "^{commit}") != base:
            parser.error("The upstream clone must contain the pinned SDL2 commit " + base)
        data = upstream / "data"
        if not (data / ".git").exists() or revision(data) != expected_data:
            parser.error(
                "Initialize the pinned data submodule in your own upstream clone. "
                f"In a fresh clone: git checkout --detach {base}; "
                f"git submodule update --init -- data (expected {expected_data})."
            )
        if git(data, "status", "--porcelain"):
            parser.error("The data checkout must be clean; the script does not change it.")
        assets, seen = [], set()
        for name in git(data, "ls-files", "-z").decode().split("\0"):
            if not name:
                continue
            rel = Path(name)
            if any(part.startswith(".") for part in rel.parts):
                continue
            if rel.parts[0] == "worlds" and (rel.parts[1:2] == ("Contest",) or rel.name.startswith("Contest_")):
                continue
            source = data / rel
            if rel.is_absolute() or source.is_symlink() or not source.is_file() or not source.resolve().is_relative_to(data.resolve()):
                parser.error("Missing or unsupported asset: " + name)
            if (any(not p.isascii() or len(p) > 42 or re.search(r'[\\/:*?"<>|\x00-\x1f]', p) for p in rel.parts)
                    or len("E:/Games/SuperMarioWar/data/" + name) >= 260 or name.lower() in seen):
                parser.error("FATX-incompatible asset path: " + name)
            seen.add(name.lower())
            assets.append((source, rel))
        if not assets:
            parser.error("No tracked runtime data found.")

        prepare = [sys.executable, str(kit / "tools/prepare.py"), "--workspace", str(work), "--source-cache", str(upstream)]
        if args.sdk_cache:
            prepare.extend(["--sdk-cache", str(args.sdk_cache.expanduser().resolve())])
        subprocess.run(prepare, check=True)
        subprocess.run([sys.executable, str(kit / "tools/build.py"), "--workspace", str(work), "--clean", "--jobs", str(args.jobs)], check=True)
        bin_dir = work / "repos/modern/ports/nxdk/bin"
        pending = work / "staging"
        pending.mkdir()
        for source, rel in assets:
            target = pending / "data" / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        shutil.copyfile(bin_dir / "default.xbe", pending / "default.xbe")
        output = work / "SuperMarioWar"
        pending.rename(output)
        print(f"Xbox folder: {output}\nCopy it to a writable folder on your Xbox HDD and launch default.xbe. Do not redistribute the assembled folder.")
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Build or validation failed: {error}\n")


if __name__ == "__main__":
    main()
