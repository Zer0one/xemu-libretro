# Vulkan Presentation Bottleneck

## Scope And Measurement

This core targets multiple platforms. The MichaelJSr standalone fork is a
reference for applicable renderer improvements, not a replacement architecture
or a reason to impose Apple-specific defaults on other Vulkan implementations.
Generic Libretro synchronization changes and MoltenVK-specific driver changes
must be reviewed and tested separately.

The 2026-10-24 investigation uses Apple M4, macOS 26.5.1, RetroArch 1.22.2
`fce35fc7`, and its already installed patched MoltenVK 1.4.2. All probes load the
same MoltenVK binary image UUID `7DA4B241-B21A-332E-900C-5DB7095F86EF` and
the same ARM64 core, SHA-256
`daf5ef97f5972a6d2c1207922a2f5f68afc1d929dbe290df71323544b4a551bc`,
from the uncommitted upstream/Vulkan integration. No emulator optimization or
shader change separates the probes. BIOS, memory size, internal scale, content,
frontend video scale, audio synchronization and driver selection stay identical.
Only the named frontend VSync/swapchain setting changes in each private config.

The core now logs three rates over measured wall-clock windows:

- **Guest flips:** writes to `NV_PCRTC_START`, matching the event counted by
  MichaelJSr's `query-xemu-display.flips`. Some boot code writes more than once
  per picture, so the count is not universally equal to unique rendered frames.
- **Updated pictures:** newly sampled surfaces/draw timestamps handed to the
  frontend. This is the earlier log's `guest frames` counter.
- **Frontend:** calls to `retro_run` per second, including repeated pictures.

The existing MJR probe at `74a27096ed6b` recorded 3,610 guest scanout flips in
60.1693 seconds between its two samples, approximately 60 flips/s. This supplies
a reference rate, not a matched-scene comparison of identical counters for all
rendering stages. The core's added counter closes that instrumentation gap. In
the sampled 3D interval, core guest flips and updated pictures agree closely.

## Controlled Results

Every probe runs for 6,000 frontend frames with a 180-second bound. Process
sampling is scheduled 75 seconds after launch for five seconds. Complete JSON
results, raw logs, screenshots, config files and symbolized samples are retained
under ignored `build/performance-vulkan/`.

| Private Probe | Guest Flips/s, Last 25 s | Updated Pictures/s, Last 25 s | Frontend Calls/s | Exit |
| --- | ---: | ---: | ---: | ---: |
| VSync On, 3 Swapchain Images | 19.97 | 19.85 | 60.04 | 0 |
| VSync Off, 3 Swapchain Images | 59.22 | 59.22 | 60.30 | 0 |
| VSync On, 2 Swapchain Images | 24.97 | 25.01 | 59.93 | 0 |

With VSync on, reducing the requested swapchain image count from three to two
raises the later updated-picture rate from about 20 to 25/s. The log confirms
two actual swapchain images. This independently supports a drawable/queue
availability bottleneck, while leaving a substantial residual loss.

The VSync-off case is a diagnostic control, not a default change or a completed
presentation fix. Audio synchronization continues pacing the frontend. The
renderer's CPU-side display wait is around 0.5–0.6 ms/run and the video callback
around 0.01 ms/run in its later windows; audio submission absorbs the remaining
frame period. The control proves this core/build can sustain about 59 updated
pictures/s in the tested game, rather than having an inherent 18–20 FPS render
throughput ceiling.

The same wall-clock windows contain different points in the attract sequence
because the slow case advances less game time. These results localize a large
presentation-dependent loss; they are not a matched-scene ranking of smaller
renderer optimizations or a guarantee of full-catalog performance.

## Sampled Blocking Chain

With VSync on and three images, the five-second sample contains 3,605 samples
of the emulated GPU's PFIFO thread:

- 2,098 samples wait in `pgraph_vk_finish` after guest `FLIP_STALL`.
- 926 samples wait for a texture-upload submission fence.
- 193 samples wait for the display-copy submission fence.

