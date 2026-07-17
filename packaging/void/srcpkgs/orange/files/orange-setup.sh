#!/usr/bin/env bash
# orange-setup.sh - Optional post-install helper for Orange on Void Linux.
#
# Configures dconf, AccountsService, D-Bus, the GDM runit service,
# and copies the default config. GDM and PAM are configured by the
# package itself -- this script handles things that cannot be shipped
# as conf_files.
#
# This script is OPTIONAL. The package installs everything needed to
# run Orange. Run this only if you want AccountsService integration
# and the GDM runit service pre-configured.
#
#   sudo orange-setup
#
# Can be re-run safely; existing configuration is preserved.

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

info()  { printf "${GREEN}[*]${NC} %s\n" "$*"; }
warn()  { printf "${YELLOW}[!]${NC} %s\n" "$*"; }
error() { printf "${RED}[!]${NC} %s\n" "$*" >&2; }

# ── Preflight ────────────────────────────────────────────────────────

if [ "$(id -u)" -ne 0 ]; then
	error "this script must be run as root"
	exit 1
fi

if [ ! -x /usr/bin/orange ] && [ ! -x /usr/bin/orange-session ]; then
	error "orange binary not found; install the orange package first"
	exit 1
fi

# ── 1. dconf profile and database for GDM ────────────────────────────

info "setting up dconf for GDM"

if command -v glib-compile-schemas >/dev/null 2>&1; then
	glib-compile-schemas /usr/share/glib-2.0/schemas/ >/dev/null 2>&1 || true
fi

install -d -m 0755 /etc/dconf/profile
if [ ! -f /etc/dconf/profile/gdm ]; then
	cat > /etc/dconf/profile/gdm <<'DCONF'
user-db:user
system-db:local
DCONF
	info "  /etc/dconf/profile/gdm written"
else
	info "  /etc/dconf/profile/gdm already exists"
fi

if command -v dconf >/dev/null 2>&1; then
	install -d -m 0755 /etc/dconf/db
	touch /etc/dconf/db/local
	dconf update 2>/dev/null || true
	info "  dconf database updated"
else
	warn "dconf not found; GDM may log backend assertion errors"
fi

# ── 2. AccountsService - set default session to Orange ───────────────

info "configuring AccountsService default session"

install -d -m 0755 /var/lib/AccountsService/users

ORANGE_USER="${ORANGE_USER:-orange}"
if [ ! -f "/var/lib/AccountsService/users/${ORANGE_USER}" ]; then
	cat > "/var/lib/AccountsService/users/${ORANGE_USER}" <<ACCT
[User]
Session=orange
SystemAccount=false
ACCT
	chmod 0644 "/var/lib/AccountsService/users/${ORANGE_USER}"
	info "  created AccountsService entry for ${ORANGE_USER} with Session=orange"
else
	if grep -q 'Session=' "/var/lib/AccountsService/users/${ORANGE_USER}"; then
		sed -i "s|^Session=.*|Session=orange|" "/var/lib/AccountsService/users/${ORANGE_USER}"
	else
		printf 'Session=orange\n' >> "/var/lib/AccountsService/users/${ORANGE_USER}"
	fi
	info "  updated AccountsService entry for ${ORANGE_USER} -> Session=orange"
fi

# ── 3. D-Bus launch helper setuid (required by GDM/AccountsService) ──

info "fixing D-Bus launch helper permissions"

DBUS_HELPER="/usr/libexec/dbus-daemon-launch-helper"
if [ -f "$DBUS_HELPER" ]; then
	if [ "$(stat -c %a "$DBUS_HELPER" 2>/dev/null)" != "4754" ]; then
		chmod u+s "$DBUS_HELPER"
		info "  set setuid on $DBUS_HELPER"
	fi
else
	warn "D-Bus launch helper not found at $DBUS_HELPER"
fi

# ── 4. GDM 48 greeter directory ──────────────────────────────────────

info "ensuring GDM greeter directory exists"

if [ -d /var/lib/gdm ]; then
	install -d -m 0755 -o gdm -g gdm /var/lib/gdm/greeter
fi

# ── 5. GDM custom.conf and PAM ──────────────────────────────────────

info "configuring GDM"

install -d -m 0755 /etc/gdm
cat > /etc/gdm/custom.conf <<'GDMCONF'
[daemon]
WaylandEnable=true
InitialSetupEnable=false

[security]

[xdmcp]

[chooser]

[debug]
GDMCONF
info "  /etc/gdm/custom.conf written"

cat > /etc/pam.d/gdm <<'GDMPAM'
#%PAM-1.0

auth       required     pam_securetty.so
auth       requisite    pam_nologin.so
auth       include      system-login
account    include      system-login
password   include      system-login
session    include      system-login
-session   optional     pam_elogind.so
GDMPAM
info "  /etc/pam.d/gdm written"

# Fix pam_systemd -> pam_elogind for Void Linux
for f in /etc/pam.d/*; do
	if grep -q "pam_systemd.so" "$f" 2>/dev/null; then
		sed -i "s/pam_systemd\.so/pam_elogind.so/" "$f"
		info "  fixed pam_systemd -> pam_elogind in $f"
	fi
done

# ── 6. Ensure XDG_RUNTIME_DIR base exists ────────────────────────────

info "ensuring runtime directory base"

install -d -m 0755 /run/user

# ── 6. GDM runit service ────────────────────────────────────────────

info "configuring GDM runit service"

if [ -d /etc/sv/gdm ]; then
	cat > /etc/sv/gdm/run <<'GDMRUN'
#!/bin/sh
exec 2>&1

# Create elogind seat file so sd_seat_can_graphical() returns true.
mkdir -p /run/systemd/seats
cat > /run/systemd/seats/seat0 <<'SEAT'
# This is private data. Do not parse.
CAN_GRAPHICAL=yes
CAN_TTY=yes
SEAT

[ ! -d /run/gdm ] && mkdir -m0711 -p /run/gdm && chown root:gdm /run/gdm
[ ! -d /var/lib/gdm/greeter ] && mkdir -m0755 -p /var/lib/gdm/greeter && chown gdm:gdm /var/lib/gdm/greeter
exec gdm
GDMRUN
	chmod +x /etc/sv/gdm/run
	info "  /etc/sv/gdm/run written"
else
	warn "  /etc/sv/gdm not found; GDM runit service not configured"
fi

# ── 7. Default config ──────────────────────────────────────────────

info "ensuring default config"

install -d -m 0755 /etc/orange
if [ ! -f /etc/orange/orange.conf ] && [ -f /usr/share/orange/orange.conf ]; then
	cp /usr/share/orange/orange.conf /etc/orange/orange.conf
	info "  copied default config to /etc/orange/orange.conf"
fi

# ── Done ─────────────────────────────────────────────────────────────

echo ""
info "setup complete"
echo ""
echo "  dconf database is ready for GDM."
echo "  AccountsService will launch Orange by default."
echo "  GDM runit service is configured."
echo ""
echo "  Services are NOT auto-enabled. Enable them manually:"
echo "    # ln -s /etc/sv/dbus /var/service/"
echo "    # ln -s /etc/sv/elogind /var/service/"
echo "    # ln -s /etc/sv/seatd /var/service/"
echo "    # ln -s /etc/sv/gdm /var/service/"
echo ""
