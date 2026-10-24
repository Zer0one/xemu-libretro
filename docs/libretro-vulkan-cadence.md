# Vulkan Acquisition And Guest Cadence

## Question And Scope

The VSync-on correction uses a dedicated renderer queue and synchronous
MoltenVK host submission. It restores about 60 changed pictures/s in the local
OutRun 2 attract sequence. That result alone did not explain why asynchronous
submission was slower. This follow-up records the missing timing evidence.

All probes use Apple M4, macOS 26.5.1, RetroArch 1.22.2 `fce35fc7`, the same
isolated OutRun 2 media/system setup and 128 MB guest memory. VSync stays enabled.
No standalone emulator is built or launched here. These are local frontend and
driver diagnostics, not Windows/Linux or full-catalog results.

## Observation Method

Private instrumentation records a shared monotonic timeline for:

- `retro_run`, scheduling/executing its display-update bottom half, raising and
  acknowledging the guest vblank interrupt, and guest scanout address writes.
- Vulkan submissions, presentation, acquisition and CPU fence waits.
- Metal drawable acquisition, `present` and actual presented callbacks.

A private dispatch library forwards Vulkan calls to the exact provider. The
first pair uses the already installed, patched MoltenVK 1.4.2, SHA-256
`6c511b765b68df9043b05fe6cdaacb78ce256115aab887a2b9d9c582143aa22b`.
Metal method hooks observe the drawable calls and add a timestamp callback;
they do not change the drawable or force its presentation.

A second pair adds logging inside a separately built driver from the same
pinned `e9b7bd2e4d6f365b36d29f5f57b8d39ca92f157f` source and local Xenia
corrections used by the installed driver. Only the private copy is loaded.
The added driver events identify immediate/deferred acquisition, the fence
selected by `getPresentationSignaler`, and its eventual signal. The driver
recipe and source inputs are read from the Xenia project without editing it.

Each probe runs 6,000 frontend calls with a 180-second bound. Events are buffered
in memory only during seconds 65–75 after the observer loads, then written on
normal process exit. No per-event file writes occur in the measured interval.
No recorded buffer overflow occurs. Window boundaries explain incomplete
begin/end pairs; statistics use complete pairs only.

Private files are retained under ignored `build/presentation-trace/`:
`observe.mm`, `instrumentation.patch`, `driver-instrumentation.patch`, the
configure command, source/dependency references, per-case environment and binary
hashes, raw events, CSV timelines, analysis scripts and results. The diagnostic
core selects its submission mode through a private environment control; its
`[xemu trace] synchronous override=0/1` line records the actual setting. The
inherited production log sentence still says synchronous and must not be used
to classify these diagnostic runs.

## Installed Driver Trace

The synchronous observer control preserves the measured behavior: 600 frontend
calls in ten seconds, 600 vblank raises, 600 acknowledgements and 600 guest
scanout flips. All 599 complete intervals between calls lie between 8 and 25 ms;
their mean is 16.667 ms and maximum 17.116 ms.

The asynchronous observer probe also records 600 calls, but 436 of the 599
intervals are below 2 ms and 149 exceed 25 ms. Median interval is 0.645 ms and
maximum is 65.211 ms. During each of those 149 long acquisition-fence waits,
Metal completes four actual presentations. Its presented-callback interval
remains approximately 16.667 ms. A single `nextDrawable` call is not taking
65 ms: its maximum observed duration is 17.085 ms.

The guest receives 600 vblank raises, acknowledges 546, and produces 157 scanout
flips in that window. Fifty-four raises find the interrupt already pending.
This proves some level-triggered interrupt coalescing, but it cannot explain
all the missing pictures. Do not equate every skipped picture with a lost IRQ.

## Where The Long Wait Comes From

The private driver trace reproduces the same approximately 15 FPS behavior.
In ten seconds it records 450 immediate acquisitions and 150 deferred ones.
The relevant source path is
`MVKPresentableSwapchainImage::acquireAndSignalWhenAvailable`:

1. When the logical image is already marked available, the acquisition fence
   is signaled immediately on the calling thread.
2. Otherwise the signaler is stored in `_availabilitySignalers`.
3. The presentation worker later removes it in `getPresentationSignaler` and
   captures it for that image's presentation.
4. `endPresentation`, called from the actual drawable-presented callback,
   signals that deferred acquisition fence. RetroArch is waiting on that fence
   in `vulkan_acquire_next_image`.

