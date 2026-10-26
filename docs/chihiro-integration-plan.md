# Xbox and Chihiro Core Separation

Status: dependency assessment completed on 2026-10-26. No Chihiro sources have
been integrated into the working core, and no split binaries have been built.

## References

- Fork HEAD at assessment start: `3d45b56f8ffae8cd1604eee2c8e017cc3ea12719`,
  plus the Linux boot, APU, VBlank and Raven OpenGL corrections preserved in
  `v0.8.138-3-libretro.1`, the release checkpoint before further integrations.
- Official upstream: `25e1ee2fd59a451f2ab57e825b4f8a5414220c8b`.
- Wizzard reference: `8f7da2e39323e0df6e0a6c321c9f679b36bc624c`.
- Original Libretro reference: `c3cf3a923cdd3fc54d783b137c96ed6fbea0262e`.

The Wizzard update changes 283 files, including a second NV2A implementation,
TCG changes, shader seeds and optional Vulkan fast paths. Its full diff is not
the minimum Chihiro dependency set. Its upstream policy and version label do
not replace this repository's official-upstream policy or merged revision.

## Build Architecture

Use one source repository with two fixed build profiles. Initially configure
two independent Meson build directories with a proposed `libretro_system`
choice, defaulting to `xbox`. QEMU's shared source sets, generated headers and
static objects make independent build directories simpler to isolate than
linking two names against the same combined object library.

| Property | Xbox Profile | Chihiro Profile |
| --- | --- | --- |
| Library stem | `xemu_libretro` | `chihiro_libretro` |
| Frontend label | Microsoft - Xbox (OG) | Sega - Chihiro |
| Machine choice | Fixed Xbox | Fixed Chihiro |
| Chihiro board/UI objects | Excluded | Included |
| System directory | Existing `system/xemu/` | Separate `system/chihiro/` |
| Content | Existing Xbox ISO/XISO | Initially validated FATX netboot images |
| RAM | Existing Xbox setting, including 64 MiB | 128 MiB main RAM; DIMM managed separately |
| Options | Preserve existing `xemu_*` keys | Separate `chihiro_*` keys |

These are proposed profiles, not implemented build switches. The Chihiro core
still needs the shared Xbox-derived CPU, APU and NV2A hardware. Fixed profiles
must also select metadata, controller descriptions, firmware checks, defaults,
cache directories and packaging. Renaming a combined library is insufficient.

The content reader must distinguish an unreadable or invalid file from another
system's media. Wizzard's `not Xbox disc => Chihiro` heuristic is unsuitable for
the fixed profiles. For Chihiro, validate the FATX layout and boot metadata;
the `FATX` magic alone also occurs in Xbox storage. A no-content launch, if
advertised, must retain the profile's fixed machine.

## Required Source Groups

| Group | Sources / Hooks | Integration Rule |
| --- | --- | --- |
| Board emulation | `hw/xbox/chihiro/`: 16 C files, 17 headers, one Meson file | Chihiro profile only; retain the coherent board implementation initially |
| Machine wiring | `hw/xbox/xbox.c`, `xbox.h`, `meson.build`, SMC hooks | Compile-time separation; preserve the Xbox construction path |
| IDE media board | `hw/ide/core.c` DMA serving and IDENTIFY; `block/blkmemory.c`, `include/block/blkmemory.h` | Chihiro-only dispatch; do not take unrelated IDE performance counters |
| Snapshot participation | `BlockDriver.snapshots_covered_by_vmstate`, matching `block/snapshot.c` exemption, block build selection | DIMM is serialized by device VMState; the memory backend is not an independent snapshot disk |
| Startup and firmware | Chihiro parts of `system/vl.c`; debug EEPROM generation; settings schema/defaults | No retail MCPX/HDD/DVD or Xbox USB hub on Chihiro; preserve Xbox resources |
| USB | Chihiro OHCI start/stop notifications, device lookup and port-status changes | Scope changed behavior to Chihiro; do not replace Xbox OHCI behavior wholesale |
| Frontend controls | `ui/libretro-chihiro.c`, optional crosshair bridge, rumble output, input polling | Use RetroArch input callbacks; no SDL event or window dependency |
| Libretro lifecycle | `ui/libretro.c`, settings/UI stubs | Fixed target, controlled errors, unload/reset/reload and per-game persistence |
| Build and packaging | Meson profile, separate `.info`, existing local/CI packaging recipes | Preserve Xbox identity and dependencies; CI stays disabled |
| Attribution | DES source notices and `licenses/mame-naomigd.license.txt` | Retain attribution with the Chihiro source and distribution |

Specific adaptations are required; copying all changed files in these groups
would also copy unrelated behavior:

- `hw/ide/core.c:1486` in the reference changes IDENTIFY for every slave device
  without checking machine type. Limit that override to the Chihiro profile.
- OHCI's disabled-port fallback and port-enable status changes are likewise
  shared code in the donor. Preserve the existing Xbox branch.
- Chihiro needs a debug EEPROM with progressive video defaults. Keep that
  EEPROM separate; do not import automatic renaming/regeneration of an
  existing Xbox EEPROM as a prerequisite for Chihiro.
- Wizzard's `populate_config()` still resolves Xbox files before adding the
  Chihiro paths. The split must branch before firmware validation and must not
  require the Xbox HDD/BIOS files for Chihiro.
- The Chihiro configuration schema can supply the board's fields and enums
  without importing every standalone binding, CPU option or automatic-machine
  setting. Core settings continue to come from Libretro options, not TOML.
- `ui/libretro-chihiro.c` compiled in the probe against the current input API.
  Its JVS bridge does not require the donor's `ControllerState.gp` migration,
  USB Xbox light-gun device, SDL haptics or native pointer backends.

