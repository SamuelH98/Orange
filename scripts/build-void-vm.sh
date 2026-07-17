#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
workdir=${ORANGE_VOID_WORKDIR:-"${HOME}/.cache/orange-void-vm"}
void_packages=${ORANGE_VOID_PACKAGES:-"${workdir}/void-packages"}
image_raw=${ORANGE_VOID_IMAGE_RAW:-"${workdir}/orange-void.raw"}
image_override=${ORANGE_VOID_IMAGE:-}
image_format_requested=${ORANGE_VOID_IMAGE_FORMAT:-}
kernel_out=${ORANGE_VOID_KERNEL:-"${HOME}/orange-void-vmlinuz"}
initrd_out=${ORANGE_VOID_INITRD:-"${HOME}/orange-void-initramfs.img"}
mount_dir=${ORANGE_VOID_MOUNT:-"/mnt/orange-void"}
image_size=${ORANGE_VOID_IMAGE_SIZE:-"10G"}
void_repo=${ORANGE_VOID_REPO:-"https://repo-default.voidlinux.org/current"}
void_arch=${ORANGE_VOID_ARCH:-"x86_64"}
xbps_static_dir=${ORANGE_XBPS_STATIC_DIR:-"${workdir}/xbps-static"}
void_live_base=${ORANGE_VOID_LIVE_BASE:-"https://repo-default.voidlinux.org/live/current"}
rootfs_url=${ORANGE_VOID_ROOTFS_URL:-}
rootfs_tarball=${ORANGE_VOID_ROOTFS_TARBALL:-}
rootfs_sha256=${ORANGE_VOID_ROOTFS_SHA256:-}
rootfs_sha256_url=${ORANGE_VOID_ROOTFS_SHA256_URL:-"${void_live_base%/}/sha256sum.txt"}

say() {
	printf '[*] %s\n' "$*"
}

die() {
	printf '[!] %s\n' "$*" >&2
	exit 1
}

normalize_image_format() {
	local requested=${1,,}
	case "$requested" in
		qcow|qcow2)
			image_format=qcow2
			image_extension=qcow2
			;;
		vmdk)
			image_format=vmdk
			image_extension=vmdk
			;;
		vdi|vti)
			image_format=vdi
			image_extension=vdi
			;;
		*)
			die "unsupported ORANGE_VOID_IMAGE_FORMAT=$1; use qcow, qcow2, vmdk, vdi, or vti"
			;;
	esac
}

infer_image_format() {
	if [ -n "$image_format_requested" ]; then
		normalize_image_format "$image_format_requested"
		return
	fi

	case "$image_override" in
		*.vmdk)
			normalize_image_format vmdk
			;;
		*.vdi|*.vti)
			normalize_image_format vdi
			;;
		*.qcow|*.qcow2|"")
			normalize_image_format qcow2
			;;
		*)
			die "cannot infer image format from ORANGE_VOID_IMAGE=$image_override; set ORANGE_VOID_IMAGE_FORMAT"
			;;
	esac
}

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"
}

infer_image_format
image=${image_override:-"${HOME}/orange-void.${image_extension}"}

need_cmd git
need_cmd qemu-img
need_cmd sfdisk
need_cmd losetup
need_cmd mkfs.ext4
need_cmd blkid
need_cmd tar
need_cmd awk
need_cmd sha256sum
need_cmd sudo

fetch_url() {
	local url=$1
	local output=$2
	if command -v curl >/dev/null 2>&1; then
		curl -L --fail --output "$output" "$url"
	elif command -v wget >/dev/null 2>&1; then
		wget -O "$output" "$url"
	else
		die "missing curl or wget for downloading xbps-static"
	fi
}

download_file() {
	local url=$1
	local output=$2
	local description=$3
	if [ -f "$output" ]; then
		say "Using cached ${description}: $output"
		return
	fi
	say "Downloading ${description}: $url"
	fetch_url "$url" "$output"
}

