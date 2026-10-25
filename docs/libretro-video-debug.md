# Video Debug Measurements

Enable **Core Options → Video → Video Debug Logging** (`xemu_video_debug =
"enabled"`) and enable frontend info-level logging. No graphical overlay or
standalone configuration file is required. The option defaults to disabled,
matching the standalone's initially closed Video Debug window.

## Reference And Definitions

The inspected reference is MichaelJSr/xemu-macos commit
`74a27096ed6bd7b5ad6d83ed7fd55230f9e98cb8`, based on the same official xemu
commit as this core, `25e1ee2fd59a451f2ab57e825b4f8a5414220c8b`.
Its `ui/xui/debug.cc`, `DebugVideoWindow::Draw`, reads `g_nv2a_stats`.
The FPS and MSPF calculation in `hw/xbox/nv2a/pgraph/profile.c` is identical
in both repositories. The core retains that calculation.

| Log Field | MJR Video Debug Source | Meaning |
|---|---|---|
| `mjr_fps` | `g_nv2a_stats.increment_fps`, displayed as `FPS: %d` | Count of `NV_PGRAPH_INCREMENT_READ_3D` events divided by elapsed host real time, updated after at least 250 ms, truncated to an integer. |
| `mjr_mspf` | Last completed `frame_history[].mspf`, displayed as `MSPF: %d` | Integer milliseconds from the most recent increment event to the subsequent `FLIP_STALL` profiling call. It is neither a GPU timestamp duration nor the interval between displayed frames. |
| `Video Debug Counters` | Last completed `frame_history[].counters[]`, MJR Advanced plot | Per-frame draw, shader, texture, surface and queue counters, identified by name. Not totals over the logging window. |
| `increment_hz` | Additional core diagnostic | Decimal increment event rate between two snapshots, normally about one second apart. Same event as MJR FPS, different averaging window and no integer truncation. |
| `flip_stall_hz` | Additional core diagnostic | Rate of completed `FLIP_STALL` profiling events over that window. |
| `frontend_hz` | Additional core diagnostic | Actual `retro_run` call rate over the corresponding frontend time window. |
| `declared_hz` | Libretro AV information | The current declared refresh rate, 59.94 Hz. This change does not alter it or the audio budget. |
| `last_increment_age_ms` | Additional core diagnostic | Age of the last guest increment event; -1 before the first. The inherited integer FPS can remain stale when the guest stops producing events. |

MJR has changed some advanced counter names and queue/flush paths. Compare
common names in the context of each implementation, not array indices or
identical expected values. Its release builds can compile out the advanced
counters (`NV2A_STRIP_PROFILE_COUNTERS`, default 1 outside debug builds), while
the core retains its existing counters. All-zero advanced values in such an
MJR build are unavailable measurements, not proof of zero GPU work. FPS and
MSPF remain active in those MJR builds.

## Frame Intervals And Limits

`Video Debug Intervals` uses the producer's own host real-time timestamps at
successive guest increment events. It reports mean, p50, nearest-rank p99,
maximum and number over 25 ms for up to the last 300 intervals (roughly five
seconds at 60 events/s). Non-positive intervals are excluded. Intervals crossing
the initial diagnostic baseline are excluded; baselines restart after content
load, snapshot load, reset, or re-enabling the option. Pauses inside an active
window can appear as long intervals. The one-second rates and rolling interval
statistics have different windows and must not be treated as the same sample.

The snapshot copies completed stats while briefly holding the existing PGRAPH
lock. Acquisition is a single nonblocking try; a busy worker defers collection
to a later `retro_run`. Sorting, allocation and log formatting happen after the
lock is released. No file I/O, sleep, GPU wait or queue policy change is added
to the producer. Collection is optional; the added producer bookkeeping is a
timestamp difference and a bounded ring write per increment event.

The logs do not identify unique on-screen pictures or measure physical
scanout. A regular guest can still be displayed unevenly by a frontend or a
compositor. The existing `FPS: guest flips; updated pictures; frontend` log
uses separate scanout-write/display-update counters and a longer window;
those fields are not aliases of the MJR Video Debug FPS.

## Interpreting 59.94 And 60

The core currently declares 59.94 Hz and hands over audio at 48000/59.94
samples per call. With frontend synchronization enabled the measured call
rate may follow that target. Turning synchronization off changes what paces
the frontend. This alone does not establish whether any observed difference
comes from audio pacing, display refresh or another limiter.

The MJR panel formats FPS as an integer and uses short windows. For example,
15 events in 251 ms produce `59`, while 15 events in 250 ms produce `60`.
A displayed integer 60 cannot establish a sustained exact 60.000 Hz or better
frame pacing. Compare the same event, averaging period, snapshot and settings,
and retain long stalls alongside typical intervals. This diagnostic change
does not itself attempt to improve synchronization or rendering performance.

## Local Validation

macOS arm64 Vulkan validation loaded the same OutRun 2 snapshot with VSync
on/Audio Sync off, both on, and both off. Each diagnostic run unloaded and
exited normally. A signed canonical installed-core smoke also passed with
logging disabled and emitted no new debug blocks. Focused checks cover the
original FPS truncation, completed-frame MSPF/counters, nonblocking snapshot
access and interval-ring wrap, including AddressSanitizer/UBSan; option
registration was checked for APIs 0/1/2 in English and Italian.

One actual core log pairs `mjr_fps=59` with `increment_hz=59.996`, demonstrating
the difference between integer short-window FPS and decimal event rate. The
current MJR build likewise yielded both integer 59 and 60. Its advanced counters
were compiled out. The diagnostic feature is validated; a lossless comparison
of physical display presentation remains outside these measurements. Full
local evidence is retained in `build/video-debug-2026-10-25/REPORT.md`.
