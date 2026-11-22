#!/usr/bin/env python3
"""Build and locally assemble Super Mario War for original Xbox."""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def revision(repo, ref="HEAD"):
    return git(repo, "rev-parse", "--verify", ref).decode().strip()


def clone_pinned_source(destination, lock):
    base = lock["upstream"]["revision"]
    expected_data = lock["data"]["revision"]
    subprocess.run(
        ["git", "clone", "--filter=blob:none", "--no-checkout", lock["upstream"]["url"], str(destination)],
        check=True,
    )
    subprocess.run(["git", "-C", str(destination), "checkout", "--detach", base], check=True)
    subprocess.run(
        ["git", "-C", str(destination), "submodule", "update", "--init", "--depth", "1", "--", "data"],
        check=True,
    )
    if revision(destination) != base or revision(destination / "data") != expected_data:
        raise RuntimeError("Downloaded source or data did not resolve to the locked revision")


def collect_assets(upstream, lock, parser):
    base = lock["upstream"]["revision"]
    expected_data = lock["data"]["revision"]
    if not (upstream / ".git").exists():
        parser.error("Supply a Git clone of the locked upstream source, not a ZIP or data-only folder.")
    if revision(upstream, base + "^{commit}") != base:
        parser.error("The upstream clone must contain the pinned SDL2 commit " + base)
    data = upstream / "data"
    if not (data / ".git").exists() or revision(data) != expected_data:
        parser.error("The data submodule must be initialized at the locked commit " + expected_data)
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
    return assets


def stage_game(destination, xbe, assets):
    if destination.exists():
        raise RuntimeError("Output already exists; move or remove it first: " + str(destination))
    destination.parent.mkdir(parents=True, exist_ok=True)
    pending = Path(tempfile.mkdtemp(prefix=destination.name + ".staging-", dir=destination.parent))
    try:
        for source, rel in assets:
            target = pending / "data" / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        shutil.copyfile(xbe, pending / "default.xbe")
        pending.rename(destination)
    except Exception:
        shutil.rmtree(pending, ignore_errors=True)
        raise


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog="With no upstream argument, the exact locked game source and data are downloaded into an external temporary workspace. Nothing is added to this repository.",
    )
    parser.add_argument("upstream", nargs="?", type=Path, help="Optional local upstream Git checkout with locked data initialized")
    parser.add_argument("workspace", nargs="?", type=Path, help="Legacy external build workspace; requires upstream")
    parser.add_argument("--destination", type=Path, help="Final local Xbox folder (default: SuperMarioWar beside the script)")
    parser.add_argument("--kit", type=Path, default=Path(__file__).resolve().parent, help="Local source-only port kit checkout")
    parser.add_argument("--sdk-cache", type=Path, help="Optional local nxdk checkout with its submodules")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    kit = args.kit.expanduser().resolve()
    if sys.platform == "win32":
        parser.error("On Windows, double-click 'Build Super Mario War.cmd'; it installs the native toolchain automatically and does not use WSL.")
    if not 1 <= args.jobs <= 64:
        parser.error("--jobs must be 1..64.")
    if args.workspace and not args.upstream:
        parser.error("The legacy workspace argument requires an upstream checkout.")

    temporary = None
    try:
        lock = json.loads((kit / "upstream-lock.json").read_text())
        if args.workspace:
            work = args.workspace.expanduser().resolve()
            upstream = args.upstream.expanduser().resolve()
            destination = args.destination.expanduser().resolve() if args.destination else work / "SuperMarioWar"
            if work.exists() or any(c.isspace() for c in str(work)) or any(base == work or base in work.parents for base in (kit, upstream)):
                parser.error("Choose a new workspace outside both repositories, with no whitespace in its path.")
        else:
            temporary = tempfile.TemporaryDirectory(prefix="smw-xbox-build-")
            session = Path(temporary.name).resolve()
            work = session / "work"
            upstream = args.upstream.expanduser().resolve() if args.upstream else session / "source"
            destination = (args.destination or (kit / "SuperMarioWar")).expanduser().resolve()
            if not args.upstream:
                print("Downloading exact locked game source and data into a temporary workspace...")
                clone_pinned_source(upstream, lock)

        assets = collect_assets(upstream, lock, parser)
        prepare = [sys.executable, str(kit / "tools/prepare.py"), "--workspace", str(work), "--source-cache", str(upstream)]
        if args.sdk_cache:
            prepare.extend(["--sdk-cache", str(args.sdk_cache.expanduser().resolve())])
        subprocess.run(prepare, check=True)
        subprocess.run([sys.executable, str(kit / "tools/build.py"), "--workspace", str(work), "--clean", "--jobs", str(args.jobs)], check=True)
        xbe = work / "repos" / "modern" / "ports" / "nxdk" / "bin" / "default.xbe"
        stage_game(destination, xbe, assets)
        print(f"\nXbox folder ready: {destination}\nCopy it to a writable folder on your Xbox HDD and launch default.xbe. Do not redistribute the assembled folder.")
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Build or validation failed: {error}\n")
    finally:
        if temporary:
            temporary.cleanup()


if __name__ == "__main__":
    main()