discover_void_rootfs_url() {
	if [ -n "$rootfs_url" ]; then
		return
	fi

	local index_file rootfs_name
	index_file="${workdir}/void-live-current.html"
	say "Discovering minimal Void rootfs for ${void_arch}"
	fetch_url "${void_live_base%/}/" "$index_file"
	rootfs_name=$(awk -v arch="$void_arch" '
		{
			pattern = "void-" arch "-ROOTFS-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]\\.tar\\.xz"
			if (match($0, pattern)) {
				print substr($0, RSTART, RLENGTH)
				exit
			}
		}
	' "$index_file")
	[ -n "$rootfs_name" ] || die "could not find void-${void_arch}-ROOTFS tarball in ${void_live_base%/}/"
	rootfs_url="${void_live_base%/}/${rootfs_name}"
}

rootfs_expected_sha256() {
	local rootfs_name=$1
	if [ -n "$rootfs_sha256" ]; then
		printf '%s\n' "$rootfs_sha256"
		return
	fi
	if [ "${ORANGE_VOID_ROOTFS_VERIFY:-1}" = "0" ]; then
		return
	fi

	local checksum_file expected
	checksum_file="${workdir}/void-live-current-sha256sum.txt"
	printf '[*] Downloading Void rootfs checksums: %s\n' "$rootfs_sha256_url" >&2
	fetch_url "$rootfs_sha256_url" "$checksum_file" ||
		die "failed to download Void rootfs checksums from ${rootfs_sha256_url}"
	expected=$(awk -v name="$rootfs_name" '
		{
			for (i = 1; i <= NF - 3; i++) {
				if ($i == "SHA256" &&
						$(i + 1) == "(" name ")" &&
						$(i + 2) == "=") {
					print $(i + 3)
					exit
				}
			}
		}
	' "$checksum_file")
	[ -n "$expected" ] || die "could not find checksum for ${rootfs_name} in ${rootfs_sha256_url}"
	printf '%s\n' "$expected"
}

ensure_void_rootfs() {
	discover_void_rootfs_url

	local rootfs_name expected actual
	rootfs_name=${rootfs_url##*/}
	[ -n "$rootfs_tarball" ] || rootfs_tarball="${workdir}/${rootfs_name}"
	download_file "$rootfs_url" "$rootfs_tarball" "minimal Void rootfs"

	expected=$(rootfs_expected_sha256 "$rootfs_name") || exit $?
	if [ -n "$expected" ]; then
		actual=$(sha256sum "$rootfs_tarball" | awk '{print $1}')
		[ "$actual" = "$expected" ] || die "checksum mismatch for ${rootfs_tarball}"
	fi
}

xbps_static_arch() {
	case "${ORANGE_XBPS_STATIC_ARCH:-$(uname -m)}" in
		x86_64|amd64) printf 'x86_64' ;;
		aarch64|arm64) printf 'aarch64' ;;
		*) die "unsupported xbps-static host architecture: ${ORANGE_XBPS_STATIC_ARCH:-$(uname -m)}" ;;
	esac
}

have_host_xbps() {
	local tool
	for tool in xbps-install xbps-query xbps-rindex xbps-uhelper \
			xbps-reconfigure xbps-remove xbps-create xbps-uchroot \
			xbps-uunshare; do
		command -v "$tool" >/dev/null 2>&1 || return 1
	done
}

ensure_host_xbps() {
	if have_host_xbps; then
		return
	fi

	local arch static_base static_url tarball
	arch=$(xbps_static_arch)
	static_base=${ORANGE_XBPS_STATIC_BASE:-"https://repo-default.voidlinux.org/static"}
	static_url=${ORANGE_XBPS_STATIC_URL:-"${static_base}/xbps-static-latest.${arch}-musl.tar.xz"}
	tarball="${workdir}/xbps-static-latest.${arch}-musl.tar.xz"

	if [ ! -x "${xbps_static_dir}/usr/bin/xbps-uhelper" ]; then
		say "Downloading host-only xbps static tools for ${arch}"
		mkdir -p "$xbps_static_dir"
		fetch_url "$static_url" "$tarball"
		tar -C "$xbps_static_dir" --strip-components=0 -xJf "$tarball"
	fi

	export PATH="${xbps_static_dir}/usr/bin:${PATH}"
	have_host_xbps || die "xbps static tools were downloaded but required xbps commands are still unavailable"
}

ensure_glibc_target() {
	case "$void_arch" in
		*-musl) die "ORANGE_VOID_ARCH=$void_arch selects musl; use x86_64 for glibc" ;;
	esac
	case "$void_repo" in
		*/musl|*/musl/) die "ORANGE_VOID_REPO=$void_repo selects the musl repo; use https://repo-default.voidlinux.org/current for glibc" ;;
	esac
}

