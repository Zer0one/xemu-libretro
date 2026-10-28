# Upstream Integration, 2026-10-28

## Sources And Scope

- Official xemu: full merge `25e1ee2fd5` → `dc10b4cea9`, six commits.
  Version record: `0.8.138-9 dc10b4cea9`.
- WizzardSK Libretro: full merge `252a7accfb` → `84570ac21d`, four commits.
- MichaelJSr: inspected reference remains `237d34f193`; no new update in this pass.
- Development remains on `main`. `upstream` follows official xemu;
  `libretro` remains the original preserved reference. No release or CI dispatch.

Official changes update VulkanMemoryAllocator to 3.5.0 and use RGBA component
names in generated color expressions. The color notation does not change the
shader calculation. Preserve the Libretro donor's back-face lighting when
resolving that conflict. Standalone workflows and contributor documentation
remain excluded; `CONTRIBUTING.md` is now listed explicitly.

The Wizzard donor remains the complete Libretro base: import its updates by
default and adapt this fork's improvements on that common base. Consolidate
overlapping implementations on the donor, verify behavior and bring necessary
exceptions to the developer for review. Keep portable, modular changes suitable
for future upstream pull requests; see the ongoing
[integration policy](libretro-donor-integration.md#integration-policy).

Preserve the shared engine,
Xbox/Chihiro profiles, native optical/DIMM loader, MAME firmware/key paths,
macOS Vulkan sharing, Linux lifecycle, graphics corrections and MJR adaptations.

## Reviewed Behavior

The developer approved the donor's enabled-by-default Audio Thread, subject
to successful tests. This is an explicit exception to the repository's existing
per-`retro_run` audio rule; AGENTS.md and CLAUDE.md were not edited. The frontend
callback consumes the existing APU ring independently of `retro_run`; the APU's
approximately 64 ms queue bound remains. If disabled or rejected by the frontend,
use the existing fixed 800/801-frame path at 48 kHz / 59.94 Hz. Register only
once content validation has succeeded; publish readiness atomically, clear it
on close, unregister the callback and reset the boot flush for each load.

The developer selected the donor's radial Analog Dead Zone (Player 1/2),
default 10%, instead of retaining our separate driving-only 12% filter.
Remove the old option and ignore saved values. Do not stack both core filters.
Frontend filtering still precedes the core filter. Preserve the donor's
per-player rumble switches, gun aim and force-feedback defaults.

Import DSP JIT/HRTF/voice-worker settings and load-time resolution scale;
retire the shader-cache switch and keep caching enabled. Follow the donor's
console boot/video and Vulkan fast-path defaults. Keep the dedicated arcade
System category generic and combine all
arcade and per-player controls in the same Controls category.
Free Play is applied when content starts, including on reopening content in the
same process; changing the selector alone does not change a running game.

Import the MIT-licensed bundled Xbox dashboard HDD and disabled-by-default
Trim HDD on Close. Never overwrite an existing HDD when automatic creation
fails to open it: use exclusive creation and remove only our incomplete file.
Expose HDD trimming only in Xbox/Full. Mark the bundled-default HDD optional
in frontend metadata. Keep all writable data under each profile's System root.

## Validation

macOS ARM64, local RetroArch 1.22.2 (`fce35fc7`), Vulkan/MoltenVK Sync, VSync on,
CoreAudio output on the actual 44.1 kHz device. Each real-content run lasts
approximately 65 seconds before content close and process exit. A private proxy
observes audio delivery without modifying the frontend/core settings installed
for the user. Source media stay in place; writable test data are private.

| OutRun 2 Case | Batch Calls | Delivered Frames | Calls Off `retro_run` Thread | Batch Size | Result |
| --- | ---: | ---: | ---: | --- | --- |
| Xbox, callback accepted | 3015 | 3085312 | 3015 | 256–1024 | Attract; exit 0 |
| Xbox, callback rejected | 3967 | 3176775 | 0 | 800–801 | Attract; exit 0 |
| Chihiro CHD, callback accepted | 2822 | 2888704 | 2822 | 256–1024 | Attract; exit 0 |
| Chihiro CHD, callback rejected | 3970 | 3178977 | 0 | 800–801 | Attract; exit 0 |

All cases produce nonzero PCM samples and join the emulation thread on close.
Both Chihiro cases inject port 1 left X = 2000/32767 (about 6.1%), the original
centering reproduction; the donor filter reaches attract with the copied
existing save. This is a technical audio-output/boot check, not a listening
comparison, physical-controller certification or proof of improved frame pacing.
Protected configuration, original EEPROM/save and media remain unchanged.

Native ASan/UBSan checks cover 1332 option registrations across all profiles,
option APIs and translated/English tables; retired values and new HDD filtering;
1,835,008 radial/JVS axis mappings; audio readiness/pause/per-load flush guards;
HDD exclusive creation, existing-file preservation and interrupted-write retry;
18 media fixtures and 20 firmware layouts. Both dedicated libraries coexist
and reject six invalid/wrong-profile load attempts before graphics setup.
macOS option counts are Xbox 25 / Chihiro 23 / Full 29; elsewhere 24 / 22 / 28.
Full option registration is checked without generating its optional core.

Both normal macOS core builds pass. VMA and every available commit-pinned Git
subproject match their declared revisions. Windows/Linux runtime tests, a new
GDI gameplay test, subjective audio quality and HDD trimming remain outside
this bounded verification. Trimming stays disabled; no original user HDD was
trimmed or replaced.

Compact logs, JSON receipts and representative captures are retained under
`build/upstream-merge-20261028/`. Private System trees, temporary test libraries,
media caches and the fresh Chihiro build tree are removed after installation.
