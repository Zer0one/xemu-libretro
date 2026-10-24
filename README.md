# xemu libretro core

A [libretro](https://www.libretro.com/) core of [xemu](https://xemu.app), the original Xbox emulator, for RetroArch and other libretro frontends.

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
| `libretro` | [WizzardSK/xemu-libretro](https://github.com/WizzardSK/xemu-libretro) | The original Libretro port. |
| `upstream` | [xemu-project/xemu](https://github.com/xemu-project/xemu) | Official emulator upstream; the separate local `upstream` branch tracks `upstream/master`. |
| `michaeljsr` | [MichaelJSr/xemu-macos](https://github.com/MichaelJSr/xemu-macos) | Additional standalone macOS reference, branch `master`. |
| `cosmicsnow` | [CosmicSnow/xemu-for-macos](https://github.com/CosmicSnow/xemu-for-macos) | Additional standalone macOS reference, branch `master`. |

The two additional remotes provide references for separately reviewed
adaptations. The official upstream remains the emulator base. Their fetch
configuration tracks only `master`; selected Vulkan adaptations and the exact
comparison revisions are documented in [Vulkan Integration And Validation](docs/libretro-vulkan.md).

Standalone checkouts, builds and runtime tests belong to the dedicated project
at `/Users/andrea/dev/xemu`. This repository remains dedicated to the Libretro core.

## Downloads

The core is built by libretro's buildbot for Windows and Linux (x86_64, aarch64), and is installed from RetroArch: Online Updater > Core Downloader. Every push to the `libretro` branch is also built in [Actions](https://github.com/WizzardSK/xemu-libretro/actions/workflows/libretro.yml), macOS (Apple Silicon, Intel) and Android (arm64-v8a, x86_64) included; the builds are the artifacts of a run, and downloading them needs a GitHub login.

- **Linux:** `xemu_libretro.so` and `libslirp.so.0`; put both into RetroArch's `cores` folder. glibc builds, they do not load into a musl frontend.
- **macOS:** `xemu_libretro.dylib` and the folder `xemu_libretro_libs`; put both into RetroArch's `cores` folder.
- **Windows:** `xemu_libretro.dll`.
- **Android:** `xemu_libretro_android.so`, Android 10 or newer.

## Setup

Put these files into RetroArch's system directory, under `system/xemu/`:

| File | What it is |
|---|---|
| `mcpx_1.0.bin` | the MCPX boot ROM, 512 bytes, MD5 `d49c52a4102f6df7bcf8d0617ac475ed` |
| `Complex_4627v1.03.bin` | the flash BIOS, a 1 MB or 256 KB image |
| `xbox_hdd.qcow2` | the hard disk, for example [xemu's ready-made image](https://github.com/xemu-project/xemu-hdd-image/releases) |

The boot ROM and the BIOS are Microsoft's and have to be dumped from your own console; see xemu's [required files](https://xemu.app/docs/required-files/). Other names and locations can be set in the core options. The EEPROM (`xbox_eeprom.bin`) is created in the same folder when it is missing.

Resolve RetroArch's configured **System/BIOS** directory before copying these
files; installing the core alone does not install its firmware or HDD. A debug
BIOS such as the 4627 image used in local validation needs **System Memory (MB)**
set to **128 MB**; the retail default remains 64 MB.

## Content

Xbox games as `.iso` or `.xiso` (xemu's XISO format, as made by extract-xiso).

## Video

Select **Graphics API (Restart)** in Core Options and use RetroArch's matching `glcore` or `vulkan` video driver. Vulkan is the default; Android uses Vulkan only. macOS supports Vulkan through MoltenVK as well as OpenGL. The Metal path converts unsupported primitives on the CPU and uses hardware depth interpolation, which approximates NV2A depth behavior. Vulkan uses a dedicated renderer queue when available and hands frames to the frontend through GPU semaphores; MoltenVK keeps synchronous host submission and frontend VSync. See [Vulkan Integration And Validation](docs/libretro-vulkan.md) for driver selection, sources, evidence and remaining limits.

On Vulkan devices without geometry shaders, W-buffer draws reconstruct guest W
from reciprocal W and use neutral host Z, while retaining guest depth clipping
and bias. Z-buffer draws retain hardware Z reconstruction. These fallback paths
remain approximations; see [the depth review](docs/libretro-vulkan-depth-candidates.md).

On Vulkan devices without custom border colors, packed depth-texture borders
are reproduced in the shader. This removes the grey horizon/building overlay
observed in OutRun 2. Unsupported sampler LOD bias is kept at zero; devices
that support it retain the guest's bias within the device limit.

## Building

The macOS OpenGL context-reset investigation, standalone comparison, and isolated RetroArch boot procedure are documented in [macOS OpenGL Validation](docs/libretro-macos-opengl.md).

Vulkan reset, content close/reload in the same process, and normal frontend exit are covered in [Vulkan Lifecycle Validation](docs/libretro-vulkan-lifecycle.md). On macOS the resident core retains local symbol visibility so a later load is accepted by the frontend.

Linux, with upstream's build dependencies installed:

```
git clone https://github.com/WizzardSK/xemu-libretro.git
cd xemu-libretro
./configure --extra-cflags="-DXBOX=1 -Wno-error" --target-list=i386-softmmu \
    -Dlibretro=true -Dpipewire=disabled -Db_staticpic=true
ninja -C build xemu_libretro.so
```

The CI workflow `.github/workflows/libretro.yml` builds every platform, and shows each one's dependencies.

## License

GPL-2.0, as xemu and QEMU. See [LICENSE](LICENSE).