sudo_auth() {
	if sudo -n true >/dev/null 2>&1; then
		return
	fi
	if [ -n "${SUDO_PASS:-}" ]; then
		printf '%s\n' "$SUDO_PASS" | sudo -S -v >/dev/null
		return
	fi
	if [ -t 0 ]; then
		say "sudo is required to mount and populate the VM image"
		sudo -v
		return
	fi
	die "sudo credentials required. Run from an interactive terminal, or set SUDO_PASS for this script invocation."
}

sud() {
	if [ -n "${SUDO_PASS:-}" ]; then
		printf '%s\n' "$SUDO_PASS" | sudo -S "$@"
	else
		sudo "$@"
	fi
}

loop_partition() {
	local dev=$1 partition candidate
	for _ in 1 2 3 4 5 6 7 8 9 10; do
		for candidate in "${dev}p1" "${dev}1"; do
			if [ -b "$candidate" ]; then
				partition=$candidate
				printf '%s\n' "$partition"
				return
			fi
		done
		sleep 0.1
	done
	return 1
}

mkdir -p "$workdir"
ensure_glibc_target
ensure_host_xbps
ensure_void_rootfs

if [ ! -d "${void_packages}/.git" ]; then
	say "Cloning void-packages"
	git clone --depth=1 https://github.com/void-linux/void-packages.git "$void_packages"
else
	say "Using existing void-packages checkout: $void_packages"
fi

say "Copying local Orange package overlay"
rm -rf "${void_packages}/srcpkgs/orange" "${void_packages}/srcpkgs/wlroots0.20"
mkdir -p "${void_packages}/srcpkgs"
cp -a "${repo_root}/packaging/void/srcpkgs/orange" "${void_packages}/srcpkgs/orange"

say "Copying current Orange tree into the package files area"
rm -rf "${void_packages}/srcpkgs/orange/files/src"
mkdir -p "${void_packages}/srcpkgs/orange/files/src"
tar -C "$repo_root" \
	--exclude='.git' \
	--exclude='.cache' \
	--exclude='.local' \
	--exclude='.venv' \
	--exclude='.venv-wlroots-build' \
	--exclude='build' \
	--exclude='private' \
	--exclude='*.qcow' \
	--exclude='*.qcow2' \
	--exclude='*.raw' \
	--exclude='*.vdi' \
	--exclude='*.vmdk' \
	--exclude='*.vti' \
	-cf - . | tar -C "${void_packages}/srcpkgs/orange/files/src" -xf -

say "Bootstrapping xbps-src for ${void_arch} glibc"
(
	cd "$void_packages"
	./xbps-src -A "$void_arch" binary-bootstrap
	./xbps-src -A "$void_arch" bootstrap-update
)

say "Building orange package for ${void_arch} glibc"
(
	cd "$void_packages"
	./xbps-src -A "$void_arch" clean orange
	./xbps-src -A "$void_arch" -1 pkg orange
)

find_xbps_install() {
	local name root found
	for name in xbps-install.static xbps-install; do
		found=$(command -v "$name" || true)
		if [ -n "$found" ]; then
			printf '%s\n' "$found"
			return
		fi
		for root in "${void_packages}/masterdir-${void_arch}" "${void_packages}/masterdir"; do
			[ -d "$root" ] || continue
			found=$(find "$root" -type f -name "$name" -perm -111 | sort | head -n 1)
			if [ -n "$found" ]; then
				printf '%s\n' "$found"
				return
			fi
		done
	done
}

xbps_install=$(command -v xbps-install.static || true)
if [ -z "$xbps_install" ]; then
	xbps_install=$(find_xbps_install)
fi
[ -n "$xbps_install" ] || die "could not find xbps-install in xbps-src masterdir"

binpkgs="${void_packages}/hostdir/binpkgs"
[ -d "$binpkgs" ] || die "xbps-src did not create $binpkgs"

sudo_auth

loopdev=
cleanup_mounts() {
	sud umount "${mount_dir}/dev" >/dev/null 2>&1 || true
	sud umount "${mount_dir}/proc" >/dev/null 2>&1 || true
	sud umount "${mount_dir}/sys" >/dev/null 2>&1 || true
	sud umount "$mount_dir" >/dev/null 2>&1 || true
	if [ -n "${loopdev:-}" ]; then
		sud losetup -d "$loopdev" >/dev/null 2>&1 || true
		loopdev=
	fi
}
trap cleanup_mounts EXIT

