# Linux Boot Validation

The core includes portable boot/lifetime corrections and an automatic OpenGL
workaround for Mesa/RadeonSI on Raven. On the tested Vega 11 system the
workaround needs the original system Mesa, with no driver patch or debug flag.

## Scope And Reference

The investigation started from `libretro-test-4`, repository commit
`3d45b56f8ffae8cd1604eee2c8e017cc3ea12719`, official xemu base
`25e1ee2fd59a451f2ab57e825b4f8a5414220c8b`. Linux tests used Batocera 43.1,
RetroArch 1.22.2, Ryzen 5 3400G / Radeon Vega 11 and Mesa 25.3.6. macOS
controls used an Apple M4 and RetroArch 1.22.2. Artifacts were identified by
SHA-256 and startup revision logs, not their filenames.

Retail 64 MB is the baseline. OutRun 2 and Halo boot with the tested Complex
4627 BIOS in paced Vulkan controls. The earlier claim that this BIOS needed
128 MB was incorrect; increasing RAM does not correct the reported boot bug.

## Unpaced Vulkan Boot Failure

With audio initialization failing and VSync disabled, the Linux frontend
called `retro_run` roughly 3,000–4,600 times per second. The guest eventually
stopped updating and the final image was black. Restoring either functioning
audio synchronization or VSync restored boot with the original release.
These are frontend call rates, not game FPS.

The diagnostic build separated requested VBlank updates, completed callbacks,
actual NV2A VBlank deliveries and guest acknowledgements. Requests accumulated
while callback completion stopped. A bounded native debugger capture then
identified the wait chain:

1. The APU worker held its mutex across consecutive processing blocks.
2. An unpaced frontend continuously emptied its audio ring, so the normal
   watermark wait did not release that mutex.
3. The guest CPU waited in `voice_lock` for the APU mutex while holding BQL.
4. The QEMU main loop could not reacquire BQL to deliver queued VBlank work.

A simple unlock/relock between APU blocks was insufficient on the Linux host:
the worker could reacquire the unfair mutex repeatedly before the guest writer.
The retained correction announces pending guest register writers atomically.
At a complete APU block boundary, the worker waits on its existing condition
variable until an announced writer acquires the mutex and completes its update.
No new worker, presentation wait or fixed host pacing sleep is introduced.
The waiter count is host synchronization state and is not serialized.

Once lock starvation was removed, frontend calls could still deliver thousands
of VBlank interrupts per second. The callback now checks the guest virtual
clock against the declared refresh interval. Extra requests in the same
interval do not generate another interrupt. A late callback emits one update,
without a burst of catch-up interrupts; a backward clock jump rebases the
deadline. Refresh declaration, VBlank interval and audio sample accounting use
the same 59.94 Hz constant. The frontend continues to pace audio and display.

The common APU and VBlank changes are under the Libretro build path, not under
a Linux-specific runtime setting. They therefore also apply to Windows,
Android and macOS builds. Runtime verification in this investigation covers
macOS arm64 and Linux x86_64; it does not certify the other architectures.

## Metadata Loading And Library Lifetime

The Linux debugger observed a separate lifetime defect: RetroArch loaded the
core for a metadata query, unloaded it, then loaded it again. QEMU's RCU
constructor ran twice while its first thread was still alive. Captures show
two RCU consumers and failures in RCU queue/event handling. Pinning only in
`retro_init` was too late for the initial metadata probe.

Linux and Android now link the core with ELF `NODELETE`, protecting the image
before constructors execute. The existing platform pinning logic is also
called by `retro_get_system_info`, as well as `retro_init`. Windows retains
its module-pin API; POSIX builds retain local-symbol `dlopen` pinning.
Content close still follows the existing upstream shutdown sequence and joins
the emulation thread; keeping the library mapped does not replace cleanup.