All are CPU waits for the shared Vulkan queue. Meanwhile its MoltenVK worker
has 3,116 samples inside `MVKPresentableSwapchainImage::getCAMetalDrawable`,
including 3,112 blocked in `CAMetalLayerPrivateNextDrawableLocked` and a dispatch
semaphore wait. That is approximately 86% of the full sample, not GPU utilization.
The call chain is RetroArch's swapchain render pass obtaining its Metal drawable.
The frontend itself also samples in `vulkan_acquire_next_image` waiting for its
acquisition fence.

With VSync off, only one sampled `nextDrawable` call remains, and it is not in
the semaphore-wait path. PFIFO instead has 3,304 of 3,687 samples in its normal
condition wait for work, rather than being continuously held in GPU fences.
This supports the blocking chain:

1. The frontend's swapchain rendering needs a Metal drawable.
2. Obtaining that drawable blocks the serial MoltenVK submission worker.
3. Core rendering and uploads behind it cannot complete on the shared queue.
4. PFIFO waits for their fences, slowing the guest even though RetroArch still
   calls the core at roughly 60 Hz.

This is the same class of queue/drawable contention investigated for Xenia.
The Xenia marker-based asynchronous-host policy is active in these probes; its
presence alone does not eliminate this workload's drawable availability problem.
The data identifies the blocked path. It does not yet prove which retain,
present-mode behavior or frontend/driver lifetime rule makes drawables late.

## Implementation Boundary

The initial diagnostic pass left rendering, queue selection, GPU-completed
handoff and VSync behavior unchanged. Its added code consisted of a
Libretro-only atomic guest-flip counter and timing-log rates. The runner gains
explicit private diagnostic controls. The baseline binary is retained under
`build/performance-vulkan/baseline/`.

A proper correction should preserve normal VSync and frontend-owned pacing,
correct frame-image ownership and unload behavior. Potential common Vulkan
handoff/pipelining improvements belong in the generic backend. Any drawable or
host-submission workaround belongs only to the detected MoltenVK path. Reusing
MJR's flight slots, deferred reports or Apple presenter without accounting for
the Libretro device/queue ownership contract would not establish correctness.
Windows/Linux behavior has not been benchmarked by these macOS probes.

## Dedicated Queue Correction

The follow-up retains the installed driver. Its SHA-256 is
`6c511b765b68df9043b05fe6cdaacb78ce256115aab887a2b9d9c582143aa22b`,
matching the Xenia timeline-fix installation receipt. Marker 1 alone is not a
version for all its later independent-queue corrections.

The core now prefers a second graphics/compute queue in the frontend family,
then a suitable separate family. Apple M4 exposes only one queue per family;
the tested renderer uses family 1 and RetroArch uses family 0. This queue
selection is capability based and shared by Vulkan platforms. If no dedicated
queue exists, the locked shared-queue path remains available.

The handoff explicitly accounts for independent execution:

- Create command pools for the selected renderer family.
- Create frame images with concurrent sharing when the two families differ.
- Signal a per-index binary semaphore with the display-copy submission, and
  supply it to the frontend through `set_image` for the producer dependency.
- The initial fix retained the CPU wait for completed rendering. The
  [asynchronous display follow-up](libretro-vulkan-async-handoff.md) replaces
  that immediate wait on dedicated queues with per-index recording resources
  and fences for safe reuse. The measurements below predate that follow-up.
- Call `wait_sync_index` before writing the index again. RetroArch implements
  this as a no-op because it already waits the index in its swap-buffer path;
  calling the contract remains necessary for other frontends.
- Fill the current index every run on a dedicated queue. Repeating an older
  index could extend frontend reads beyond the fence for that index; same-queue
  ordering previously serialized those reads and writes. Changed-picture
  diagnostics still count actual guest surface/draw-time changes.

MoltenVK requests synchronous host submission per instance, including when the
Xenia marker is present. This matches MoltenVK's default. It changes no other
Vulkan implementation, global environment, installed framework or RetroArch
VSync setting. The marker remains diagnostic metadata. The asynchronous policy
originally borrowed from Xenia is superseded for this core by these controls:

