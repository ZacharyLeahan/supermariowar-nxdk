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
port="$stage/repos/modern/ports/nxdk"
record_args=("$@")
mkdir -p "$root/logs"
if [ "${1:-}" = clean-build ]; then
    shift
    make -C "$port" CURDIR="$port" -j"${JOBS:-4}" clean "$@" 2>&1 | tee "$root/logs/smw-clean.log"
fi
make -C "$port" CURDIR="$port" -j"${JOBS:-4}" "$@" 2>&1 | tee "$root/logs/smw-build.log"

python3 "$modern/ports/nxdk/tools/record-build.py" --program "$port/bin/default.xbe" \
    --kind game --config "$port/.build-config" -- "${record_args[@]}"