say "Creating partitioned raw image: $image_raw"
cleanup_mounts
rm -f "$image_raw" "$image" "$kernel_out" "$initrd_out"
truncate -s "$image_size" "$image_raw"
printf 'label: dos\n,,L,*\n' | sfdisk "$image_raw" >/dev/null
loopdev=$(sud losetup --find --partscan --show "$image_raw")
root_partition=$(loop_partition "$loopdev") || die "could not find first partition for ${loopdev}"

say "Formatting VM root partition: $root_partition"
sud mkfs.ext4 -F -q "$root_partition"
root_uuid=$(sud blkid -s UUID -o value "$root_partition")
[ -n "$root_uuid" ] || die "could not read filesystem UUID for ${root_partition}"

say "Mounting VM image at $mount_dir"
sud mkdir -p "$mount_dir"
sud mount "$root_partition" "$mount_dir"

say "Extracting minimal Void rootfs"
sud tar -C "$mount_dir" --numeric-owner -xJf "$rootfs_tarball"

say "Installing VM runtime packages and Orange package"
sud mkdir -p "$mount_dir/var/db/xbps/keys"
keys_src=
for root in "${void_packages}/masterdir-${void_arch}" "${void_packages}/masterdir" "$xbps_static_dir"; do
	if [ -d "${root}/var/db/xbps/keys" ]; then
		keys_src="${root}/var/db/xbps/keys"
		break
	fi
done
if [ -n "$keys_src" ]; then
	sud cp -a "${keys_src}/." "$mount_dir/var/db/xbps/keys/"
fi

say "Updating rootfs xbps package"
sud env XBPS_ARCH="$void_arch" XBPS_TARGET_ARCH="$void_arch" "$xbps_install" -Suy -r "$mount_dir" \
	-R "$void_repo" \
	xbps

say "Updating rootfs base packages"
sud env XBPS_ARCH="$void_arch" XBPS_TARGET_ARCH="$void_arch" "$xbps_install" -Suy -r "$mount_dir" \
	-R "$void_repo"

sud env XBPS_ARCH="$void_arch" XBPS_TARGET_ARCH="$void_arch" "$xbps_install" -Sy -y -r "$mount_dir" \
	-R "$binpkgs" \
	-R "$void_repo" \
	runit-void linux grub qemu-ga dbus elogind gdm polkit \
	orange gnome-session gnome-settings-daemon gnome-apps ghostty nautilus \
	firefox gnome-software \
	adwaita-icon-theme adwaita-fonts hicolor-icon-theme \
	gnome-themes-extra gnome-themes-extra-gtk gnome-backgrounds gvfs \
	dejavu-fonts-ttf mesa-dri \
	ModemManager wl-clipboard \
	socklog socklog-void

say "Configuring guest"
printf 'orange-void\n' | sud tee "${mount_dir}/etc/hostname" >/dev/null
cat <<EOF | sud tee "${mount_dir}/etc/fstab" >/dev/null
UUID=${root_uuid} / ext4 defaults 0 1
tmpfs /tmp tmpfs defaults,nosuid,nodev 0 0
EOF

sud mount --bind /dev "${mount_dir}/dev"
sud mount -t proc proc "${mount_dir}/proc"
sud mount -t sysfs sys "${mount_dir}/sys"

sud env ORANGE_GRUB_DISK="$loopdev" ORANGE_ROOT_UUID="$root_uuid" chroot "$mount_dir" /bin/sh -eux <<'EOF'
echo 'HOSTNAME="orange-void"' >> /etc/rc.conf

mkdir -p /etc/runit/runsvdir/default
ln -sf /etc/runit/runsvdir/default /etc/runit/runsvdir/current 2>/dev/null || true
ln -sf /etc/runit/runsvdir/current /var/service 2>/dev/null || true
for svc in dhcpcd dbus elogind polkitd qemu-ga sshd gdm socklog-unix nanoklogd; do
	if [ -d "/etc/sv/${svc}" ]; then
		ln -sf "/etc/sv/${svc}" "/etc/runit/runsvdir/default/${svc}" 2>/dev/null || true
	fi
done

