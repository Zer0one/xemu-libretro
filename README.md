# xemu libretro core

A [libretro](https://www.libretro.com/) core of [xemu](https://xemu.app) for Xbox and Sega Chihiro, for RetroArch and other libretro frontends.

Frontends display this core as **Microsoft - Xbox (OG)**. The core filename is
still `xemu_libretro`, support files stay in `system/xemu/`, and option keys
retain the `xemu_` prefix. When upgrading from the runtime name `xemu`, copy
the frontend's existing core configuration to the new name to retain settings.

The emulator is upstream's: this repository follows [xemu-project/xemu](https://github.com/xemu-project/xemu) and adds the libretro frontend in `ui/libretro.c`. It holds the core only; upstream's standalone UI, packaging and CI are not part of it (the paths are listed in `.upstream-excluded`). The version the core reports is upstream's, with the upstream commit it is merged to (`upstream.version`). The core is currently on xemu 0.8.138-3, commit 25e1ee2fd5.

## Repository References

This fork is developed on `main`. Its Git references have separate roles:

| Remote | Repository | Role |
|---|---|---|
| `origin` | [Zer0one/xemu-libretro](https://github.com/Zer0one/xemu-libretro) | This Libretro fork; development on `main`. |
| `libretro` | [WizzardSK/xemu-libretro](https://github.com/WizzardSK/xemu-libretro) | Libretro integration base; merge updates into `main` and reapply this fork's adaptations. |
| `upstream` | [xemu-project/xemu](https://github.com/xemu-project/xemu) | Official emulator upstream; the separate local `upstream` branch tracks `upstream/master`. |
| `michaeljsr` | [MichaelJSr/xemu-macos](https://github.com/MichaelJSr/xemu-macos) | Additional standalone macOS reference, branch `master`. |
| `cosmicsnow` | [CosmicSnow/xemu-for-macos](https://github.com/CosmicSnow/xemu-for-macos) | Additional standalone macOS reference, branch `master`. |

The `libretro` branch preserves the original port reference; development is not
merged into it. The separate `upstream` branch stays free of Libretro changes,
and full official upstream merges are integrated into `main`.

Libretro integration follows the donor's APIs and formats unless a documented
exception is needed. The current donor integration is `252a7accfb`. It keeps
Xbox and Chihiro in one core; the earlier
[separation assessment](docs/chihiro-integration-plan.md) is preserved as deferred
research. The official xemu revision and the Libretro donor revision are tracked
separately.

See [Libretro Donor Integration](docs/libretro-donor-integration.md) for retained
adaptations, validation results and the pending Chihiro media requirement.

The two additional remotes provide references for separately reviewed
adaptations. The official upstream remains the emulator base. Their fetch
configuration tracks only `master`; selected Vulkan adaptations and the exact
comparison revisions are documented in [Vulkan Integration And Validation](docs/libretro-vulkan.md).

Standalone checkouts, builds and runtime tests belong to the dedicated project
at `/Users/andrea/dev/xemu`. This repository remains dedicated to the Libretro core.

## Downloads

This fork's [GitHub Actions workflows](https://github.com/Zer0one/xemu-libretro/actions)
are currently disabled, including build, buildbot preflight and translation
workflows. Their recipes target `main` when explicitly re-enabled; no automatic
builds or releases are published automatically by this fork.

Manual builds are available from [Releases](https://github.com/Zer0one/xemu-libretro/releases).
**v0.8.138-3.1** preserves the Xbox-only core before further upstream and
Chihiro integrations, with the Linux boot/lifetime and Raven OpenGL corrections.
The macOS ARM64 package for Apple Silicon requires
macOS 14 or later because of its bundled libraries. It includes the core,
matching `.info` and `xemu_libretro_libs`; install the `.info` into RetroArch's
configured core-info directory. MoltenVK, console firmware and games are not
included. Sync is the default; Async requires a compatible patched frontend
MoltenVK driver.

The published checkpoint does not include the combined Xbox/Chihiro integration
being developed on `main`. The official xemu base remains `0.8.138-3`
(`25e1ee2fd5`).

Release tags use `v<upstream.version>.<core revision>`. The upstream part
identifies the integrated xemu version, including its commits-since-tag count;
the final core revision increases for releases on that same base.

Future releases target Windows, Linux and macOS. Android builds and packages
are excluded until the developer requests their return. The workflow defaults
and `all` selection omit Android; its source and explicit build recipe remain
available. Older Android release assets are unaffected.

RetroArch's Online Updater distributes the original Libretro port through its
buildbot. Those downloads are independent of this fork's changes. Use this
fork's release package or build from `main` to use its adaptations.

- **Linux:** `xemu_libretro.so` and `libslirp.so.0`; put both into RetroArch's `cores` folder. glibc builds, they do not load into a musl frontend.
- **macOS:** `xemu_libretro.dylib` and the folder `xemu_libretro_libs`; put both into RetroArch's `cores` folder.
- **Windows:** `xemu_libretro.dll`.
- **Android:** `xemu_libretro_android.so`, Android 10 or newer.

## Setup

For Xbox, put these files into RetroArch's system directory, under `system/xemu/`:

| File | What it is |
|---|---|
| `mcpx_1.0.bin` | the MCPX boot ROM, 512 bytes, MD5 `d49c52a4102f6df7bcf8d0617ac475ed` |
| `Complex_4627v1.03.bin` | the flash BIOS, a 1 MB or 256 KB image |
| `xbox_hdd.qcow2` | the hard disk, for example [xemu's ready-made image](https://github.com/xemu-project/xemu-hdd-image/releases) |

The boot ROM and the BIOS are Microsoft's and have to be dumped from your own console; see xemu's [required files](https://xemu.app/docs/required-files/). Other names and locations can be set in the core options. The EEPROM (`xbox_eeprom.bin`) is created in the same folder when it is missing.

Resolve RetroArch's configured **System/BIOS** directory before copying these
files; installing the core alone does not install its firmware or HDD.
**System Memory (MB)** defaults to the retail Xbox's **64 MB**. OutRun 2 and Halo
booted at 64 MB with the 4627 BIOS used in Linux validation; that BIOS does not
establish a requirement for 128 MB. Increasing memory is not a boot-failure fix.

## Content

Xbox games as `.iso` or `.xiso` (xemu's XISO format, as made by extract-xiso).

Chihiro uses FATX netboot `.bin` images and board firmware under
`system/xemu/Chihiro/`: `chihiro_xbox_bios.bin`, `fpr21042_m29w160et.bin`,
`ic10_g24lc64.bin`, `ic11_24lc024.bin` and `pc20_g24lc64.bin`.
Linked cabinets additionally use `ver1305.bin`. The core selects the machine
from the content, following the donor. Xbox-patched Chihiro discs are Xbox
content. Switching between Xbox and Chihiro requires restarting RetroArch.
Chihiro game boot has not yet been verified in this fork: the available test
media are Xbox-patched discs, not netboot images.

Shader-cache formats follow the donor; device-specific shader variants remain
distinct. Save states remain experimental and unsupported in core metadata;
snapshots from before this integration are not guaranteed to load correctly.

## Video

Select **Graphics API (Restart)** in Core Options and use RetroArch's matching `glcore` or `vulkan` video driver. Vulkan is the default; Android uses Vulkan only. macOS supports Vulkan through MoltenVK as well as OpenGL. The Metal path converts unsupported primitives on the CPU and uses hardware depth interpolation, which approximates NV2A depth behavior. Vulkan uses a dedicated renderer queue when available and hands frames to the frontend through GPU semaphores. See [Vulkan Integration And Validation](docs/libretro-vulkan.md) for driver selection, sources, evidence and remaining limits.

The integrated shader compiler requires Vulkan 1.2 or later. OpenGL requires
Core 4.0 (macOS provides 4.1).

When all staging allocations are host-visible, coherent and device-local, the
Vulkan renderer binds them directly and avoids redundant copies. Other memory
types retain separate device buffers and staging copies, with explicit flushes
for noncoherent writes. Selection is based on Vulkan allocation properties on
every platform. MJR's USB/reset corrections, reduced CPU fog-carryover work,
mapped-DMA coherence and NVNet receive-queue recovery also apply independently
of macOS. Snapshot restoration discards old GPU surfaces and visibility reports
before they can overwrite restored RAM; save states remain experimental. See
[MJR Adaptations](docs/libretro-mjr-adaptations.md) for sources and validation.

On macOS, **MoltenVK Queue Submission (Restart)** under Core Options → Video selects **Sync** (default) or **Async** host submission. Async requires a compatible MoltenVK driver with the acquisition and presentation fixes. Restart RetroArch after changing it; closing and reopening content may retain the existing Vulkan instance. The option applies only to the frontend-selected MoltenVK instance, whose provider path and requested mode are logged. If `VK_EXT_layer_settings` is unavailable, the core logs a warning and retains the driver's default. This option does not change VSync, audio synchronization or shader compilation, and has no effect on OpenGL.

For Linux boot failures and the unpaced-frontend corrections, see
[Linux Boot Validation](docs/libretro-linux-boot.md). The tested AMD/RadeonSI
OpenGL fence failure also occurs in official standalone xemu at the same source
revision. The core automatically detects Mesa/RadeonSI on Raven and omits unused
geometry-shader point-size output for lines and triangles, preserving point
output. This avoids the tested driver trigger without replacing Mesa; OutRun 2
attract was verified on Vega 11 with system Mesa 25.3.6. A separate Mesa driver
patch is also documented. See the validation document for evidence and limits.

On Vulkan devices without geometry shaders, W-buffer draws reconstruct guest W
from reciprocal W and use neutral host Z, while retaining guest depth clipping
and bias. Z-buffer draws retain hardware Z reconstruction. These fallback paths
remain approximations; see [the depth review](docs/libretro-vulkan-depth-candidates.md).

On Vulkan devices without custom border colors, packed depth-texture borders
are reproduced in the shader. This removes the grey horizon/building overlay
observed in OutRun 2. Unsupported sampler LOD bias is kept at zero; devices
that support it retain the guest's bias within the device limit.

## Video Debug Logging

**Video Debug Logging** under Core Options → Video writes the standalone's
Video Debug measurements to the frontend log once per second. It is disabled
by default. The log includes integer FPS/MSPF, named completed-frame GPU
counters, decimal guest event rates and rolling frame intervals. These are
guest measurements; they do not measure physical display presentation. See
[Video Debug Measurements](docs/libretro-video-debug.md) for the MJR mapping
and interpretation of 59.94 versus 60 FPS.

## Building

The macOS OpenGL context-reset investigation, standalone comparison, and isolated RetroArch boot procedure are documented in [macOS OpenGL Validation](docs/libretro-macos-opengl.md).

Vulkan reset, content close/reload in the same process, and normal frontend exit are covered in [Vulkan Lifecycle Validation](docs/libretro-vulkan-lifecycle.md). On macOS the resident core retains local symbol visibility so a later load is accepted by the frontend.

Linux, with upstream's build dependencies installed:

```
git clone --branch main https://github.com/Zer0one/xemu-libretro.git
cd xemu-libretro
./configure --extra-cflags="-DXBOX=1 -Wno-error" --target-list=i386-softmmu \
    -Dlibretro=true -Dpipewire=disabled -Db_staticpic=true
ninja -C build xemu_libretro.so
```

The disabled CI workflow `.github/workflows/libretro.yml` contains the build
recipes and dependencies for Windows, Linux, Android and macOS.

## License

GPL-2.0, as xemu and QEMU. See [LICENSE](LICENSE).
