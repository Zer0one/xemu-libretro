# Xbox, Chihiro And Full Build Profiles

Status: first implementation checkpoint, 2026-10-27. Profile selection,
identity, option registration/reads, initial content admission, storage roots
and build recipes are implemented. Three macOS ARM64 libraries build and pass
metadata coexistence checks. Xbox reaches the OutRun 2 attract sequence in
RetroArch/Vulkan and closes normally. The two dedicated cores are installed
locally for testing. Chihiro also boots OutRun 2 directly from its CHD and
shows the introduction and attract scenes; broader worker lifecycle validation
remains pending. Full remains a private probe only.

## Source Baseline And Scope

Use the current `main` at `1af96052df4b5c23d04ab661cd385b652818c31f`,
including the complete Wizzard donor merge and subsequent MJR adaptations.
All profiles must build from the same checkout and revision. Do not split
repositories, maintain separate emulator implementations, or revert to the
selective import proposed in the historical
[dependency assessment](chihiro-integration-plan.md).

Keep CPU, APU, both NV2A implementations, shader seeds and existing platform
corrections common. This activity changes build selection and Libretro
integration, not rendering or performance policy. Keep platform-independent
code shared and retain existing platform guards.

## Artifact Contract

| Profile | Frontend Name | Library Stem | Default Build Set |
| --- | --- | --- | --- |
| `xbox` | Microsoft - Xbox (OG) | `xemu_libretro` | Included |
| `chihiro` | Sega - Chihiro | `xemu_chihiro_libretro` | Included |
| `full` | Xemu-Full | `xemu_full_libretro` | Excluded; explicit selection only |

Chihiro retains `Xemu-Chihiro` as its internal runtime/core name; the generated
metadata uses `Sega - Chihiro` as its display name and description. This keeps
the requested presentation separate from the stable core/config identity.

The two additional library stems match the
requested frontend names and conventional Libretro naming. Extensions remain
platform-specific: `.dylib`, `.so`, `.dll`. Each library needs its own matching
`.info` file and package identity. Never rename the installed Xbox core to a
development-only identity.

After manually replacing a core or its `.info`, refresh RetroArch's Core Info
Cache: write the standard `core_info.refresh` marker in its configured info
directory, then restart the frontend. A cached entry with missing/older
metadata can hide newly supported content extensions even when the installed
`.info` is correct. Frontend variants with a separate `core_probe.cache` may
also need the affected core's stale probe entry removed; preserve other entries.
The 2026-10-28 local refresh restored Sega - Chihiro's CUE association without
changing the user's configuration or starting a game.

Use the single Meson choice `libretro_system=xbox|chihiro|full`, and
separate build directories for generated headers and objects. The build
orchestrator selects Xbox and Chihiro by default; a single Meson build selects
one profile. Full must be selected explicitly, including in release recipes.
Keep the currently disabled CI disabled. Android remains excluded from the
default release set.

## Modular Implementation Boundary

Use `ui/libretro-profile.h` for fixed identity, permitted
machine types, option eligibility and storage policy. Avoid scattering
platform checks or copied option tables through emulator devices.

| Current Location | Required Adaptation |
| --- | --- |
| `meson_options.txt`, `meson.build` | Profile choice, generated profile configuration, distinct library target and metadata |
| `ui/libretro.c` | Profile metadata, content admission, firmware paths, input descriptions and load/unload integration |
| `ui/libretro-settings-stub.c` | Fixed machine selection for dedicated profiles; shared selection for Full |
| `ui/libretro_core_options.h` | Filter registration before converting to APIs 2, 1 and 0, including translations |
| `ui/libretro-chihiro.c` | Preserve donor JVS, light-gun and rumble behavior; expose it only for arcade content |
| `hw/xbox/meson.build`, `ui/meson.build` | Exclude dedicated arcade objects from Xbox where practical without replacing hardware logic with dummy implementations |
| `hw/xbox/xbox.c`, `system/vl.c`, IDE, OHCI and SMC hooks | Retain common hardware; audit cross-profile references before removing objects |
| `.github/workflows/libretro.yml` and local packaging | Two default profiles, optional Full; matched library/info names and dependencies |

Separate source directories are not separate machines: Chihiro uses the Xbox
CPU, APU and NV2A. Conversely, merely renaming the existing combined library
does not create a console-only or arcade-only core. Fixed machine admission,
options and resource ownership are mandatory even if some shared translation
units retain both implementation paths.

## Options And Defaults

The current English table contains 29 options on macOS (28 elsewhere). Preserve existing raw `xemu_*`
keys initially; separate library identities give frontend core configuration
its own scope. A second option namespace would multiply donor merge changes
without being necessary to hide irrelevant options.

