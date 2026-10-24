# Vulkan Integration And Validation

## Source Baselines

The official upstream merge applies `xemu-project/xemu:master` at
`25e1ee2fd59a451f2ab57e825b4f8a5414220c8b`, version `0.8.138-3`.
The merge is a full merge, with `.upstream-excluded` applied immediately;
standalone UI, packaging and workflows remain removed. The newer WizzardSK port
reference `c3cf3a923c` contributes its translation update. Development stays on
this fork's `main`, as requested by the owner. The earlier local OpenGL fixes
are retained.

Targeted macOS renderer references, rather than whole standalone-fork merges:

- [CosmicSnow MoltenVK compatibility, `115f912c6d`](https://github.com/CosmicSnow/xemu-for-macos/commit/115f912c6d6f2e790f1839e48b55f6137c24d09b):
  CPU primitive conversion, capability-dependent rasterization and hardware
  depth fallback. The Libretro shared-device path already avoids external-memory
  export, so the standalone build/link and instance setup changes are not copied.
- [MichaelJSr MoltenVK compatibility, `50ccd17cfd`](https://github.com/MichaelJSr/xemu-macos/commit/50ccd17cfd):
  skip the geometry-shader winding probe on devices without geometry shaders.
- [MichaelJSr driver work, `5706595936`](https://github.com/MichaelJSr/xemu-macos/commit/5706595936):
  CPU expansion of triangle fans on Apple, avoiding the driver's compute
  conversion and render-pass restart. Its standalone driver picker, MetalFX,
  CPU/TCG experiments and packaging are not part of this core integration.
- The local Xenia core's `libretro/libretro_vk_presenter.cc` and
  `tools/moltenvk/README.md`: per-instance MoltenVK configuration and exact provider/patch-marker
  detection, adapted to this core's C wrapper. Asynchronous submission was
  evaluated and superseded by the measured synchronous policy below.

`MichaelJSr/xemu-macos:master` at `74a27096ed6bd7b5ad6d83ed7fd55230f9e98cb8`
contains the same official upstream revision. Its existing dedicated-project
validation reports OutRun 2 title rendering on Apple M4 through MoltenVK 1.4.2.
That reference uses additional fork changes and a Metal presenter, so it is not
an identical frontend or performance baseline. No standalone build was created
in this checkout.

## Frontend And Driver Contract

**Graphics API (Restart)** selects Vulkan or OpenGL. Use the corresponding
RetroArch video driver. Android remains Vulkan-only. On macOS, Vulkan now uses
MoltenVK rather than being forcibly replaced with OpenGL.

The frontend owns the Vulkan instance/device. The renderer adopts that device,
passes images with producer dependencies to RetroArch and keeps them per sync
index. Prefer a dedicated graphics/compute queue, using another queue family
when the frontend's family has only one queue. Frame images shared between
families use concurrent sharing; a binary semaphore establishes each producer
to frontend dependency. The core waits the frontend sync index before reuse and
fills it each run rather than duplicating an older image across indices.
If no dedicated queue is available, use the frontend queue under its lock.
GPU completion waits remain outside that lock. No external-memory export or
second MoltenVK copy is required. Native-platform feature requirements stay as
before; Apple enables only supported features and enables portability subset
only when advertised.

Vulkan negotiation v2 allows `VK_EXT_layer_settings` configuration on this one
instance. The core resolves the provider selected by RetroArch, checks the local
`xeniaMoltenVKDrawableOwnershipPatchVersion` symbol in that exact loaded library
and verifies that the symbol belongs to the same library address range:

- Request synchronous host submission for MoltenVK, including patched drivers.
  This matches its default and avoids the measured VSync throughput loss in
  xemu's blocking submission path. The marker is logged for driver diagnostics;
  it no longer selects asynchronous submission.
- Missing layer-settings extension, or negotiation v1: keep driver defaults and
  log the limitation.

The marker is local driver metadata, not a MoltenVK version guarantee. No global
environment or RetroArch configuration is changed, and no driver is installed
by the core. On a dedicated renderer queue, the display compositor now signals
a producer semaphore without immediately waiting on the CPU. Each frontend
sync index has its own command buffer, descriptor set and completion fence;
CPU reuse waits that fence. Display resize, overlay replacement, surface
destruction, renderer finish and teardown drain pending producer work. The
shared-queue fallback retains its completed-image handoff. See
[Asynchronous Display Handoff](libretro-vulkan-async-handoff.md) for the separate
GPU-handoff and MoltenVK-host policies. The tested local RetroArch already carried Xenia's signed patched 1.4.2
framework; its original framework was not overwritten during this work.

## Corrections And Optimizations

macOS now exports only `retro_*` entry points. Without this, dyld coalesced
incompatible exported glslang C++ definitions from RetroArch and the core. The
first Vulkan boot crashed in `glslang::TParseContext::lValueErrorCheck` while
compiling the display shader. Restricting exports removed that crash without
changing shader input or the frontend.

CPU primitive conversion covers the fork's line-loop, quad, quad-strip and
triangle paths. Both indexed and array draws share one conversion implementation;
incomplete primitives are ignored. Triangle list conversion accounts for the
guest's first/last provoking-vertex choice. Apple triangle fans are expanded
before submission. Geometry-shader devices retain the original shader path.

The first working integration submitted the display image even if no new guest
picture existed. A five-second process sample placed 3,812 of 3,960 PFIFO samples
in the display-copy fence wait. This runs under the PGRAPH lock; the previous
presentation on the shared queue delayed guest execution. The shared-queue fallback now lets
RetroArch repeat unchanged frames. A dedicated queue fills every current sync
index so frontend duplication cannot extend an older image's lifetime across
independently executing writes. It still redraws after surface uploads,
changed surfaces/draw times/display uniforms, overlay updates, and output image
recreation. New frames on a dedicated queue use a producer semaphore handoff.

## Local Evidence — 2026-10-24

Environment: Apple M4, macOS 26.5.1, ARM64 core, RetroArch 1.22.2 `fce35fc7`,
MoltenVK 1.4.2 with local drawable-ownership marker 1. BIOS, disk, EEPROM, ISO and
128 MB debug-BIOS setup follow [OpenGL Validation](libretro-macos-opengl.md).
Configuration, logs, saves and support-file writes are isolated under `build/`.
Source media remains untouched. `command.json` records the exact core SHA-256;
logs report official version and the fork's HEAD. Local uncommitted changes are
identified by the binary hash rather than claiming they are already in HEAD.

| Probe | Result |
| --- | --- |
| Initial integration | Shader-compiler crash, subsequently fixed by restricting exports. |
| Export isolation, 2,400 frontend frames | Exit 0, 45.28 s, SEGA logo rendered. |
| Export isolation, 6,000 frontend frames | Exit 0, 103.75 s, boot still slow. |
| Updated dependencies, 12,000 frontend frames | Exit 0, 203.63 s, OutRun 2 title and Start prompt. |
| Repeated-frame reuse, 6,000 frontend frames | Exit 0, 104.22 s, final PNG shows the car and road in 3D attract mode with Start prompt. |
| Final build OpenGL regression, 2,400 frontend frames | Exit 0, 44.39 s, OutRun 2 title and Start prompt. |
| Final build Vulkan, 6,000 frontend frames | Exit 0, 103.17 s, final PNG shows the 3D road scene and Start prompt in attract mode. |

These are frontend frames, not a count of distinct game frames. In the first
working implementation renderer waits averaged about 16.6 ms per run. With
repeated-frame reuse, recent 300-run windows showed roughly 0.5–3.6 ms waiting
for the renderer. Presentation still showed spikes around 65 ms and distinct
frame counts varied; this is not proof of full-speed gameplay or a matched-scene
performance benchmark.

The six upstream vertex-carryover tests and all twenty upstream PTIMER tests
pass with the updated `nv2a_vsh_cpu` dependency. SPIRV-Reflect and nv2a_vsh_cpu
sources match the revisions in their merged wrap files. SDL uses the existing
system dependency rather than the wrap fallback. The build and packaged core
are ad-hoc signed; bundled MacPorts dependencies require macOS 14 or later,
regardless of the lower deployment flag in the inherited local build recipe.

Private logs, PNGs, SHA-256 manifests and the sample are retained in
`build/vulkan-integration/`. The bounded runner supports `--renderer vulkan` or
`--renderer opengl`; its default remains OpenGL. Diagnostic controls include
`--vsync on|off`, `--video-scale 1|2`, `--swapchain-images 2|3`, and a five-second
macOS process sample at `--sample-at SECONDS`. These affect only the generated
private runner configuration, not normal core defaults.

The subsequent controlled presentation investigation is documented in
[Vulkan Presentation Bottleneck](libretro-vulkan-presentation.md).

The follow-up uses a dedicated renderer queue and synchronous MoltenVK host
submission. It measures about 60 guest flips/s with frontend VSync enabled,
with both two and three swapchain images. See the presentation report for
same-driver controls and the distinction from frontend Hz. Its separate
synchronization-validation run completes with zero API errors/hazards after
correcting compute-buffer usage and explicit shader/portability features;
unused fragment-output warnings remain.

## Remaining Limits

The no-geometry-shader depth path uses hardware Z for Z-buffer draws and
reciprocal W with neutral host Z for W-buffer draws. These remain
approximations of NV2A interpolation and slope/depth bias. Flat
shading across quads and non-filled strip/fan polygon modes also need dedicated
coverage. Do not describe the Metal path as full NV2A rendering parity.

### OutRun 2 Horizon-Band Investigation

The user reports a translucent dark-grey area starting at a line near the
horizon and extending over buildings. The retained RetroArch log shows both
recorded content loads using Vulkan: selecting OpenGL after starting the
resident Vulkan machine does not change that machine's renderer. A fresh
frontend process is required. A separate fresh OpenGL core probe completes
4,200 frames and normal exit, but its attract image is not the reported scene.

The user's MJR snapshot `vm-20261024220248` (24 October 2026, 22:02:48 Europe/Rome)
contains the reported race scene. Its HDD was copied only after the emulator
closed, hashed and retained read-only; source HDD/EEPROM remain untouched.
Existing standalone builds were tested in isolated app/media copies under
the dedicated `/Users/andrea/dev/xemu/.local/shadows-or2/` directory.

MJR `74a27096ed6b` loads that snapshot in Vulkan and OpenGL. After 120 vblanks,
both report `picture-stale=false` and a picture from the current load, so the
images are newly rendered rather than merely the snapshot's saved framebuffer.
Vulkan shows the dark band/occluded building details; OpenGL shows the building
detail and scene without that band. Both exit normally. The views come from
the same snapshot with a short advance; different guest execution progress
means they are not identical frame/pixel comparisons. The initial attempt
without MJR's existing snapshot-pad restoration helper failed on USB topology;
the successful private runs set `XEMU_LOADVM_ADD_PADS=1`.

The official `25e1ee2fd5` OpenGL attempt aborts while loading this MJR snapshot
with a DSP program-memory assertion (`read_memory_p`), so it supplies no valid
same-scene renderer comparison. This is not classified as an OpenGL defect.
The earlier user-reported MJR OpenGL crash is not reproduced in the bounded
snapshot test and remains separate.

The no-geometry-shader depth fallback was the initial suspect. A W-depth/bias
candidate and a cumulative unnormalized-Z candidate did not remove the band
and were withdrawn from the active patch. They remain independent accuracy
proposals, not disproven corrections; see
[Depth Candidates: Reassessment](libretro-vulkan-depth-candidates.md) for the
preserved patches, mathematical justification, evidence and acceptance checks.
The supplied state became a reproduction fixture for the correction below.
Core evidence, source comparisons and snapshot-input hashes are in
`build/shadows-or2/`; the successful standalone captures are in
`michaeljsr-vulkan-fresh/` and `michaeljsr-opengl-fresh/` beneath the dedicated
standalone evidence directory.

### Confirmed Packed-Depth Border Correction

The core now loads the copied MJR race snapshot for a direct before/after
comparison. The test copy's final snapshot tag is renamed to `libretro_save`
without changing its VM payload; a small raw `.state` header requests that
snapshot through the core's existing experimental API. The private frontend
probe suppresses only its automatic undo-serialize call: otherwise RetroArch
overwrites that HDD snapshot before restoring it. This diagnostic harness is
not shipped or installed. Save-state metadata remains `savestate=false`.

Two prerequisites were needed to restore the supplied state: the headless
bridge must skip the standalone's UI-metadata prefix, as its normal reader
does, and the APU worker must stay stopped during reset/restore until the
running-state callback synchronizes its DSPs. The APU condition follows the
existing MJR correction. Snapshot errors are now logged instead of discarded.
These changes do not import standalone UI or claim general cross-version
snapshot compatibility.

The decisive texture trace reports stage 3, packed depth format `0x2e`, border
color `0xffffffff`, and address values `0x00040404`/`0x00040303`. Without
`VK_EXT_custom_border_color`, the inherited integer-sampler fallback always
chooses transparent black, so samples beyond the shadow map compare against
zero depth and darken scenery outside its intended coverage. Standard Vulkan
integer opaque white is the integer value 1; after extracting the most
significant 24 bits it is also zero, so selecting that enum is insufficient.
See [Vulkan Border Replacement](https://docs.vulkan.org/spec/latest/chapters/textures.html#textures-border-replacement).

When native custom borders are unavailable, the fragment shader now applies
the guest border depth outside the U/V axes actually configured for border
addressing. Projective and rectangle coordinates use the same normalization
as the texture lookup. Border colors and axis masks are uniforms, so register
changes do not create extra shader variants. OpenGL and Vulkan devices with
the native feature retain their sampling paths; there is no game-name or
macOS-specific workaround. That border correction did not change the earlier
depth approximation. The later scoped W-buffer update is documented in
[Restricted W-Only Implementation](libretro-vulkan-depth-candidates.md#restricted-w-only-implementation-2026-10-25).

The corrected Vulkan race image has no horizon band and restores the building
details; the OpenGL core also renders the supplied scene correctly. The race
views follow the same state but not identical elapsed guest frames, so this is
visual defect confirmation rather than pixel equality. All successful probes
unload and exit normally. Before/after PNGs and exact hashes are retained under
`build/shadows-or2/fix/loader-baseline-vulkan/` and `border-candidate/`.

Validation of the race exposed another portability requirement: a nonzero
sampler LOD bias is invalid when `samplerMipLodBias` is not enabled. The shared
device now enables that feature when supported and reports its enabled state
to the renderer; unsupported bias is forced to zero. Native Vulkan support
and the device's numeric bias limit are still respected. This addresses
`VUID-VkSamplerCreateInfo-samplerMipLodBias-04467`, not the shadow-band cause.

In 30-second direct-provider comparisons, excluding the first two five-second
windows, the baseline median changed-picture rate is 59.8/s (58.6–59.8), and
the final candidate is 59.6/s (59.2–60.0). Initial windows include snapshot
restore and cold shader compilation and are not steady-state samples. The
validation-loader path has a different performance profile and is used for
API/synchronization checks, not this throughput comparison. This is a bounded
one-scene check, not full gameplay or all-game rendering qualification.

The final build's Khronos/synchronization-validation snapshot run completes in
18.058 seconds with zero API errors or synchronization hazards; eight
unused-output shader warnings remain. The prior LOD-bias VUID is absent.
Direct-provider Vulkan, validation-loader Vulkan and the final OpenGL snapshot
check all unload and exit with code 0 and no forced termination. Source media,
the user's MJR HDD and the external RetroArch system files are preserved.
The border-correction build's packaged core SHA-256 is
`991de76a815edceaabb695ca001507c4a1e8abd6056cb1c3227ead527abd697c`.
The local canonical RetroArch deployment has a separate backup/hash receipt;
the driver framework, global config and user core options are not changed.

The bounded OutRun 2 reset, same-process content reload and normal exit check
passes on Apple M4 with synchronous host submission; see
[Vulkan Lifecycle Validation](libretro-vulkan-lifecycle.md) for the macOS
loader correction, diagnostic reset and screenshot-widget validation limit.
Controller gameplay, physical rumble, audio quality, broader reset/reload
campaigns, long-duration stability, Intel Macs, unpatched-driver compatibility
mode and Windows/Linux runtime regressions have not been qualified.
The existing OpenGL context repair remains independent. Additional tuning needs
matched scenes and evidence before importing larger standalone renderer or CPU
optimizations.

The probes above were collected before the source changes were committed;
their binary hashes identify those tested working trees. The full official
upstream merge and subsequent Libretro adaptations are recorded separately in
this fork's history. The canonical local RetroArch core was
replaced with the validated build, with backup and hash verification recorded
in `build/shadows-or2/fix/deployment/receipt.json`.

## Experimental Acquisition-Fence Follow-Up

An isolated MoltenVK candidate restores asynchronous-host OutRun 2 cadence in
a same-binary control. See [Acquisition Fence Completion](libretro-vulkan-acquire-fence.md)
for the review patch, required driver base, lifecycle validation and limits.
The default core policy remains synchronous host submission. The user selected
that policy for this project; correctness review of the driver experiment is
deferred to the separate Xenia MoltenVK work.
