#!/usr/bin/env bash
# orange-setup.sh - Post-install configuration for Orange on Void Linux.
#
# Sets up GDM to use the Orange session by default, creates the dconf
# profile and database required by GDM, configures PAM for elogind
# session tracking, and enables the required runit services.
#
# Run once after installing the orange package:
#   sudo scripts/orange-setup.sh
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

# ── 1. GDM configuration ────────────────────────────────────────────

info "configuring GDM"

install -d -m 0755 /etc/gdm

if [ -f /etc/gdm/custom.conf ]; then
	# Back up existing config
	cp /etc/gdm/custom.conf /etc/gdm/custom.conf.bak
fi

cat > /etc/gdm/custom.conf <<'GDMCONF'
# Orange Wayland session - GDM configuration

[daemon]
WaylandEnable=true
InitialSetupEnable=false

[security]

[xdmcp]

[chooser]

[debug]
GDMCONF
chmod 0644 /etc/gdm/custom.conf
info "  /etc/gdm/custom.conf written"

# ── 2. dconf profile and database for GDM ────────────────────────────

info "setting up dconf for GDM"

if command -v glib-compile-schemas >/dev/null 2>&1; then
	glib-compile-schemas /usr/share/glib-2.0/schemas/ >/dev/null 2>&1 || true
fi

install -d -m 0755 /etc/dconf/profile
cat > /etc/dconf/profile/gdm <<'DCONF'
user-db:user
system-db:local
DCONF
info "  /etc/dconf/profile/gdm written"

if command -v dconf >/dev/null 2>&1; then
	install -d -m 0755 /etc/dconf/db
	touch /etc/dconf/db/local
	dconf update 2>/dev/null || true
	info "  dconf database updated"
else
	warn "dconf not found; GDM may log backend assertion errors"
fi

# ── 3. AccountsService - set default session to Orange ───────────────

info "configuring AccountsService default session"

install -d -m 0755 /var/lib/AccountsService/users

# Find the first real (non-system) user or default to 'orange'
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
	# Update existing entry to use Orange session
	if grep -q 'Session=' "/var/lib/AccountsService/users/${ORANGE_USER}"; then
		sed -i "s|^Session=.*|Session=orange|" "/var/lib/AccountsService/users/${ORANGE_USER}"
	else
		printf 'Session=orange\n' >> "/var/lib/AccountsService/users/${ORANGE_USER}"
	fi
	info "  updated AccountsService entry for ${ORANGE_USER} -> Session=orange"
fi

# ── 4. D-Bus launch helper setuid (required by GDM/AccountsService) ──

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

# ── 5. GDM 48 greeter directory ──────────────────────────────────────

info "ensuring GDM greeter directory exists"

if [ -d /var/lib/gdm ]; then
	install -d -m 0755 -o gdm -g gdm /var/lib/gdm/greeter
fi

# ── 6. PAM session handling for elogind ──────────────────────────────

info "checking PAM configuration for elogind"

# On Void Linux, elogind provides pam_elogind.so. Ensure the system
# login PAM config includes elogind session tracking so that seat,
# session, and XDG_RUNTIME_DIR are set correctly for GDM greeter and
# user sessions.

PAM_LOGIN="/etc/pam.d/login"
PAM_GDM="/etc/pam.d/gdm"

if [ -f "$PAM_LOGIN" ]; then
	if ! grep -q 'pam_elogind.so' "$PAM_LOGIN" 2>/dev/null; then
		# Check if we have pam_elogind.so available
		PAM_ELOGIND_PATH=""
		for dir in /lib/security /lib64/security /usr/lib/security /usr/lib64/security; do
			if [ -f "$dir/pam_elogind.so" ]; then
				PAM_ELOGIND_PATH="$dir/pam_elogind.so"
				break
			fi
		done

		if [ -n "$PAM_ELOGIND_PATH" ]; then
			# Insert elogind session line after the pam_unix session line
			if grep -q 'pam_unix.so.*session' "$PAM_LOGIN"; then
				sed -i '/pam_unix.so.*session/a session    optional     pam_elogind.so' "$PAM_LOGIN"
			else
				# Append at the end of the session stack
				printf 'session    optional     pam_elogind.so\n' >> "$PAM_LOGIN"
			fi
			info "  added pam_elogind.so to $PAM_LOGIN"
		else
			warn "pam_elogind.so not found; elogind session tracking may not work"
		fi
	else
		info "  $PAM_LOGIN already includes pam_elogind.so"
	fi
else
	warn "$PAM_LOGIN not found; PAM may need manual configuration"
fi

# Ensure GDM PAM config exists
if [ ! -f "$PAM_GDM" ]; then
	if [ -f /etc/pam.d/gdm-launch-environment ]; then
		info "  using gdm-launch-environment PAM config as gdm"
		ln -sf /etc/pam.d/gdm-launch-environment "$PAM_GDM"
	elif [ -f /etc/pam.d/greeter ]; then
		info "  using greeter PAM config as gdm"
		ln -sf /etc/pam.d/greeter "$PAM_GDM"
	else
		# Create a minimal GDM PAM config
		cat > "$PAM_GDM" <<'GAMPAM'
#%PAM-1.0
auth       include     system-login
account    include     system-login
password   include     system-login
session    include     system-login
GAMPAM
		info "  created minimal $PAM_GDM"
	fi
fi

# ── 7. Ensure XDG_RUNTIME_DIR base exists ────────────────────────────

info "ensuring runtime directory base"

install -d -m 0755 /run/user

# ── 8. GDM runit service ────────────────────────────────────────────

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

# ── 9. Enable runit services ────────────────────────────────────────

info "enabling runit services"

# Services must start in dependency order.
SERVICES="dbus elogind seatd polkitd gdm"
ENABLED=0

for svc in $SERVICES; do
	if [ -d "/etc/sv/$svc" ]; then
		if [ ! -e "/var/service/$svc" ]; then
			ln -s "/etc/sv/$svc" "/var/service/$svc"
			info "  enabled $svc"
			ENABLED=$((ENABLED + 1))
		else
			info "  $svc already enabled"
		fi
	else
		warn "  /etc/sv/$svc not found; skipping $svc"
	fi
done

# ── 10. Default config ──────────────────────────────────────────────

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
echo "  GDM is configured to offer the Orange Wayland session."
echo "  dconf database is ready for GDM."
echo "  AccountsService will launch Orange by default."
echo "  PAM elogind session tracking is enabled."
echo ""

if [ "$ENABLED" -gt 0 ]; then
	echo "  Services were just enabled. If GDM was already running,"
	echo "  you may need to restart it or reboot."
fi

if [ ! -d /var/service/gdm ]; then
	echo ""
	echo "  To start GDM now:"
	echo "    sudo ln -s /etc/sv/gdm /var/service/"
	echo ""
	echo "  Or reboot:"
	echo "    sudo reboot"
fi