| Eligibility | Current Keys |
| --- | --- |
| Xbox only | `xemu_memory`, `xemu_network_backend`, `xemu_skip_boot_anim`, `xemu_avpack`, `xemu_video_aspect`, `xemu_hdd_trim` |
| Chihiro only | `xemu_chihiro_freeplay`, `xemu_chihiro_gun_aim`, `xemu_chihiro_crosshair`, `xemu_chihiro_ffb` |
| Common | `xemu_hard_fpu`, `xemu_cpu_boost`, `xemu_renderer`, `xemu_moltenvk_queue_submission`, `xemu_aspect_ratio`, `xemu_surface_scale`, `xemu_video_debug`, `xemu_audio_thread`, `xemu_use_dsp`, `xemu_dsp_jit`, `xemu_hrtf`, `xemu_vp_workers`, `xemu_rumble_p1`, `xemu_rumble_p2`, `xemu_deadzone_p1`, `xemu_deadzone_p2`, `xemu_smooth_first_play`, `xemu_audio_volume`, `xemu_vk_fast_paths` |

The source inventory confirms each of the 29 macOS keys appears exactly once
in this classification (25 Xbox, 23 Chihiro, 29 Full). Other platforms omit
MoltenVK Queue Submission, leaving 24/22/28. The network option is
Xbox NAT/System Link, not an arcade cabinet-link control. Chihiro has fixed
128 MiB main RAM in `system/vl.c`; DIMM size is a separate board setting.
The console boot-animation, AV-pack and System Aspect Ratio selectors are
not arcade cabinet settings. System Aspect Ratio changes the retail dashboard
flags; its EEPROM write is also skipped for Chihiro content in Full. The
remaining Aspect Ratio setting changes frontend presentation: Auto is 4:3 for
Chihiro, while 16:9 stretches that image without enabling game widescreen.
The dedicated Chihiro profile puts Free Play under System and gun/steering/
rumble settings under Controls. Full retains the donor's Chihiro category.

`xemu_display_filtering` was removed: it only assigned a standalone display
setting with no consumer in the Libretro renderer/presentation paths. Old
saved values are ignored in all profiles. Frontend filtering/shaders remain
the frontend's settings. The donor's radial per-player deadzone (10%) replaces the old driving-only
12% filter. Ignore its retired key and `xemu_cache_shaders`; shader caching
is always enabled. Threaded audio follows the donor by explicit developer
approval; the per-frame path remains the frontend-rejection/disabled fallback.

Filter both registration and reads: a stale frontend variable must not change
an option unavailable in the selected profile. Remove empty categories.
Test English and translated definitions, API versions 2/1/0 and repeated
`retro_set_environment` calls. Do not hand-edit generated translations.
Full keeps both option families.

## Content, Firmware And Writable State

The previous detector returned false for both non-Xbox content and failed
reads, and `retro_load_game` treated either as Chihiro. The new standalone
admission module, `ui/libretro-media.c`, distinguishes absent content,
unreadable/unsupported files, Xbox disc signatures and plausible FATX
containers. Dedicated profiles reject recognized content of the other type
before configuring graphics or starting QEMU. Full retains content-based
selection but no longer treats arbitrary invalid files as Chihiro.

Admission checks FATX header geometry and minimum container size; it is not
proof of a bootable Chihiro title. Complete boot validation remains with the
donor. If deeper preflight validation is needed, reuse its FATX/boot metadata
logic rather than adding a second filesystem parser. The current
`chihiro_read_image_bootid` depends on configured DIMM filesystem sizing, so
calling it before machine configuration is not a ready-made validator.
An Xbox-patched arcade ISO remains Xbox content. No-content behavior, if
advertised, must honor the fixed profile instead of silently booting Xbox.

