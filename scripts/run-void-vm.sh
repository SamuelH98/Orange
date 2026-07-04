#!/usr/bin/env bash
set -euo pipefail
image_override=${ORANGE_VOID_IMAGE:-}
image_format_requested=${ORANGE_VOID_IMAGE_FORMAT:-}
memory=${ORANGE_VM_MEMORY:-"8G"}
cpus=${ORANGE_VM_CPUS:-"4"}
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
infer_image_format
image=${image_override:-"${HOME}/orange-void.${image_extension}"}
[ -f "$image" ] || die "VM image not found: $image; run scripts/build-void-vm.sh first"
command -v qemu-system-x86_64 >/dev/null 2>&1 || die "missing qemu-system-x86_64"
accel=kvm
cpu=${ORANGE_VM_CPU:-host}
if [ ! -r /dev/kvm ] || [ ! -w /dev/kvm ]; then
    if [ "${ORANGE_VM_ALLOW_TCG:-0}" != "1" ]; then
        die "/dev/kvm is not accessible. Enable nested virtualization for WSL2 and restart WSL, or set ORANGE_VM_ALLOW_TCG=1 for a very slow fallback."
    fi
    accel=tcg
    if [ "${ORANGE_VM_CPU:-}" = "" ]; then
        cpu=max
    fi
fi
display_backend=${ORANGE_VM_DISPLAY:-gtk}
gl=${ORANGE_VM_GL:-on}
display_opts=()
case "$display_backend" in
    gtk)
        display_opts=(-display "gtk,gl=${gl},show-cursor=on")
        ;;
    sdl)
        display_opts=(-display "sdl,gl=${gl}")
        ;;
    none)
        display_opts=(-display none -serial mon:stdio)
        ;;
    *)
        die "unsupported ORANGE_VM_DISPLAY=$display_backend"
        ;;
esac
audio_opts=()
if [ -n "${PULSE_SERVER:-}" ] && command -v pactl >/dev/null 2>&1; then
    if pactl info >/dev/null 2>&1; then
        audio_opts=(-audiodev pa,id=snd0 -device virtio-sound-pci,audiodev=snd0)
    fi
fi
# Pointer device selection.
# usb-tablet reports ABSOLUTE coordinates, which keeps host/guest cursor
# position in sync without mouse-grab, but some wlroots/libinput setups
# apply a default calibration matrix to tablet-class devices that inverts
# the Y axis (desktop renders fine; cursor movement feels flipped).
# virtio-mouse-pci reports RELATIVE motion and is not subject to that
# calibration-matrix issue, so it's now the default. Set
# ORANGE_VM_POINTER=tablet to restore the old absolute-positioning behavior
# (e.g. if you fix the calibration_matrix in your compositor config and want
# seamless cursor sync back).
pointer=${ORANGE_VM_POINTER:-mouse}
pointer_opts=()
case "$pointer" in
    mouse)
        pointer_opts=(-device virtio-mouse-pci)
        ;;
    tablet)
        pointer_opts=(-device "usb-tablet,bus=usb0.0")
        ;;
    *)
        die "unsupported ORANGE_VM_POINTER=$pointer; use mouse or tablet"
        ;;
esac
# Disable the default VGA device so virtio-gpu is the only display adapter.
# Without -vga none, the guest kernel prefers the legacy VGA framebuffer and
# the Wayland compositor never opens the virtio-gpu DRM node.
# Prefer virtio-gpu-rutabaga-pci for native Vulkan support (ideal for wlroots
# with Vulkan renderer), but fall back to virtio-gpu-gl-pci with ample hostmem.
gpu_opts=(
    -vga none
)
if qemu-system-x86_64 -device help 2>&1 | grep -q virtio-gpu-rutabaga-pci; then
    gpu_opts+=(-device "virtio-gpu-rutabaga-pci,edid=on,hostmem=4G")
else
    gpu_opts+=(-device "virtio-gpu-gl-pci,edid=on,hostmem=4G")
fi
printf '[*] Starting Void Orange VM (accel=%s, display=%s, pointer=%s, image_format=%s)\n' "$accel" "$display_backend" "$pointer" "$image_format"
exec qemu-system-x86_64 \
    -m "$memory" \
    -smp "$cpus" \
    -machine "type=q35,accel=${accel}" \
    -cpu "$cpu" \
    -drive "file=${image},format=${image_format},if=virtio" \
    -boot order=c \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-pci,netdev=net0 \
    "${gpu_opts[@]}" \
    -device virtio-keyboard-pci \
    -device qemu-xhci,id=usb0 \
    "${pointer_opts[@]}" \
    -device virtio-rng-pci \
    -device virtio-balloon-pci \
    "${audio_opts[@]}" \
    "${display_opts[@]}" \
    "$@"