#!/usr/bin/env bash
# build-orange-package.sh - Build and optionally install the orange Void package.
#
# Automates the manual steps from the Void packaging README:
#   1. Find or clone a void-packages checkout
#   2. Run xbps-src binary-bootstrap if needed
#   3. Overlay the orange srcpkgs and current source tree
#   4. Build with xbps-src
#   5. Index the local repository
#   6. Optionally install with xbps-install
#
# Usage:
#   scripts/build-orange-package.sh [--install]
#   scripts/build-orange-package.sh --void-packages-dir /path/to/void-packages
#
# Environment:
#   ORANGE_REPO             Path to orange-wlroots repo (auto-detected)
#   ORANGE_VOID_PACKAGES    Path to void-packages checkout
#   ORANGE_VOID_INSTALL     Set to 1 to install after building

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

info()  { printf "${GREEN}[*]${NC} %s\n" "$*"; }
warn()  { printf "${YELLOW}[!]${NC} %s\n" "$*"; }
die()   { printf "${RED}[!]${NC} %s\n" "$*" >&2; exit 1; }

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"
}

# ── Parse arguments ───────────────────────────────────────────────────

void_packages="${ORANGE_VOID_PACKAGES:-}"
do_install="${ORANGE_VOID_INSTALL:-}"

while [ $# -gt 0 ]; do
	case "$1" in
		--void-packages-dir)
			void_packages="$2"; shift 2 ;;
		--install)
			do_install=1; shift ;;
		*)
			die "unknown argument: $1" ;;
	esac
done

# ── Locate repos ──────────────────────────────────────────────────────

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
orange_repo="${ORANGE_REPO:-$repo_root}"

[ -d "$orange_repo/packaging/void/srcpkgs/orange" ] || \
	die "orange overlay not found at $orange_repo/packaging/void/srcpkgs/orange"

if [ -z "$void_packages" ]; then
	# Look in common locations
	for candidate in \
		"${PWD}/void-packages" \
		"${PWD}/../void-packages" \
		"${HOME}/void-packages" \
		"${HOME}/.cache/orange-void-vm/void-packages"; do
		if [ -f "$candidate/xbps-src" ]; then
			void_packages="$candidate"
			break
		fi
	done
fi

if [ -z "$void_packages" ] || [ ! -f "$void_packages/xbps-src" ]; then
	info "void-packages checkout not found; cloning..."
	need_cmd git
	void_packages="${HOME}/.cache/orange-void-vm/void-packages"
	mkdir -p "$(dirname "$void_packages")"
	if [ ! -d "$void_packages" ]; then
		git clone https://github.com/void-linux/void-packages.git "$void_packages"
	fi
fi

info "void-packages: $void_packages"
info "orange repo:   $orange_repo"

# ── Prerequisites ─────────────────────────────────────────────────────

need_cmd tar
need_cmd git

if [ ! -f "$void_packages/bin/xbps-src" ] && [ ! -f "$void_packages/xbps-src" ]; then
	die "xbps-src not found in $void_packages; expected a void-packages checkout"
fi

# ── 1. Bootstrap if needed ───────────────────────────────────────────

if [ ! -d "$void_packages/hostdir" ]; then
	info "bootstrapping void-packages..."
	(cd "$void_packages" && ./xbps-src binary-bootstrap)
else
	info "void-packages already bootstrapped"
fi

# ── 2. Overlay orange srcpkgs ────────────────────────────────────────

info "copying orange overlay..."
rm -rf "$void_packages/srcpkgs/orange"
cp -a "$orange_repo/packaging/void/srcpkgs/orange" "$void_packages/srcpkgs/orange"

# ── 3. Extract current source into files/src ──────────────────────────

info "extracting orange source tree..."
rm -rf "$void_packages/srcpkgs/orange/files/src"
mkdir -p "$void_packages/srcpkgs/orange/files/src"
tar -C "$orange_repo" \
	--exclude .git \
	--exclude build \
	--exclude 'void-packages' \
	--exclude '.cache' \
	--exclude '*.qcow2' \
	--exclude '*.vmdk' \
	--exclude '*.vdi' \
	--exclude '*.raw' \
	--exclude '*.img' \
	--exclude '*.tar.xz' \
	--exclude '*.tar.gz' \
	--exclude '*.tar.bz2' \
	--exclude '*.iso' \
	--exclude '*.o' \
	--exclude '*.a' \
	--exclude '*.so' \
	--exclude '*.so.*' \
	--exclude 'compile_commands.json' \
	--exclude '.cache' \
	-cf - . | tar -C "$void_packages/srcpkgs/orange/files/src" -xf -

info "source tree: $(find "$void_packages/srcpkgs/orange/files/src" -type f | wc -l) files"

# ── 4. Build ──────────────────────────────────────────────────────────

info "building orange package..."
(cd "$void_packages" && ./xbps-src -1 pkg orange)

# ── 5. Index local repository ────────────────────────────────────────

info "indexing local repository..."
xbps-rindex -a "$void_packages/hostdir/binpkgs/"*.xbps

info "package built: $(ls -1 "$void_packages/hostdir/binpkgs/"*orange*.xbps 2>/dev/null | head -1)"

# ── 6. Install (optional) ────────────────────────────────────────────

if [ "${do_install:-}" = "1" ]; then
	info "installing orange..."
	sudo xbps-install -S -R "$void_packages/hostdir/binpkgs" orange

	echo ""
	info "installation complete"
	echo ""
	echo "  Reboot to start GDM with Orange:"
	echo "    sudo reboot"
else
	echo ""
	info "build complete"
	echo ""
	echo "  Package: $void_packages/hostdir/binpkgs/*orange*.xbps"
	echo ""
	echo "  To install:"
	echo "    xbps-rindex -a $void_packages/hostdir/binpkgs/*.xbps"
	echo "    sudo xbps-install -S -R $void_packages/hostdir/binpkgs orange"
	echo "    sudo reboot"
	echo ""
	echo "  Or re-run with --install:"
	echo "    $0 --install"
fi
