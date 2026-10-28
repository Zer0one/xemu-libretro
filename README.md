# xemu libretro core

A [libretro](https://www.libretro.com/) core of [xemu](https://xemu.app) for Xbox and Sega Chihiro, for RetroArch and other libretro frontends.

The Xbox build is displayed as **Microsoft - Xbox (OG)**, with filename
`xemu_libretro` and data in `system/xemu/`. **Sega - Chihiro** uses
`xemu_chihiro_libretro`, with internal core name **Xemu-Chihiro**;
the optional combined **Xemu-Full** uses
`xemu_full_libretro`. The dedicated profiles expose only their relevant
options; all retain the existing `xemu_` keys. The dedicated Chihiro profile
organizes Free Play under System and arcade controls under Controls, without
a redundant machine category or Xbox dashboard/System Aspect Ratio settings. This separation is under
validation. The two dedicated macOS cores are installed locally for testing;
OutRun 2 Chihiro boot and attract have been verified. When upgrading from the runtime name `xemu`, copy
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
Xbox and Chihiro hardware in the same source base. The new
[build profiles](docs/libretro-build-profiles.md) select Xbox and Xemu-Chihiro
as the default artifacts, with Xemu-Full available only by explicit selection.
Build selection, option filtering and content admission are implemented;
Xbox and Chihiro/Vulkan have reached OutRun 2 attract; broader arcade lifecycle
and cross-platform validation remain pending. The earlier
[separation assessment](docs/chihiro-integration-plan.md) remains historical
research. The official xemu revision and the Libretro donor revision are tracked
separately.

See [Libretro Donor Integration](docs/libretro-donor-integration.md) for retained
adaptations and validation results; current Chihiro media support is described below.

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
| `xbox_hdd.qcow2` | optional existing Xbox HDD; the core creates its bundled xemu-dashboard image when absent |

The boot ROM and the BIOS are Microsoft's and have to be dumped from your own console; see xemu's [required files](https://xemu.app/docs/required-files/). The EEPROM (`xbox_eeprom.bin`) is created in the same folder when it is missing.

Resolve RetroArch's configured **System/BIOS** directory before copying these
files; installing the core alone does not install Microsoft firmware. A missing
Xbox HDD is created from the bundled MIT-licensed [xemu-dashboard](https://github.com/xemu-project/xemu-dashboard) image. An existing disk is never replaced.
**System Memory (MB)** defaults to the retail Xbox's **64 MB**. OutRun 2 and Halo
booted at 64 MB with the 4627 BIOS used in Linux validation; that BIOS does not
establish a requirement for 128 MB. Increasing memory is not a boot-failure fix.

## Content

Xbox games as `.iso` or `.xiso` (xemu's XISO format, as made by extract-xiso).

Chihiro uses FATX netboot `.bin` images or supported optical `.chd`, `.cue`/BIN,
`.gdi`, MODE1 `.iso` and raw MODE1 `.bin` files,
with matching security ZIPs or loose `.data` / `.pic` dumps beside the CHD,
beside its containing directory, or under `system/xemu-chihiro/keys/`.
MAME CHD/set associations and SHA-1 lookup support renamed dumps; a name-based
fallback warns if its key SHA-1 is unverified. If the MAME route cannot supply
a working key, image-name lookup searches beside the image, in
`system/xemu-chihiro/keys/`, then `system/xemu-chihiro/redump/`: use a matching
`<image-name>.data` / `.pic`, or a matching ZIP/folder containing DATA/PIC dumps
with arbitrary member names. Candidates must decrypt a valid FATX/`boot.id`;
a generic ISO signature alone does not identify a Chihiro game. Firmware belongs under
`system/xemu-chihiro/` or its `Chihiro/` subfolder (legacy fallback:
`system/xemu/Chihiro/`):
`chihiro_xbox_bios.bin`, `fpr21042_m29w160et.bin`,
`ic10_g24lc64.bin`, `ic11_24lc024.bin` and `pc20_g24lc64.bin`.
Linked cabinets additionally use `ver1305.bin`. The standard MAME `chihiro.zip`
and `seganetw.zip` sets may be placed under the profile's System root;
`seganetw.zip` supplies the network firmware.
Mutable `chihiro_eeprom.bin`, saves and snapshots remain outside the archive.
Firmware may also be read from
associated game ZIPs/folders or MAME `chihiro` / `seganetw` sets beside the
content or its containing folder; System sources take precedence. ZIP firmware
uses a hash-addressed cache under the profile's System root. Dedicated cores fix their
machine at build time; Full selects it from content. Xbox-patched Chihiro
discs remain Xbox content. Switching machine types inside Full still requires
restarting RetroArch. Full writes under `system/xemu-full/`; its Xbox HDD is
separate and is not automatically copied from the existing Xbox installation.
OutRun 2 has booted directly from its MAME CHD in RetroArch/Vulkan and reached
its introduction and attract scenes. The native loader decrypts directly into
DIMM; no converted game image or runtime Python/chdman is needed. See
[Direct Chihiro Media](docs/chihiro-direct-media.md) for key lookup, supported
track layouts, the shared title catalogue, uniform DIMM detection and validation
limits. OutRun 2 (World, Rev A) also boots from its Redump CUE/BIN set into
introduction/attract. GDI has synthetic reader/encrypted FATX coverage only.
Select the CUE with its referenced BIN files extracted together; game ZIPs
are storage archives, while the core reads ZIP containers only for keys/firmware.

Shader-cache formats follow the donor; device-specific shader variants remain
distinct. Save states remain experimental and unsupported in core metadata;
snapshots from before this integration are not guaranteed to load correctly.

## Video

In the dedicated Chihiro core, **Aspect Ratio** controls the presented image:
Auto uses 4:3; 16:9 stretches the 4:3 image. **System Aspect Ratio (Restart)**
is Xbox-only and changes the dashboard video flags games read; its default is
16:9, following the Libretro donor. **Internal Resolution Scale (Restart)**
is applied when content starts. Shader caching is always enabled. The old core
**Display Filtering** option was unused and has been removed; choose output
filtering in RetroArch.

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

## Controls And Audio

**Analog Dead Zone (Player 1/2)** uses the Libretro donor's radial stick filter,
0–30%, default 10%, on both sticks for Xbox and Chihiro. It replaces the older
Chihiro-only 12% steering filter; stale values of that old option are ignored.
The remaining travel is rescaled. This runs after RetroArch's own filtering,
so the effective physical deadzone can exceed either value. Digital steering
and pedals retain their mappings. **Rumble (Player 1/2)** can disable all rumble
for that pad; Chihiro force-feedback rumble defaults to 100% and light-gun aim
to Analog Stick, following the donor. See
[Drive-Board Centering](docs/chihiro-drive-centering.md) for the original reproduction.

**Audio Thread (Restart)** is enabled by default, following the Libretro donor
with the developer's approval. When the frontend accepts it, its audio thread
pulls APU samples independently of `retro_run`, including during shader stalls.
Disabling it, or using a frontend that rejects the callback, retains exactly
one frame's worth of audio per `retro_run`. The APU queue remains capped at
about 64 ms. DSP JIT (disabled), HRTF (enabled) and Voice Processor Threads
(Auto) follow standalone defaults. These settings are shared by both profiles.

Xbox/Full also expose **Trim HDD on Close**, disabled by default. It clears the
Xbox cache partitions X/Y/Z and rewrites the disk to reclaim space after shutdown;
internal snapshots prevent trimming. It requires temporary free space and can
lengthen content close. It does not apply to Chihiro or automatically run on an
existing user disk.

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
python3 scripts/build-libretro-profiles.py -- \
    --extra-cflags="-DXBOX=1 -Wno-error" --target-list=i386-softmmu \
    -Dpipewire=disabled -Db_staticpic=true
```

This builds Xbox and Chihiro in `build/profiles/xbox/` and
`build/profiles/chihiro/`. Use `--profiles full` before `--` to build Full
explicitly, or select one dedicated profile. Platform dependencies and
compiler flags remain the same as for the previous combined core.
For a manual single build, pass `-Dlibretro=true -Dlibretro_system=xbox`,
`chihiro` or `full` to `configure`; omission selects Xbox.
Use the matching generated `.info` in the build directory. The source
`ui/xemu_libretro.info` is the donor metadata input, not a split-core package.
The local builder does not bundle runtime libraries; packaging remains
platform-specific, as shown in the CI recipes.

The disabled CI workflow `.github/workflows/libretro.yml` contains the build
recipes and dependencies for Windows, Linux, Android and macOS.

## License

GPL-2.0, as xemu and QEMU. See [LICENSE](LICENSE).
