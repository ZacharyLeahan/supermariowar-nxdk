#!/bin/sh
set -eu

KIT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
DESTINATION=${1:-"$HOME/Desktop/SuperMarioWar-Xbox"}

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required for the first build. Install it from https://brew.sh and run this file again." >&2
  exit 1
fi

echo "Installing or checking the open-source Xbox build prerequisites..."
HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_ANALYTICS=1 HOMEBREW_NO_INSTALL_CLEANUP=1 \
  brew install git python lld llvm coreutils pkgconf cmake bison flex

export PATH="$(brew --prefix llvm)/bin:$(brew --prefix bison)/bin:$(brew --prefix flex)/bin:$PATH"
JOBS=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
cd "$KIT"
exec python3 build-xbox.py --destination "$DESTINATION" --jobs "$JOBS"