| VSync-On Probe, 6,000 Frontend Frames | Guest Flips/s, Last 25 s | Updated Pictures/s | Exit |
| --- | ---: | ---: | ---: |
| Shared Queue, Asynchronous | 19.97 | 19.85 | 0 |
| Shared Queue, Synchronous | 41.24 | 40.60 | 0 |
| Dedicated Queue, Asynchronous | 15.00 | 15.00 | 0 |
| Dedicated Queue, Synchronous, 3 Images | 60.00 | 59.92 | 0 |
| Dedicated Queue, Synchronous, 2 Images | 60.00 | 59.96 | 0 |

The dedicated/synchronous probe's last 50 seconds average 59.98 guest flips/s
and 59.94 changed pictures/s. Its sample has 3,146/3,718 PFIFO samples in the
normal work-condition wait and 199 in the guest flip-fence wait. Drawable waits
still occur on the frontend thread, but they no longer block the emulated GPU's
submission queue. Its recent run periods are approximately 16.67 ms with maxima
around 17.1 ms, apart from an isolated 40.3 ms spike.

The dedicated/asynchronous control also removes PFIFO queue starvation, yet
samples the frontend waiting for acquisition while its worker blocks in
`nextDrawable`. Recent 300-run windows have 65.2 ms maximum run periods despite
a 16.67 ms average. This is consistent with irregular delivery of frontend-driven
vblank, not insufficient offscreen renderer throughput. The exact driver/
compositor scheduling mechanism behind those intervals is not established.
Synchronous submission plus queue isolation corrects the measured behavior
without changing guest CPU timing or adding a core sleep.

These results establish the local OutRun 2 attract-sequence correction, not
full-catalog performance, physical-controller gameplay or Windows/Linux runtime
validation. The earlier no-VSync probe remains a throughput control, not the
chosen solution. No standalone emulator build or installed-core replacement is
part of this change.

## Vulkan Validation Follow-Up

A separate 3,600-frame session loads the same installed MoltenVK through the
local Vulkan loader and injects `VK_LAYER_KHRONOS_validation` with synchronization
validation. All overrides are child-process environment variables, recorded in
`build/performance-vulkan/vsync-on-separate-validation/environment.json`.
This is a correctness probe, excluded from performance comparisons.

It found three API declarations requiring correction, in addition to the queue
work: the compute-output buffer is also a transfer source, portability texture
view swizzles must be enabled explicitly, and SPIR-V 1.6's helper-invocation
demotion requires a device feature. The core now queries/enables the supported
demotion feature on Vulkan 1.3 devices and the required portability swizzle
feature; shader generation falls back to SPIR-V 1.5 if demotion is not enabled.
The compute-output buffer now includes `TRANSFER_SRC` usage. These changes are
feature/resource correctness fixes, not claimed FPS optimizations.

The corrected validation repeat completes all 3,600 frontend frames in 63.45 s,
including content unload and renderer parking, with exit 0 and the process
reaped. The loader log confirms the validation layer and exact installed driver;
`VK_LAYER_VALIDATE_SYNC=1` enables synchronization checks. It reports zero API
errors and zero synchronization hazards. Ten emitted warnings describe unused
fragment color output in passes without a color attachment; this probe does not
claim a warning-free or Vulkan-conformant renderer. The original failing run is
retained separately. No missing/swapped binary-semaphore signal or queue-family
ownership error is reported in the exercised corrected path.

The final production-policy build after those API corrections is SHA-256
`063c2977e5fde56a667892b00b427285348d13aa4ae0a52a6576d7cc44fcf39f`. Its 6,000-frame
VSync-on, three-image repeat exits 0 in 103.23 s, with 60.00 guest flips/s
and 59.88 changed pictures/s in the last 25 seconds. The process is reaped.
The earlier two-image check precedes only these feature/resource declarations;
it is not mislabeled as the final binary. Per-case hashes and results are in
`build/performance-vulkan/presentation-fix-manifest.json`.

## Cadence Trace Follow-Up

The subsequent [Acquisition And Guest Cadence](libretro-vulkan-cadence.md)
trace distinguishes a 60 Hz physical presentation stream from burst-delivered
frontend calls, identifies the immediate/deferred acquisition-fence branches,
and tests a diagnostic serialization control while keeping MoltenVK asynchronous.
It supersedes the earlier uncertainty about where the 65 ms wait accumulates;
OutRun 2's exact guest-side handling of irregular ticks remains unproven.
