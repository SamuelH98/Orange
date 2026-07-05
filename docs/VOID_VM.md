# Void VM

Orange's Void VM path uses Void's minimal rootfs as the VM base and keeps
Orange package-first:

1. `scripts/build-void-vm.sh` prepares a local `void-packages` checkout.
2. The script overlays `packaging/void/srcpkgs/orange`.
3. The current Orange working tree is copied into
   `srcpkgs/orange/files/src` inside the checkout.
4. `xbps-src` builds the local `orange` package using Void's packaged
   `wlroots0.20-devel` dependency.
5. The script downloads and verifies Void's official minimal `ROOTFS` tarball.
6. The rootfs is extracted into a partitioned disk image with an ext4 root
   partition.
7. The rootfs `xbps` package is updated, the rootfs base package set is updated,
   the generated packages plus the VM runtime package delta are installed, and a
   generic dracut initramfs is generated for QEMU virtio boot devices.
8. The guest installs GRUB for BIOS boot and writes a GRUB menu entry using the
   root filesystem UUID.
9. The guest enables `dbus`, `elogind`, `seatd`, `polkitd`, and `gdm`; GDM
   autologins as the `orange` user and starts the packaged Orange Wayland
   session.
10. `scripts/run-void-vm.sh` starts QEMU with KVM and a GTK display for WSLg.

The `orange` Void package is deliberately desktop-complete for a normal Void
install. A repository user should be able to add the repo and run
`xbps-install orange`; the dependency set pulls GDM, `gnome-core`,
`gnome-apps`, Nautilus/GVFS, Firefox, Ghostty, Adwaita icons/fonts/backgrounds,
Adwaita GTK light/dark theme support, ModemManager, and `wl-clipboard`. The package
installs the Orange session file, default config, bundled wallpapers under both
`/usr/share/orange/assets` and `/usr/share/backgrounds/orange`, and a Wayland
session wrapper that exports toolkit Wayland defaults, Chromium/Electron Ozone
hints, Firefox Wayland mode, and `ORANGE_TERMINAL=ghostty`.

Void still leaves runit service activation to the administrator. After
installing Orange on a real Void system, enable the session services once:

```sh
for svc in dbus elogind seatd polkitd gdm; do
	if [ ! -e "/var/service/$svc" ]; then
		sudo ln -s "/etc/sv/$svc" /var/service/
	fi
done
```

The VM build needs network access, `sudo`, `qemu-img`, `sfdisk`, `losetup`,
`mkfs.ext4`, `blkid`, and a host that can mount loopback images. The build
script caches Void's official minimal rootfs under `~/.cache/orange-void-vm/`
and verifies it against
`live/current/sha256sum.txt`. If the host does not already provide xbps tools,
the script downloads Void's official static xbps helper archive under
`~/.cache/orange-void-vm/xbps-static` and uses it only to run `xbps-src` and
install packages into the rootfs.

The output image format is selected with `ORANGE_VOID_IMAGE_FORMAT`:

- `qcow` or `qcow2` writes a qcow2 image, defaulting to `~/orange-void.qcow2`.
- `vmdk` writes a VMDK image, defaulting to `~/orange-void.vmdk`.
- `vdi` writes a VirtualBox VDI image, defaulting to `~/orange-void.vdi`.
- `vti` is accepted as a compatibility alias for `vdi`; QEMU does not provide a
  literal `vti` image format.

`ORANGE_VOID_IMAGE=/path/to/image` can override the output path. The build and
run scripts use `ORANGE_VOID_IMAGE_FORMAT` when set; otherwise they infer the
format from the image extension.

The guest/package target is glibc `x86_64` by default:

- `ORANGE_VOID_ARCH=x86_64`
- `ORANGE_VOID_REPO=https://repo-default.voidlinux.org/current`

Do not set `ORANGE_VOID_ARCH=x86_64-musl` or point `ORANGE_VOID_REPO` at
`current/musl` for this VM path. The `musl` suffix in Void's xbps-static
download is host-tool packaging only; it is not the guest ABI.

Rootfs inputs can be overridden for pinned or mirrored builds:

- `ORANGE_VOID_LIVE_BASE=https://repo-default.voidlinux.org/live/current`
- `ORANGE_VOID_ROOTFS_URL=.../void-x86_64-ROOTFS-YYYYMMDD.tar.xz`
- `ORANGE_VOID_ROOTFS_TARBALL=/path/to/cached-rootfs.tar.xz`
- `ORANGE_VOID_ROOTFS_SHA256=<expected sha256>`

Set `ORANGE_VOID_ROOTFS_VERIFY=0` only for a trusted local rootfs cache that
cannot be matched against Void's checksum file.

Void names the GNOME Display Manager package `gdm`, not `gdm3`. The VM image
uses GDM so Orange starts inside a real `elogind` login session instead of as a
root boot service; this is the expected path for wlroots seat creation.

The run step needs `/dev/kvm` for acceleration. On WSL, expose nested
virtualization before running:

```ini
[wsl2]
nestedVirtualization=true
```

Then run `wsl --shutdown` from Windows and start the distro again.