For 149 complete deferred pairs, the mean time breaks down as follows:

| Stage | Mean |
| --- | ---: |
| Deferred acquisition to worker presentation capture | 14.952 ms |
| Capture to actual presentation completion | 49.068 ms |
| Total deferred acquisition wait | 64.020 ms |

The physical presentation stream continues at 60 Hz throughout that wait.
After it finishes, the available-image path permits three rapid acquisitions,
then the next deferred acquisition holds the caller again. Thus the frontend
averages 60 calls/s while delivering them in bursts separated by about four
refresh periods. Since this core schedules guest vblank from those calls, the
guest also receives irregular timing.

These acquisition-signaling branches are retained from the pinned upstream
MoltenVK source; the Xenia ownership correction does not change their placement.
No new unpatched-driver runtime comparison is made here. This identifies the
observed frontend/driver/core interaction, not a general Vulkan conformance bug
or evidence that the Xenia patch caused it.

## Counterfactual Control

The control keeps MoltenVK asynchronous and inserts a bounded, queue-ordered
fence wait after `vkQueuePresentKHR`, before the frontend's next acquisition.
An empty submission on that queue supplies the fence; its wait is limited to
one second. This waits for submitted work without forcing any existing fence
to signal, changing guest clocks, disabling VSync or adding a pacing sleep.
It also ensures the presentation worker has processed the preceding present.
It is a diagnostic serialization control, not a proposed production feature.

With this control the private driver records 600 immediate acquisitions and
zero deferred ones. All 600 diagnostic waits complete successfully. The same
ten-second window has 600 vblank raises, 600 acknowledgements and 600 guest
scanout flips. Mean call interval is 16.667 ms, median 16.668 ms and maximum
18.462 ms. Metal continues presenting at approximately 60 Hz.

This removes the long deferred-acquisition path and restores guest progress
while leaving the driver asynchronous. It supports the acquisition/presentation
ordering and resulting burst cadence as the cause of the measured loss, rather
than a general inability of asynchronous MoltenVK to render this game at 60 FPS.

The no-wait case was then repeated using the exact same core, observer and
private driver binaries as the wait control. Both sessions exit 0 and reap their process. The no-wait final image shows
the 3D attract sequence; visible output is separate from the counter measurements. Only the diagnostic wait switch
changes behavior; output/trace paths are naturally different.

| Same-Binary Asynchronous Pair, Ten-Second Trace | No Added Wait | Present-Ordered Wait |
| --- | ---: | ---: |
| Immediate acquisitions | 450 | 600 |
| Deferred acquisitions | 150 | 0 |
| Call intervals below 2 ms | 440 | 0 |
| Call intervals above 25 ms | 149 | 0 |
| Vblank raises | 600 | 600 |
| Vblank acknowledgements | 474 | 600 |
| Scanout flips | 157 | 600 |

The repeat's complete deferred pairs average 63.818 ms total: 14.747 ms before
presentation capture and 49.071 ms from capture to presentation completion.
Its last 25-second log windows remain around 15 guest flips/s; the wait control
remains around 60. The interrupt-coalescing count varies across slow probes
while the burst cadence and slow guest progress persist. That further cautions
against attributing the entire loss to pending IRQs alone.

## What Remains Unproven

The trace does not reverse-engineer OutRun 2's interrupt handler, frame scheduler
or timer tests. It therefore does not assign each of the remaining missed
pictures to a specific guest decision. It establishes irregular delivery,
measured interrupt coalescing, and recovery when the acquisition sequence is
serialized. The exact guest-side response beyond those counters remains open.

A general driver-policy change or an alternative core timing architecture would
need its own design, lifecycle checks and tests across other workloads. The
production core retains the already tested synchronous MoltenVK policy and
dedicated queue. All temporary core source edits are restored, the working
packaged core is preserved byte-for-byte, and the installed RetroArch framework
is not changed. No commit, push or installed-core replacement is made.

## Experimental Correction

The subsequent [acquisition-fence experiment](libretro-vulkan-acquire-fence.md)
moves deferred CPU acquisition completion to the predecessor GPU completion
callback on the existing detached-drawable implementation. It restores about
60 changed pictures/s in the local asynchronous-host control without the
diagnostic post-present CPU wait. This is a bounded driver experiment; the
normal core and installed framework policy remain unchanged.