## Dependencies That Can Be Separated

Two direct source references tie the Chihiro board code to donor features that
are not board logic:

1. `chihiro-an2131.c:1586`: `xemu_tbrate_enabled()` gates a performance probe.
   Omit that optional instrumentation when bringing the board to the current
   core; it does not require importing the donor's CPU execution changes.
2. `chihiro.c:1430`: `nv2a_set_game_name()` selects and renames shader seeds.
   Omit the seed notification until that subsystem is evaluated independently.
   Preserve Chihiro's own executable identification and cabinet selection.

Do not import the following as part of the first Chihiro milestone:

- TCG/CPU Boost changes, native SSE helpers, widened jump caches and CPU pacing.
- The optimized NV2A renderer or the duplicate `nv2a-stock` tree.
- Vulkan pipeline libraries, expanded dynamic state and associated fast paths.
- Shader seed learning, embedded shader seeds and Smooth First Play.
- Replacement SEGABOOT logo/jingle data: these are donor easter-egg assets,
  not required board firmware. Keep the current `data` exclusion intact.
- Standalone window input, pointer-device backends, SDL haptics and session-log
  capture threads.
- Linked-cabinet configuration. The initial profile targets one cabinet; the
  coherent hardware set may still compile its network-board implementation,
  without enabling it or exposing an inactive option.

These exclusions establish an initial integration boundary. They do not prove
that every Chihiro title renders correctly or runs fast enough on the current
CPU/renderer. Any demonstrated missing hardware behavior must be evaluated
separately with a pinned reference before extending that boundary.

## Lifecycle Work Required Before Tester Builds

The reference registers `chihiro_exit_notify()` and the ASIC worker shutdown
with QEMU's process-exit notifiers. The Libretro emulation thread ends through
`vm_shutdown()`, while the machine and module remain alive. There is no
`chihiro_flush_save_now()` call in the donor's Libretro unload path.

The initial implementation must therefore:

- Flush arcade backup and DIMM system data when content closes, even if
  RetroArch remains open. Switching to a different Chihiro title already has a
  flush path, but that is not a substitute for closing the current title.
- Reuse the reference's signal/broadcast/join sequence for stoppable Chihiro
  workers, at a point that does not hold a lock the worker needs. Make the
  matching restart path explicit for subsequent loads; an initialized ASIC
  must not be left marked initialized with its worker permanently stopped.
- Handle timers, deferred callbacks, card readers and rumble consistently
  across reset, unload, failed load and another title. Split save flushing from
  worker teardown where necessary; the donor's flush helper also tears down
  the network board.
- Preserve the existing QEMU module pinning exception and private symbols.
  Test metadata-only open/close and Xbox-to-Chihiro-to-Xbox core switching in
  one frontend process; two pinned modules must not share emulator globals.
- Preserve the current OpenGL reload limitation until separately corrected;
  refuse unsupported reloads clearly rather than assuming the split fixes it.

## Independent Libretro Improvements

Frontend-inactivity pause and post-VBlank frame handover are separate from
Chihiro support. They can be reviewed against the current guest-clock VBlank
implementation and telemetry without replacing the emulator base.

The donor pauses after 50 ms without `retro_run()` and uses a bounded 4 ms
polling wait for VBlank acknowledgement. Those constants are not automatically
appropriate for slow frames, fast forward, menu pause or this fork's existing
deadlines. Preserve the tested implementation until a focused comparison
establishes a benefit. Neither change is required to compile the board subset.

## Evidence and Limits

An isolated macOS ARM64 syntax/API probe compiled the donor's 16 Chihiro
hardware C files, memory-backed block driver and Libretro JVS bridge against
the current QEMU, CPU and renderer headers:

| Probe | Passed | Result |
| --- | --- | --- |
| Reference board subset with its config declarations | 15 / 18 | Missing TBRATE hook, shader-seed hook, block snapshot field |
| Optional TBRATE and shader-seed hooks removed | 17 / 18 | Only block snapshot field remains |
| Required block snapshot field added in temporary header | 18 / 18 | No syntax errors or compiler warnings |

The reference config schema/settings declarations were overlaid for this
probe; they have not been integrated into the core. The real block snapshot
exemption must accompany the new field in a complete implementation.

This was `-fsyntax-only`: no object link, two-target build, firmware load,
emulation, title rendering, gameplay, performance or peripheral compatibility
is established. All pre-existing dirty source files were hash-checked before
and after and remained unchanged. Temporary extracted sources and generated
configuration were removed; the pinned reproduction script and result logs
remain under `build/upstream-check-20261026/subset-probe/`.

## Implementation Order and User Involvement

1. Preserve the completed Xbox correction set as a separate reviewable change.
2. Implement the default Xbox profile and second library identity; compile-time
   exclusion must be verifiable from build inputs and binary symbols.
3. Integrate the board subset and guarded shared hooks into the Chihiro profile,
   with fixed machine selection, separate data and complete content lifecycle.
4. Build both profiles, then test Xbox regressions and one Chihiro title using
   existing media in place. Inventory storage before requesting missing assets.
5. Evaluate independent performance/frontend changes only against a repeatable
   baseline and retain the current Raven, APU, VBlank and NODELETE corrections.

Source adaptation, local build recipes, metadata, documentation, existing
resource inventory and supported automated tests can be completed autonomously.
User assistance is needed only for unavailable firmware/media and physical
peripheral or subjective display tests. Adopting another principal emulator
base remains a separate decision. Commit/push and publishing require explicit
authorization; workflows remain disabled.
