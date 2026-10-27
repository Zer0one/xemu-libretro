# MJR Adaptations

## Sources And Scope

Targeted adaptations above the WizzardSK `252a7accfb` integration, reviewed
against MichaelJSr/xemu-macos `237d34f193061c9f25a6ab0c380e946d1982e526`
(2026-10-25). The official xemu base remains `0.8.138-3` (`25e1ee2fd5`).

| Source Commit | Adapted Behavior |
|---|---|
| [`858e2af19f`](https://github.com/MichaelJSr/xemu-macos/commit/858e2af19f6caf40f98e522bd2e817f8fe391968) | OHCI frame writeback precedes frame interrupts; write HccaDoneHead only when delivering a completion queue. |
| [`056c25b49f`](https://github.com/MichaelJSr/xemu-macos/commit/056c25b49ff78022b9d83f9c9c8c295a07ca26d3) | NV2A reset clears interrupt enables and PGRAPH pending state, then updates the IRQ line. |
| [`bb19d1d5bd`](https://github.com/MichaelJSr/xemu-macos/commit/bb19d1d5bd929220ebf436c021f6e330a312c882) | Execute only vertex-program instructions needed for the final fog output, caching slices by program content. |
| [`438548a6fd`](https://github.com/MichaelJSr/xemu-macos/commit/438548a6fdda6785c6bd04b26ec300a935f66f5f), [`9f88f1f67d`](https://github.com/MichaelJSr/xemu-macos/commit/9f88f1f67dabf4103f248abbb9018d3d28e6a022) | Directly bind suitable host staging allocations and submit one main command buffer instead of a separate copy command buffer. |

These are source adaptations, not a wholesale standalone fork merge. No new
CPU/TCG/DSP experimental families, MetalFX, standalone presenter, UI, packaging
or CI settings are imported. No new user settings or environment-variable
switches are required. The core still follows the frontend for presentation
and pacing; it does not acquire a standalone VBlank timer.

## Platform And Lifetime Contract

USB writeback ordering, GPU reset state and fog carryover have no host-OS
conditions. The fog optimization retains the full interpreter for constant
writes or unusual register references. Its host-only content cache is bounded
at 1024 slices, belongs to the PGRAPH instance, and is released by PGRAPH
destruction. Program uploads invalidate slot lookups lazily. No host cache
pointers are serialized, and this change adds no snapshot format.

Direct Vulkan staging requires **all six** staging allocations (index, inline
vertex and uniform buffers in both rotation sets) to report `HOST_VISIBLE`,
`HOST_COHERENT` and `DEVICE_LOCAL`. This is an allocation-property test, not a
macOS or Apple GPU test. Other allocations retain the original separate device
buffers. The copy path explicitly flushes noncoherent host writes before its
transfer. Existing Vulkan/OpenGL platform availability and API requirements
are unchanged.

The direct device handles borrow their staging owners' allocations and are
never freed separately. Both sets still rotate using the donor's slot fences;
draw setup reclaims a slot before writing it. Resizing settles all slots,
recreates staging owners and rebinds aliases; selected memory types are retained
so the direct-mode requirements remain true after growth. Buffer sizes and
growth limits follow the donor rather than MJR's older large preallocations.
The source commit's claimed 2.8 GB saving is not a measurement of this core.

Host writes are flushed at submission where needed. On the direct path, host
read barriers for vertex RAM, the copy-on-write arenas and staging consumers
are recorded before the draws in the main command buffer. Uniform consumers
include both vertex and fragment stages. The ordinary path retains its copy
command buffer and semaphore. Queue submission continues through the Libretro
queue wrapper, with no new queue/device-idle wait or frontend queue lock.

## DMA, Snapshot Restoration And Receive Queues

Further sources are MJR `3ca6af93d4` (mapped DMA and post-load writeback),
`22e711ac19` and `bd76bc9e83` (NVNet RX recovery), from the same reviewed tip.

Mapped RAM DMA now runs the same memory-access callbacks as flat-view accesses
before returning the mapped pointer. A dirty GPU surface reaches RAM before a
device reads or replaces those bytes. This applies to all Xbox host platforms,
without changing non-Xbox QEMU machines.

After VM restoration, RAM is authoritative. A host-only `flush_discard` flag
causes old renderer state to be discarded before queued downloads, display
resolves or visibility reports can write it back. Vulkan also drops deferred
writebacks on parked surfaces, then resets settled query counters; OpenGL
deletes old query objects and frees their arrays without reporting their values.
Ordinary GPU reset retains its existing writeback behavior. No VMState fields
or snapshot-format version are added.

NVNet retries queued receive traffic after link/register changes and, when the
RX ring is full, through one pending 1 ms **virtual-clock** timer. The timer does
not pace emulation. Running state, physical/backend link, RX enable and DMA
enable gate the operation; backend TX congestion does not block RX recovery.
Reset cancels the timer and device destruction frees it. With networking
disabled, this path does not poll the RX ring. The existing networking core
option remains the only user control.

Snapshot testing also exposed a separate Libretro dispatcher omission:
publishing a request did not wake QEMU's main loop while the frontend waited.
The omission is present in original Libretro commit `15e4475357` (2026-02-12),
the preceding fork release and the current donor. Calling `qemu_notify_event()`
after publishing the request fixes the observed save timeout. This is a port
correction found during the MJR checks, not an imported MJR change; no runtime
bisect established when the old omission became easier to encounter.

## Verification

Local source, build/test runners, binary hashes and evidence are retained under
`build/mjr-integration-20261027/`. These are uncommitted working-tree builds;
their startup commit label alone does not identify the adaptations.

- Native macOS ARM64 and Linux x86_64 builds pass. The existing six fog tests
  plus a new differential test pass on both architectures. The latter compares
  6144 fog results bit for bit with full vertex-program interpretation, covering
  partial writes, temporary/oPos dependencies, paired MAC/ILU operations,
  relative constants, constant-write fallback, cache invalidation and eviction.
- A recording VMA harness executes the actual buffer implementation for 32
  combinations of memory flags/alternate-allocation compatibility. It checks
  selection and fallback, growth of every staged-buffer family, preservation
  of aliases/memory types, and complete destruction without duplicate frees.
- Harnesses execute the actual OHCI frame functions and NV2A reset/IRQ functions
  with mocked DMA/IRQ dependencies. The pre-adaptation source fails the targeted
  ordering/head-clear and stale-interrupt checks; the adapted source passes.
  These are focused device-code regressions, not a guest-game reproduction of
  MJR's reported Azurik hang.

Final OutRun 2 checks used 64 MB and 1x resolution:

| Target | Renderer / Memory Path | Result |
|---|---|---|
| macOS ARM64 / Apple M4 | Vulkan / direct staging, validation enabled | 3D attract at 75 seconds; pause/resume, reset and same-process close/reload pass; two joined unloads, exit 0. |
| macOS ARM64 / Apple M4 | OpenGL | 3D attract at 75 seconds; joined unload, exit 0. |
| Batocera x86_64 / Vega 11, Mesa 25.3.6 | Vulkan / staging-copy fallback, VSync and audio sync off | 3D attract at 75 seconds; joined unload, exit 0. |
| Same Batocera / system driver | OpenGL / Raven workaround | 3D attract at 75 seconds; joined unload, exit 0. |

The final Vulkan validation run completed in 153.44 seconds with no API errors
or synchronization hazards; unused shader-output warnings remain. Original
RetroArch configuration, firmware/HDD/EEPROM fixtures and source media were
preserved. Tests used separate writable fixtures; the normal local macOS core
was then updated as `xemu_libretro.dylib`, with matching metadata and verified
existing dependencies. No `_dev` copy was installed.

These checks establish boot/attract and bounded lifecycle behavior, not
interactive gameplay, audible audio quality or a measured FPS/frame-pacing
improvement. Physical discrete-GPU qualification, Windows/macOS Intel/Linux
ARM64 builds and Chihiro game boot remain separate validation work; the latter
still awaits netboot media. GitHub Actions stay disabled and future release
selections exclude Android.

### Additional IO Checks

Evidence for the DMA/RX/restore adaptations and dispatcher correction is under
`build/mjr-io-20261027/`, with source hashes because the integration is still
uncommitted. Both macOS ARM64 and Linux x86_64 rebuilds pass, including the
seven fog-carryover subtests on Linux.

- Three mapped-DMA source-path checks verify callback direction/address/length,
  visibility before a mapped read, protection of a mapped write and the
  zero-length case. The pre-adaptation source fails the two coherence checks.
- Four Vulkan surface-flush cases cover ordinary/discard flushes with and
  without a renderer switch, including parked writeback debt. The ordinary
  behavior is unchanged; both formerly failing discard cases now preserve RAM.
- Seven ASan/UBSan report-discard cases exercise the actual GL/Vulkan helpers
  with mocked GL calls: clear entries with poisoned unused fields, active
  queries, array ownership, empty queues and repeated cleanup all pass.
- Ten NVNet source-function checks cover paused/link/RX/DMA gating, backend TX
  congestion, timer rearming and recovery when an RX descriptor becomes free.
  All pass with the second MJR fix; this is not a System Link gameplay test.
- In macOS RetroArch 1.22.2, a same-core save succeeds after the main-loop wakeup
  correction. A private diagnostic interposer then invokes two real
  `retro_unserialize` calls from the frontend thread with each renderer.
  Vulkan and OpenGL resume rendering the OutRun 2 attract scene and exit with
  joined emulation threads, status 0 and no forced termination. This bounded
  core-path check does not establish exact paused-image restoration, general
  frontend slot/file integration or support for older snapshots. Initial
  frontend-driven attempts separately exposed metadata gating and an
  asynchronous-file-write race in the runner; their failed reports are kept.
- Batocera 43.1 / Vega 11 / system Mesa 25.3.6 reaches OutRun 2's 3D attract on
  OpenGL at 75 seconds and Vulkan at 120/180 seconds. Vulkan uses the established
  VSync/audio-sync-off configuration; OpenGL uses the Raven workaround. All
  three native runs join the emulation thread and exit 0 without force.

The normal local macOS `xemu_libretro.dylib` and matching metadata are updated,
with the previous installed core retained. Existing dependency hashes match;
the active configuration and external-drive system location are preserved.
Diagnostic interposers and permissive metadata are not installed. Only one
current snapshot fixture, final artifacts and representative evidence are kept;
task-owned builder data, remote test roots and redundant runtime copies are
removed. These tests do not reproduce MJR's Azurik level transition or verify
network play, audible audio quality, Chihiro boot or a performance improvement.