The macOS positive result has a concrete loader explanation. A three-cycle
metadata-only test left the release dylib mapped after each `dlclose`, at the
same function address. Its Mach-O header has `MH_HAS_TLV_DESCRIPTORS`; Apple's
[dyld policy](https://github.com/apple-oss-distributions/dyld/blob/main/common/MachOAnalyzer.cpp#L2677-L2689)
marks such images as non-unloadable. Android's
[Bionic linker](https://android.googlesource.com/platform/bionic/+/master/linker/linker_soinfo.cpp)
also recognizes `DF_1_NODELETE`. Actual Windows/Android runtime checks remain
separate work.

Keeping the original Linux core mapped through an external diagnostic helper
reduced the RCU consumers to one but did not fix the black screen. Likewise,
coalescing VBlank requests alone did not fix it. Those controls distinguish
the lifetime defect from APU starvation and guest interrupt cadence. The
experimental coalescing change was removed.

## OpenGL Raven/RadeonSI Workaround

The original core and official standalone xemu at the exact upstream revision
both fail at the OpenGL fence on the tested RadeonSI host. A captured command
stream also hangs the GPU when replayed without the core or Xbox emulation.
The failure is therefore not specific to the Libretro frontend.

Selective driver tests isolated point-size output elimination in the legacy
geometry/copy-shader path. A clean Mesa 25.3.6 build with one copy-shader state
assignment passes replay and native attract tests; an earlier instrumented
prototype failure remains unexplained. That separate driver patch is retained
as evidence. The integrated core workaround avoids the triggering shader output
and does not require replacing Mesa.

At OpenGL shader initialization the core checks for Mesa in the GL version
and RadeonSI plus the Raven renderer token. It enables the workaround for
that configuration; it does not select every AMD GPU or Raven2. It logs the
actual renderer and version when enabled. The workaround applies by driver
and hardware rather than by operating-system name.

The geometry shader omits `gl_PointSize` only when emitting lines or triangles.
Point-output geometry shaders and the direct-point vertex path retain their
original code. Vulkan and other OpenGL drivers keep the default false property.
No optimized-variant flag, shader-key mask, core option, timeout change or
removed assertion is involved.

The chosen GPU property is copied into `GeomState` before cache loading and
selection. Both the program and geometry-module cache keys include it, so the
new shader variant has a distinct key from the original output variant.

## Validation

The actual current and original shader generators were compared across
primitive/polygon modes, winding, provoking vertex, shading and perspective:

| Check | Result |
| --- | --- |
| Default OpenGL/Vulkan shader output | 1,248 cases byte-identical to original |
| Affected line/triangle output | 504 cases remove only the point-size assignment |
| Affected point output | 120 cases retain the entire original shader |
| Direct-point/no-geometry decision | 144 cases preserved |
| Stock Mesa, fresh cache | OutRun 2 visible 3D attract for 75 seconds, normal close |
| Stock Mesa, existing cache | Same result using 50 shader binaries from the first run |

The unsupported polygon-point path was excluded from the generator harness.
These source comparisons do not establish native GPU point-rendering coverage.

Both native core tests automatically activate the workaround and load the
original system libgallium. Both exit normally, join the emulation thread and
leave kernel logs, protected files and original ROM metadata unchanged. The
50 seeded shader binaries retain their hashes and modification times after
the second run. No private Mesa, `AMD_DEBUG`, preload or `SI_DIAG_*` control
is selected.

The validated Linux core includes the earlier portable boot/lifetime changes.
Its SHA-256 is
`6c3158611292c99a556010c78f1b2e23c965daec5c87734a0967f41523f110fe`.
Earlier common-fix builds reached 64 MB OutRun 2 3D attract in Linux Vulkan
with paced and unpaced frontends and in macOS OpenGL. The updated macOS build
was separately deployed and tested before the Raven workaround was added.
Windows/Android and other CPU/GPU configurations have not been runtime-validated
by this investigation.

## Evidence And Limits

The retained local collection is `build/linux-fix-2026-10-26/`. Its
`core-pointsize/` folder contains the final Linux build, source identity,
generator checks, native results and cache comparison. `FINAL-STATUS.md`
is the closure summary; `OPENGL-DIAGNOSIS.md` retains the diagnostic chronology,
including unsuccessful controls and the unexplained older prototype failure.
The original captured graphics trace remains private. Archived result folders
can be extracted for deeper analysis.

The exact-revision standalone evidence remains in its separate project at
`/Users/andrea/dev/xemu/.local/linux-upstream-25e1ee2-20261026/`. No standalone
build is part of the core repository.

These tests establish boot, attract-mode video, cache handling and normal
close. They do not establish interactive gameplay, audible output, physical
display pacing, long-session stability or other Radeon GPUs. The earlier
OpenGL failure on stock Mesa does not justify a blanket all-AMD workaround.
Original ROMs were opened in place; writable HDD/EEPROM, configurations and
caches were isolated. A failed or black-image test remains a failure even when
its outer process exited successfully; GPU-reset events determine classification.

Diagnostic test roots, duplicate payloads, caches and temporary build data are
removed after preserving representative artifacts and structured receipts.
System Mesa and the installed Batocera core remain unchanged. Publishing and
release workflows are separate from this local integration.
