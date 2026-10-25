# Experimental MoltenVK Acquisition Fence Completion

## Result And Scope

The private acquisition-fence candidate restores approximately 60 guest flips
and changed pictures per second in OutRun 2 with asynchronous MoltenVK host
submission and frontend VSync enabled. Disabling only the candidate switch in
the same driver binary returns to approximately 20/s. This is a new local
experiment, not an optimization copied from MichaelJSr: its native Metal
presenter does not exercise RetroArch's Vulkan WSI acquisition path.

The correction is in **MoltenVK**, not the portable emulation logic. The normal
core still requests synchronous host submission, and the installed RetroArch
framework is unchanged. The preserved runnable experiment, source snapshot,
recipe, environment, logs, traces and binary hashes live under ignored
`build/acquire-fence/`. No standalone emulator was built here.

## Project Decision

Sync remains the default. The core now exposes **MoltenVK Queue Submission
(Restart)** with an explicit Async choice for compatible patched drivers.
Restart RetroArch to recreate the Vulkan instance. The existing asynchronous
GPU semaphore handoff is independent and remains in place. This document
preserves the original driver experiment and its original binaries/results;
it does not qualify every later driver build or introduce a driver dependency.

## Dependency And Lifetime Change

The tested base is MoltenVK source
`e9b7bd2e4d6f365b36d29f5f57b8d39ca92f157f`, version 1.4.2, with the Xenia
independent-queue semaphore/token/encoding, timeline, device-loss and drawable
ownership corrections used by the earlier private driver. Inputs were copied
from the separate Xenia project, which was not edited. Marker 1 alone does not
identify all these corrections.

In that base, a queued present detaches its drawable from the logical image
slot and retains the old drawable independently. A subsequent logical-image
acquisition can therefore use a different drawable without releasing the one
still queued for presentation. The deferred acquisition semaphore is ordered
through GPU commands, while its CPU fence is normally signaled by the later
physical presented callback. That distinction was measured in
[Vulkan Acquisition And Guest Cadence](libretro-vulkan-cadence.md).

The candidate changes only a **deferred acquisition's CPU fence**:

1. Under the existing availability lock, identify whether the selected
   presentation signaler was deferred.
2. Move that signaler's already tracked fence reference to the previous
   presentation command buffer's completed handler. Clear the fence pointer
   from the physical-presentation signaler, avoiding a second signal/release.
3. Signal and release the moved fence after GPU command-buffer completion.
   Keep semaphore encoding, drawable retention, actual presentation accounting
   and `VkPresentInfo`'s separate present fence behavior unchanged.

Thus the experiment uses a completed GPU dependency, not a forced signal when
`vkQueuePresentKHR` merely returns. It adds no sleep, post-present CPU wait or
queue/device idle wait. The prototype is explicitly enabled by the private
child-process environment `XEMU_MVK_ACQUIRE_FENCE_COMPLETION=1`; without that
variable, the same binary retains the base behavior. No new Core Option or
`xemu.toml` setting is introduced.

The Vulkan acquisition contract requires an application to wait for its
acquisition semaphore or fence before accessing the image. These probes
establish behavior on the patched drawable-slot implementation; they are not
a general WSI conformance proof. See the [Khronos WSI specification](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html).

## Same-Binary Control — 2026-10-24

Apple M4, macOS 26.5.1, RetroArch 1.22.2 `fce35fc7`, the existing private
OutRun 2 ISO/system setup with 128 MB guest memory. Both runs retain VSync,
audio synchronization, renderer queue family 1 and frontend family 0. Both
execute 6,000 frontend frames with a 180-second bound and exit normally.
The core is the previously verified asynchronous-GPU-handoff package,
SHA-256 `79ca980a0c0734d27be270c36f349c91dffc7607a3352ec0348ad84c73b0b991`.

Both runs load exactly the same observer, core and private driver. Only the
candidate environment switch changes the acquisition policy. Observer output
paths differ naturally. The observer overrides the instance's MoltenVK host
submission setting to asynchronous in both cases; the inherited core startup
sentence saying synchronous describes its original request, not the override.
No diagnostic post-present serialization switch is set.

