# Void packaging and VM

This directory contains a local Void Linux packaging overlay for Orange. Orange
uses Void's packaged `wlroots0.20-devel` dependency instead of building a local
wlroots package.

The intended flow is:

```sh
scripts/build-void-vm.sh
scripts/run-void-vm.sh
```

For an installed Void system using this repository as a local package source,
copy the overlay into a `void-packages` checkout, build it, index the generated
local repository, and install `orange`:

```sh
git clone https://github.com/void-linux/void-packages.git
cd void-packages
./xbps-src binary-bootstrap

ORANGE_REPO=/path/to/orange-wlroots
cp -a "$ORANGE_REPO/packaging/void/srcpkgs/orange" srcpkgs/orange
rm -rf srcpkgs/orange/files/src
mkdir -p srcpkgs/orange/files/src
tar -C "$ORANGE_REPO" \
  --exclude .git \
  --exclude build \
  -cf - . | tar -C srcpkgs/orange/files/src -xf -

./xbps-src -1 pkg orange
xbps-rindex -a hostdir/binpkgs/*.xbps
sudo xbps-install -S -R "$PWD/hostdir/binpkgs" orange
```

The package depends on GDM, `gnome-core`, `gnome-apps`, Nautilus/GVFS,
Firefox, Ghostty, Adwaita icons/fonts/backgrounds, Adwaita GTK light/dark theme
support, ModemManager, and wl-clipboard. Void packages do not enable runit
services automatically; enable the login/session services once after install:

```sh
for svc in dbus elogind seatd polkitd gdm; do
	if [ ! -e "/var/service/$svc" ]; then
		sudo ln -s "/etc/sv/$svc" /var/service/
	fi
done
```

GDM then offers the packaged Orange Wayland session from
`/usr/share/wayland-sessions/orange.desktop`.

`scripts/build-void-vm.sh` clones `void-packages` into
`$ORANGE_VOID_WORKDIR/void-packages`, copies this overlay into that checkout,
copies the current Orange working tree into `srcpkgs/orange/files/src`, builds
the local `orange` package with `xbps-src`, then downloads Void's official
minimal `ROOTFS` tarball and installs the VM runtime package delta plus Orange
into a partitioned disk image. The extracted rootfs `xbps` package is upgraded
first, then the rootfs base package set is upgraded before installing that
package set so older rootfs snapshots can consume current Void repository
metadata without mixing old base libraries with newer runtime packages.

The VM uses an MBR disk with one ext4 root partition and boots through GRUB.
The build script generates a generic dracut initramfs in the guest, writes a
GRUB menu entry with the root filesystem UUID, and installs GRUB for BIOS boot.
QEMU then boots from the disk image instead of using direct `-kernel` and
`-initrd` arguments.

Set `ORANGE_VOID_IMAGE_FORMAT` to select the output format:

- `qcow` or `qcow2` for qcow2, the default
- `vmdk` for VMDK
- `vdi` for VirtualBox VDI
- `vti` as a compatibility alias for `vdi`

QEMU does not support a literal `vti` format, so the builder maps that spelling
to `vdi`. `ORANGE_VOID_IMAGE` can override the output path.

Inside the guest, GDM autologins as the `orange` user and starts
`/usr/share/wayland-sessions/orange.desktop`. The compositor is not started as a
root runit service; the display-manager path gives wlroots an `elogind` session
and seat.

`scripts/run-void-vm.sh` requires `/dev/kvm` by default. Set
`ORANGE_VM_ALLOW_TCG=1` only when you intentionally want the very slow software
emulator fallback.
