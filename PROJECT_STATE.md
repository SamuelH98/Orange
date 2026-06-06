# PROJECT_STATE

## Project Overview

### Project Name

Tahoe wlroots

### Goal

Build a C/wlroots Linux compositor prototype that recreates the supplied macOS
Tahoe 26 desktop reference and behaves like a usable macOS-style shell, with
optional local Apple asset overrides and Vulkan renderer support through
wlroots.

### Current Status

Requirements, research, and architecture were revised after the user clarified
that the end result should function like a real macOS clone. Implementation is
in progress.

---

## Completed Features

### Project Planning

#### Validation

Local dependency versions were checked with `pkg-config`, `meson`, `ninja`, and
`cc`.

#### Tests Added

Pending.

---

## Current Work

### Active Feature

Native compositor prototype and shell renderer.

### Progress

Repository initialized. Docs created. Build and source implementation pending.

### Remaining Work

Implement compositor runtime, shell renderer, hit testing, xdg-shell window
management, launchers, tests, validation, and Git commits.

---

## Next Actions

1. Add Meson build files and C modules.
2. Implement deterministic shell layout and launcher tests.
3. Build and validate headless one-frame rendering.

---

## Risks

### Open Questions

None blocking for the prototype.

### Known Issues

Apple proprietary assets are not included; users must supply local licensed
files under ignored paths for private 1:1 asset replacement.

### Technical Concerns

wlroots 0.17 APIs are unstable and may need porting for newer distros. Vulkan
renderer availability depends on the local GPU/driver and wlroots runtime
support.

---

## Resume Instructions

Continue from implementation. Target wlroots 0.17.1, keep Apple assets ignored,
and validate with `WLR_BACKENDS=headless build/tahoe-wlroots --headless --once`.
