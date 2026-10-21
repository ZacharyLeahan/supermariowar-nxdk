#!/usr/bin/env bash
set -euo pipefail
modern=$(cd "$(dirname "$0")/../../.." && pwd)
root=$(cd "$modern/../.." && pwd)
stage=/tmp/smw-xbe-workspace
if [ -L "$stage" ]; then
    [ "$(readlink "$stage")" = "$root" ] || { echo "Workspace link belongs to another checkout"; exit 1; }
elif [ -e "$stage" ]; then
    echo "Cannot create workspace link: $stage exists"; exit 1
else
    ln -s "$root" "$stage"
fi
export NXDK_DIR="$stage/repos/nxdk"
export PATH="$NXDK_DIR/bin:/opt/homebrew/opt/llvm/bin:/usr/local/opt/llvm/bin:/opt/homebrew/opt/bison/bin:/opt/homebrew/opt/flex/bin:$PATH"
name=${1:?Usage: build-probe.sh PROBE [make arguments]}
shift
case "$name" in
    audio-startup|audio-hal-drain|audio-cost|audio-close|audio-reopen|audio-init-protocol|save-store|timer-granularity) ;;
    *) echo "Unknown probe: $name"; exit 1 ;;
esac
probe="$stage/repos/modern/ports/nxdk/probes/$name"
mkdir -p "$root/logs"
make -C "$probe" CURDIR="$probe" -j"${JOBS:-4}" "$@" 2>&1 | tee "$root/logs/$name-build.log"
python3 "$modern/ports/nxdk/tools/record-build.py" --program "$probe/bin/default.xbe" \
    --kind "probe/$name" -- "$@"