Preserve Xbox files under `system/xemu/` and existing user HDD/EEPROM data.
The arcade writable root is `system/xemu-chihiro/`; Full uses
`system/xemu-full/`. Firmware may be read from the existing
`system/xemu/Chihiro/` directory as a compatibility fallback. The profile's
`Chihiro/` subdirectory and MAME game/shared BIOS ZIPs or folders are also
supported; [firmware lookup and cache](chihiro-direct-media.md#firmware-lookup)
keep archive inputs separate from mutable machine data. Do not silently
share writable HDDs, EEPROMs, cards or snapshot stores between concurrently
loaded cores. Full does not automatically reuse the Xbox HDD or EEPROM;
it can reuse read-only MCPX/BIOS firmware. Existing explicit path overrides
remain supported. No writable data migration is performed.

Do not copy ROMs, generate substitute firmware or migrate existing saves as
part of the split. Resolve the frontend's actual system directory, which is
on the external drive for the user's installation.

## Lifecycle And Coexistence Gates

Rechecked on the current baseline:

- Libretro ends its emulation loop through `vm_shutdown()` and joins the
  frontend emulation thread. It does not run full process-exit cleanup.
- `chihiro_flush_save_now()` delegates to `chihiro_exit_notify()`, which first
  calls `chihiro_netboard_exit()`. It is not a persistence-only helper.
- A new `chihiro_flush_persistent_data()` helper reuses the existing USB
  backup and DIMM writers after `vm_shutdown()`, while the BQL is held. It
  does not stop the netboard workers: the core retains its machine for reload.
  The standalone exit callback retains its previous teardown order.
  Dispatch is tested with stubs; real game persistence/reload is still open.
- ASIC and network workers have upstream stop/join logic. Reuse that logic
  where teardown is required and establish the matching restart path.
- QEMU modules stay pinned. Distinct filenames alone do not prove two cores
  can coexist: check private symbols and Xbox/arcade/Full metadata loading
  in one process, followed by supported load/unload sequences.

Keep the current explicit OpenGL reload restriction unless separately
corrected and verified. Do not weaken it to make a split test appear to pass.
Separate persistence flushing from teardown only where the lifecycle audit
shows it is needed; retain the donor's device logic and snapshot format.

## Implementation And Verification Order

1. Add profile selection, identity and option filtering; validate all option
   API paths with an isolated frontend callback harness.
2. Apply fixed machine admission, firmware/storage policy and metadata;
   verify wrong-profile and unreadable content fail before guest startup.
3. Resolve arcade close/reload persistence and source dependencies; then
   build the two dedicated artifacts and an explicitly requested Full probe
   from the same revision. Do not enable remote CI for this local work.
4. Verify exported symbols and frontend metadata/coexistence. Run bounded
   Xbox regression tests using the existing game archive and private writable
   fixtures. Install only an adequately verified normal-name core.
5. Chihiro OutRun 2 boot/attract from CHD now passes with normal exit. Five
   game payloads pass native read/decrypt checks. Interactive controls, arcade
   persistence/reload and other titles remain separate validation gates.

Report source/build, frontend load, visible guest boot and gameplay separately.
Do not claim arcade gameplay, cross-platform runtime support or performance
improvement from a successful build. Commit/push and release remain separate
actions. The two dedicated normal-name cores and generated metadata have
been installed for local tests, with the prior Xbox core/info preserved.
The frontend configuration and external system data were not modified.

## Checkpoint Evidence

- `python3 scripts/test-libretro-profiles.py`: 1,332 registrations using the
  actual option code, across three profiles, APIs 0/1/2, language selections,
  repeated registration and builds with/without translations. Original
  defaults and values remain intact; hidden stale variables are not read.
  AddressSanitizer and UndefinedBehaviorSanitizer report no errors.
- `python3 scripts/test-libretro-media.py`: 14 synthetic fixtures cover missing
  files, directories, unknown/truncated data, malformed FATX geometry, a
  candidate container, XISO and Redump signatures. Null/empty content is also
  checked. This does not use or validate a real Chihiro game.
- Separate macOS ARM64 build directories produce Xbox, Chihiro and explicitly
  selected Full libraries. The metadata harness loads all three in one
  process, verifies names/extensions/options and local symbol visibility,
  then queries Xbox again. Eight invalid/wrong-profile load attempts are
  refused with a frontend message before graphics negotiation. The harness
  calls `retro_init`/`retro_deinit`; no guest or frontend rendering starts.
- `python3 scripts/test-libretro-chihiro-flush.py`: six dispatch cases
  exercise the actual extracted C functions with instrumented storage stubs
  under ASan/UBSan. This verifies ordering, not on-disk game persistence.
- A 95-second Xbox/Vulkan run of OutRun 2 at 64 MiB reached the attract
  sequence, captured at 30 and 75 seconds. The emulation thread joined and
  RetroArch exited with status zero without forced termination. Protected
  inputs/configuration hashes and archive metadata remained unchanged. This
  used private writable fixtures and null audio; it is not an audio or
  gameplay/performance validation.
- Local build orchestration defaults to Xbox and Chihiro; the disabled CI
  recipe uses the same selection and generated `.info` names. YAML structure
  and packaging references were checked locally; no remote jobs were run.

Logs, local probe packages and source/build hashes are kept under
`build/profile-separation-20261027/`. Current builds retain shared board
translation units. Source pruning, arcade-specific controller descriptions,
real arcade persistence/worker lifecycle validation, gameplay and cross-platform
builds remain open. Local test installation is recorded in `macos-chd-install.json`,
including matching artifact hashes and rollback location. These are uncommitted
experimental builds, not a release.

Direct CHD-to-DIMM loading now boots OutRun 2 with the native key/ISO/DES
path. See [Direct Chihiro Media](chihiro-direct-media.md) for supported
layouts, image-name key fallback, the common 14-title catalogue, uniform DIMM
detection and evidence. CUE/GDI/ISO/raw MODE1 readers and encrypted synthetic
FATX tests are integrated. OutRun 2 (World, Rev A) also boots from its Redump
CUE/BIN into introduction/attract. GDI game boot and broader arcade runtime
validation remain open.
