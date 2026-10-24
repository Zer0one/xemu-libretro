# Asynchronous Vulkan Display Handoff

## Two Independent Policies

At MichaelJSr `74a27096ed6bd7b5ad6d83ed7fd55230f9e98cb8` (official upstream
`25e1ee2fd5`), the standalone enables asynchronous MoltenVK host queue submission
in `ui/xemu.c`. Its native Metal presenter waits for the Vulkan compositor's
shared event on the GPU and commits its presentation command buffer without an
immediate CPU completion wait. Its compositor command buffers and fences form
a ring; a slot is waited before it is recorded again.

These are separate policies:

1. **Display handoff:** the CPU can publish a submitted image while the
   frontend's GPU waits for the producer. The ring/lifetime principle applies
   to the generic Libretro Vulkan backend.
2. **MoltenVK host submission:** Vulkan calls return before a worker encodes
   their Metal commands. This also affects RetroArch's swapchain acquisition,
   whose deferred CPU fence caused the cadence loss described in
   [Vulkan Presentation Cadence](libretro-vulkan-cadence.md).

This change implements the first policy on dedicated renderer queues. It keeps
synchronous MoltenVK host submission. It does not transplant the standalone
window, Metal presenter, MetalFX, interpolation, or its larger multi-flight
main-renderer/buffer architecture into the core.

## Libretro Adaptation

Each frontend sync index owns a compositor command buffer, descriptor set and
producer completion fence, in addition to its existing image and binary ready
semaphore. Before recording the slot again, wait its pending producer fence.
Before writing the frame image, the frontend's `wait_sync_index` contract also
protects previous consumer work. These are different dependencies.

Submit the compositor and display copy on the dedicated renderer queue,
signaling its per-index ready semaphore and completion fence. Return without
waiting for that submission on the CPU. Pass the semaphore to Libretro
`set_image`, so the frontend's GPU waits before sampling the image. Per-index
descriptors remain unchanged while their submission is pending. A dependency
orders the shared intermediate display image's previous copy before its next
render, even though its contents are discarded.

Before display-image recreation, overlay-resource replacement, guest-surface
destruction, renderer finish or display teardown, drain pending producer work.
The waits use producer fences with a five-second bound, outside the frontend's
queue lock. These submissions never wait for a frontend-driven semaphore.
No queue/device idle wait is introduced. The existing locked shared-queue
fallback retains its synchronous compositor path and repeated-frame reuse.

## Local Evidence — 2026-10-24

Apple M4, macOS 26.5.1, RetroArch 1.22.2 `fce35fc7`, already installed patched
MoltenVK 1.4.2. Private configs retain VSync and audio synchronization. OutRun 2
uses the existing private system-file copies and 128 MB debug-BIOS setup.
The packaged candidate SHA-256 is
`79ca980a0c0734d27be270c36f349c91dffc7607a3352ec0348ad84c73b0b991`.
Evidence is under ignored `build/async-handoff/`; the previous working package
is preserved as `baseline.dylib` (SHA-256
`063c2977e5fde56a667892b00b427285348d13aa4ae0a52a6576d7cc44fcf39f`).

The three-image Vulkan validation run completes 3,600 frontend frames in
64.07 seconds, exits normally, joins the emulation thread and captures the 3D
attract sequence. Khronos API and synchronization validation report zero
errors/hazards. Ten existing `Undefined-Value-ShaderOutputNotConsumed` warnings
remain. Its last 25 seconds average 60.00 guest flips/s and 59.96 changed
pictures/s.

The 6,000-frame handoff run completes in 103.69 seconds, exits normally and
averages 60.00 guest flips/s and 60.00 changed pictures/s over its last 25
seconds (last 50 seconds: 60.001 and 59.961). It uses an observer forwarding
to the exact installed framework, logging only seconds 65–75 into a memory
buffer and writing the trace at exit; no event overflow occurs.

In that ten-second window, all 600 display producer submissions are handed to
the frontend before the CPU first waits their completion fence. The mean gap
between submission return and the first CPU wait is 3.912 ms. By then, the
producer fence wait averages 0.000289 ms. In the previous synchronous-handoff
trace, the CPU immediately waited all 600 producer fences, averaging 1.152 ms
per wait. These are producer-fence measurements, not all renderer waits and
not an FPS speedup: the working reference was already about 60 FPS. The older
trace contains additional core instrumentation, so the latency comparison is
supporting evidence, not a precise end-to-end performance benchmark.

A matched control runs the **same candidate core, observer and installed
framework** with only its private per-instance host submission setting changed
to asynchronous. It completes 6,000 frontend frames in 103.43 seconds and exits
normally, but both guest flips and changed pictures average 20.00/s over the
last 50 seconds while frontend calls average 60.00/s. The trace still contains
host fence waits up to 49.98 ms while physical drawable acquisition remains
below 17.12 ms. The new GPU handoff therefore does not fix the frontend's
asynchronous-host cadence issue. The candidate keeps the synchronous-host
policy, and no installed core or framework is replaced.

A second validation run uses two confirmed swapchain images and completes
3,600 frames in 63.47 seconds, with zero API/synchronization errors and the
same ten shader warnings. Its last 25 seconds average 60.00 guest flips/s and
60.00 changed pictures/s.

A separately preserved diagnostic core triggers `update_variables` at frontend
runs 1,800, 2,400 and 3,000, requesting scales 2, 1 and 2 respectively. This
exercises the runtime Core Options application path, including the increased
maximum's `SET_SYSTEM_AV_INFO` video reinitialization. With two swapchain images
and validation enabled, all three transitions complete, the 3D sequence
continues and content unload joins the emulation thread. The run exits normally
after 3,600 frames in 90.76 seconds with zero API/synchronization errors and the
same ten shader warnings. This is a resource-lifetime regression probe, not a
matched performance benchmark or a menu-operation test. Its patch and binaries
are retained separately; the diagnostic source was restored byte-for-byte and
the normal source rebuilt. The packaged candidate remains unchanged.

## Limits

This is bounded attract-mode evidence on one macOS GPU, not interactive
controls/gameplay proof or cross-platform validation. Main rendering, uploads
and other auxiliary operations retain upstream's blocking submission model.
Full asynchronous MoltenVK host submission remains a separate problem.

The later [acquisition-fence experiment](libretro-vulkan-acquire-fence.md)
addresses that separate host-asynchronous limitation in a private driver.