# elogind must not start before dbus is up
if [ -d /etc/sv/elogind ]; then
	cat > /etc/sv/elogind/run <<'RUN'
#!/bin/sh
exec 2>&1

# Wait for dbus to be available before starting elogind
while ! sv check dbus >/dev/null 2>&1; do
	sleep 0.5
done

exec elogind-daemon
RUN
	chmod +x /etc/sv/elogind/run
fi

if ! id orange >/dev/null 2>&1; then
	useradd -m -s /bin/bash orange
fi

groups="wheel,video,input,audio"
usermod -aG "$groups" orange
passwd -d orange

# Replace the commented-out PermitEmptyPasswords with 'yes'
sed -i 's/#PermitEmptyPasswords no/PermitEmptyPasswords yes/' /etc/ssh/sshd_config

echo 'orange ALL=(ALL) ALL' > /etc/sudoers.d/orange
chmod 0440 /etc/sudoers.d/orange

for _user in messagebus polkitd qemu gdm dbus; do
	if ! id "$_user" >/dev/null 2>&1; then
		useradd -r -s /usr/bin/nologin "$_user"
	fi
done
# messagebus must have uid 22 for dbus to start
if [ "$(id -u messagebus)" != "22" ]; then
	usermod -u 22 messagebus
	# fix file ownership for the new uid
	find / -user 22 -exec chown -h messagebus {} + 2>/dev/null || true
	find / -user messagebus -exec chown -h 22 {} + 2>/dev/null || true
fi
usermod -aG video gdm

install -d -m 0755 /etc/dracut.conf.d
cat > /etc/dracut.conf.d/orange-vm.conf <<'DRACUT'
hostonly="no"
add_drivers+=" virtio_pci virtio_blk virtio_scsi virtio_net virtio_gpu virtio_rng virtio_balloon ext4 "
DRACUT

kernel_version=$(
	for dir in /usr/lib/modules/*; do
		[ -d "$dir" ] || continue
		basename "$dir"
	done | sort -V | tail -n 1
)
[ -n "$kernel_version" ] || {
	echo "no kernel modules installed under /usr/lib/modules" >&2
	exit 1
}
command -v dracut >/dev/null 2>&1 || {
	echo "dracut is missing; linux-base should have installed it" >&2
	exit 1
}
dracut --force "/boot/initramfs-${kernel_version}.img" "$kernel_version"

install -d -m 0755 /boot/grub
cat > /boot/grub/grub.cfg <<GRUB
set default=0
set timeout=0

insmod part_msdos
insmod ext2
search --no-floppy --fs-uuid --set=root ${ORANGE_ROOT_UUID}

menuentry 'Orange Void' {
	linux /boot/vmlinuz-${kernel_version} root=UUID=${ORANGE_ROOT_UUID} rw console=tty1 loglevel=3
	initrd /boot/initramfs-${kernel_version}.img
}
GRUB

command -v grub-install >/dev/null 2>&1 || {
	echo "grub-install is missing; grub should have installed it" >&2
	exit 1
}
grub-install --target=i386-pc --recheck "$ORANGE_GRUB_DISK"
EOF

say "Copying kernel and initramfs out of guest"
kernel_src=$(find "${mount_dir}/boot" -maxdepth 1 -type f -name 'vmlinuz-*' | sort -V | tail -n 1)
initrd_src=$(find "${mount_dir}/boot" -maxdepth 1 -type f \( -name 'initramfs-*.img' -o -name 'initrd*' \) | sort -V | tail -n 1)
[ -n "$kernel_src" ] || die "no guest kernel found under ${mount_dir}/boot"
[ -n "$initrd_src" ] || die "no guest initramfs found under ${mount_dir}/boot"
sud cp "$kernel_src" "$kernel_out"
sud cp "$initrd_src" "$initrd_out"
sud chown "$(id -u):$(id -g)" "$kernel_out" "$initrd_out"

say "Unmounting VM image"
cleanup_mounts
trap - EXIT

say "Converting raw image to ${image_format}: $image"
qemu-img convert -f raw -O "$image_format" "$image_raw" "$image"

cat <<EOF

================================
VOID VM READY
Image:       $image
Format:      $image_format
Bootloader:  GRUB BIOS
Kernel copy: $kernel_out
Initrd copy: $initrd_out
Run:         scripts/run-void-vm.sh
================================
EOF