| Metric | Candidate Off | Candidate On |
| --- | ---: | ---: |
| Guest Flips/s, Last 50 s | 20.000 | 59.999 |
| Changed Pictures/s, Last 50 s | 20.000 | 59.979 |
| Frontend Calls/s, Last 50 s | 60.000 | 59.999 |
| Acquisition Intervals < 2 ms, Ten-Second Trace | 400 | 0 |
| Acquisition Intervals > 25 ms, Ten-Second Trace | 199 | 0 |
| Maximum Acquisition Interval | 50.015 ms | 18.832 ms |
| Physical Presented-Callback Interval, Mean | 16.667 ms | 16.667 ms |
| Elapsed Time | 103.29 s | 103.97 s |

Traces cover seconds 65–75 after observer initialization, are buffered in memory
and written at exit, with no overflow. The off case's 199 complete deferred
acquisition pairs average 49.860 ms before the physical callback. The on case's
599 complete pairs average 16.610 ms: 15.746 ms to the worker's capture and
0.864 ms from capture to the GPU-completion callback. All 600 moved-fence
callbacks report `MTLCommandBufferStatusCompleted` (4). Physical presentation
still follows capture by about 49 ms; the candidate stops making CPU acquisition
wait for that later event.

These are identical-media/settings controls over fixed wall-clock windows,
not pixel-identical scene replays: the slower guest progresses through attract
mode at a different rate. The rate counters and acquisition timeline establish
the effect; the candidate's final screenshot shows the title, while the
validation probe below captures a later 3D frame. This is not interactive
controls/gameplay validation.

## Validation And Reproduction

A private diagnostic core enables asynchronous host submission without the
observer and invokes the existing runtime Core Options application path at
runs 1,800/2,400/3,000, requesting scales 2/1/2. With the candidate enabled,
Khronos API/synchronization validation and two confirmed swapchain images,
all changes complete, the 3D attract sequence continues and unload joins the
emulation thread. The 3,600-frame run exits normally in 90.82 seconds with zero
Vulkan errors/hazards. The ten previously recorded unused-fragment-output
warnings remain. No diagnostic code remains in the normal core source; it was
restored and rebuilt, and the working package remains byte-for-byte preserved.

The review patch is
[`patches/moltenvk-acquire-fence-completion.patch`](patches/moltenvk-acquire-fence-completion.patch).
It excludes trace hooks and absolute instrumentation paths. It applies **after**
the required Xenia base corrections and drawable ownership patch, not directly
to pristine upstream MoltenVK. Its expected base `MVKImage.mm` SHA-256 is
`7a5c832c6b47a550a258a46198bd218d33a362470d21e34b067e8bc74c3eba86`;
the header SHA-256 is
`5d81fd2be0fb2b6d5c09b6b06d30590c150da3e1cf146c9dbe7973ceff4baf53`.
`git apply --check` passes against that prepared input. A clean driver built from exactly those patched image/header sources, without
internal observation hooks, was separately runtime tested. With asynchronous
host submission, VSync, two swapchain images and Khronos validation it completes
3,600 frames in 63.64 seconds, exits normally and joins the emulation thread.
Its last 25 seconds average 60.00 guest flips/s and 60.00 changed pictures/s;
zero Vulkan errors/hazards and the same ten shader warnings are reported.
Recipe and dependency pins are retained privately. The observed A/B driver is
preserved as `build/acquire-fence/MoltenVK-observed`, while the clean binary is
`build/acquire-fence/driver/MoltenVK`.

Before broader adoption, check fence reset/reuse patterns beyond RetroArch's
fresh acquire fences, semaphore-only and combined acquisition, other present
modes, shared-queue consumers, device loss and other applications/platforms.
The existing base's pre-signaler re-signaling workaround is unchanged and has
not been audited for all those cases. Do not infer general driver correctness
or an upstream-ready patch from this bounded game result.
