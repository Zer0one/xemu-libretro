# Libretro Donor Integration

## Integration Policy

WizzardSK's Libretro port is the complete integration base. Import its additions
and changes by default, then reapply or adapt this fork's improvements on the
updated common base. A feature overlapping our implementation is a reason to
consolidate on the donor, preserving any still-needed correction, rather than
maintain parallel implementations. The per-player deadzone replacing our earlier
driving-only filter is one such consolidation.

Follow donor APIs, data formats and defaults, verifying the resulting behavior.
Bring a concrete regression, compatibility conflict or necessary divergence to
the developer for review; ordinary donor additions do not each require a new
opt-in decision. Document accepted exceptions and keep changes modular,
cross-platform and suitable for future pull requests. Xbox/Chihiro profiles
share the engine and frontend base; their selection layer handles the relevant
options, content and storage differences.

Current integrated references are WizzardSK `84570ac21d` and official xemu
`dc10b4cea9` (`0.8.138-9`). See the
[2026-10-28 integration report](upstream-integration-20261028.md) for the latest
changes and bounded validation. The following sections record the earlier
integration checkpoint and its validation limits at that time.

## Separate References

The integrated references checked on 2026-10-29 are:

| Reference | Repository And Branch | Exact Revision | Role |
| --- | --- | --- | --- |
| Upstream Libretro port | [WizzardSK/xemu-libretro](https://github.com/WizzardSK/xemu-libretro), `libretro` | `84570ac21d828274ec95a0b6d5949281b132fcc0` | Complete port base, frontend contracts and shared Xbox/Chihiro architecture. |
| Official standalone engine | [xemu-project/xemu](https://github.com/xemu-project/xemu), `master` | `dc10b4cea96e403d30c17ed53551fdaf023bc17f` (`0.8.138-9`) | Emulation engine and same-revision standalone comparison. |
| Additional standalone donor | [MichaelJSr/xemu-macos](https://github.com/MichaelJSr/xemu-macos), `master` | `237d34f193061c9f25a6ab0c380e946d1982e526` | Reviewed adaptations documented in [MJR Adaptations](libretro-mjr-adaptations.md); not a replacement Libretro base or a full merge of that fork. |

These are integrated or reviewed checkpoints, not a claim about the latest
remote tips. WizzardSK incorporates Tovarichtch-derived work; this fork receives
that work through WizzardSK rather than treating it as a separate Libretro
upstream. The local `libretro` branch preserves the original port and is distinct
from the current donor reference `libretro/libretro`. The official `upstream`
branch remains free of port adaptations. Record exact source identities and
dependency pins in each integration report; `upstream.version` identifies the
official engine, not the complete port or additional standalone adaptations.

Local improvements adapt to these bases. Standalone remains a comparison
reference: do not modify it to match the core without an explicit developer
request. Standalone work belongs in its dedicated project, not in this checkout.

## Reviewing Local Adaptations

Explain each local difference in the relevant existing code comments, technical
documents and Git history: affected area, reference revision, intended benefit,
evidence, validation limits and the condition for retiring an exception. Keep
that explanation current with the implementation. Use, for example, the existing
[Vulkan lifetime audit](libretro-donor-retention-audit.md),
[MJR adaptations](libretro-mjr-adaptations.md) and
[Chihiro persistence investigation](chihiro-freeplay.md). Do not create a
duplicate patch registry or manually inventory committed versus uncommitted
changes; Git records that state.

At every authorized update of the Libretro port or official engine, review local
adaptations in the affected areas and their semantic dependencies, including
resource lifetimes, configuration, persisted data and frontend contracts.
Perform that review even when Git merges without conflicts. A diff against the
Libretro donor can include newer official-engine changes and reviewed standalone
adaptations; it is not entirely code authored by this fork. Apply the same
review to overlapping work when updating an additional standalone donor.

Preserve a local change's benefit by adapting it to the new APIs, structures and
formats. Remove it only after verifying an equivalent upstream solution or that
its documented need no longer applies. Avoid silently losing corrections or
keeping duplicate implementations. Update the existing explanation, repeat the
relevant checks when the implementation or its assumptions change, and record
the retain/adapt/remove decision in the integration change. Preserve user data;
bring substantial behavior changes, incompatible migrations or loss of a local
feature to the developer with a concrete proposal for review.

Compare pristine standalone at the same official engine revision and assess old
and new baselines separately. Matching engine versions alone do not establish
identical behavior: a documented local correction may affect the observed path.
Consult those differences before attributing a defect to the core or dismissing
a comparison result; retain the standalone result and explain the known
divergence and its validation limits. For MJR comparisons, also identify its
exact revision and relevant adaptations. Keep build, frontend load, visible boot
and gameplay evidence distinct.

This policy supplements [AGENTS.md](../AGENTS.md) without changing it or
`CLAUDE.md`. Official engine updates still require full merges and immediate
application of `.upstream-excluded`; excluded standalone components stay removed.
A documentation or policy update does not authorize a merge, engine change,
game test, installation, commit, push, CI run or release.

## Initial Integration References And Scope

This integration merges WizzardSK/xemu-libretro's `libretro` branch through
`252a7accfb5313523928b8b9fd2f44d2f6d9ce06` into this fork's `main`, above the
`v0.8.138-3.1` checkpoint (`18a331175c`). It keeps the donor's combined Xbox and
Chihiro core, including its hardware, CPU and renderer updates, shader seeds,
automatic aspect ratio, EEPROM video settings and frontend inactivity handling.
The earlier [separation assessment](chihiro-integration-plan.md) is deferred
research, not an implementation requirement.

The official xemu base is still `0.8.138-3` (`25e1ee2fd5`). The official `upstream`
branch and the preserved original `libretro` branch are not development branches.
Standalone UI, packaging and workflows remain excluded. This integration does
not activate CI or publish a release; future release selections exclude Android.

The Libretro donor is the reference for frontend APIs and data formats. Follow
its updates, then reapply necessary fork adaptations; document exceptions and
keep them suitable for a future upstream contribution. Do not create an old
format compatibility layer without a concrete need.

## Retained Adaptations

The old-port/new-port retention audit is recorded in
[Donor Retention Audit](libretro-donor-retention-audit.md). It removes one
obsolete immediate Vulkan view-destruction instruction accidentally retained
beside the new donor's deferred retirement path. The frontend compositor wait
remains; old removed features are not restored by default.

- Preserve macOS Vulkan device sharing, portability features, CPU conversion of
  unsupported primitives, W-depth and packed-depth border approximations, and
  the opt-in MoltenVK Async submission setting. Sync remains the default.
- Preserve OpenGL context recreation, Linux metadata-load pinning, APU register
  fairness, guest-clock VBlank limiting and optional video diagnostics.
- Preserve the Raven/RadeonSI workaround: omit unused geometry-shader point-size
  output for lines and triangles, keeping actual point output. Apply the same
  variant selection to imported shader seeds before warmup, including wide-line
  geometry shaders.
- Keep donor shader structure sizes and common field offsets. Additional
  device-variant flags occupy unused bits or padding, with distinct cache keys.
- Require Vulkan 1.2, as approved by the developer, matching the donor compiler's
  minimum target. OpenGL remains available.
- Require retail MCPX, Xbox BIOS and HDD files only for Xbox. The donor's
  unconditional frontend validation also required them on Chihiro; the guard
  in `retro_load_game()` now lets Chihiro reach its separate firmware checks.
- Keep the runtime library name `Microsoft - Xbox (OG)` for existing frontend
  settings; metadata also identifies Sega Chihiro. Filenames, `xemu_*` option
  keys and the `system/xemu/` base remain stable.

Experimental pre-integration snapshots are not guaranteed to be compatible.
The donor machine state and shader-cache formats are adopted rather than
introducing a new fork-only migration format. Save states remain unsupported
in core metadata.

## Verification

Validation used an uncommitted merge on 2026-10-26. Startup logs report the
checkpoint HEAD, so identify these builds by the source manifest and binary
hashes, not by that commit label alone. Local evidence, runners and source
hashes are retained under `build/merge-wizzard-20261026/`.

| Target | Renderer | OutRun 2 Result | Shutdown |
|---|---|---|---|
| macOS ARM64, RetroArch 1.22.2 | Vulkan / MoltenVK Sync | Visible 3D attract at 75 seconds | Content close, thread join, exit 0 |
| macOS ARM64, RetroArch 1.22.2 | OpenGL | Visible 3D attract at 75 seconds | Content close, thread join, exit 0 |
| Batocera 43.1, RetroArch 1.22.2, Vega 11 | Vulkan | Visible 3D attract at 75 seconds | Content close, thread join, exit 0 |
| Same Batocera, system Mesa 25.3.6 | OpenGL | Visible 3D attract at 75 seconds; Raven workaround logged | Content close, thread join, exit 0 |

The four runs above used 64 MB, 1x internal scale, VSync and synchronous audio.
Firmware was read from its existing location; writable HDD and EEPROM fixtures
were isolated. Original configuration/firmware/fixture hashes and game metadata
were unchanged. No forced termination was required; neither Batocera run added
kernel log messages. These runs establish boot and attract, not interactive
gameplay, audible audio quality, frame pacing or complete rendering accuracy.

An additional Batocera Vulkan regression probe used the same core at 64 MB
with VSync and audio synchronization both disabled. OutRun 2 reached visible
3D attract at 75 seconds and closed with a joined emulation thread and exit 0;
protected inputs and kernel logs were unchanged. This confirms boot progress
without frontend synchronization, not a frame-pacing result.

The macOS Vulkan lifecycle probe also exercised a two-second frontend pause,
resume, reset, content close and reload in the same process. It obtained a
rendered title screen after reset and reload, recorded two loads and two joined
unloads, and exited normally after 113 seconds. This is a bounded lifecycle
check, not a prolonged pause or arbitrary title-switch campaign.

The shader-layout audit compared every common member offset and structure
size with the donor on macOS ARM64, for both GL and Vulkan. `GeomState` remains
20 bytes, `ShaderState` 2620 bytes and `ShaderModuleCacheKey` 2348 bytes; embedded
seed section item sizes match. A pre-start validation probe confirmed that
missing Xbox firmware rejects Xbox before hardware negotiation, while the
non-Xbox route reaches hardware negotiation without those retail files. It
rejected the hardware callbacks before starting a machine; it is not a Chihiro
game or FATX validation test.

Final development binary SHA-256 values:

- macOS ARM64: `80d944338c41a41ac4a947cac19a9d3dbdd9c4ca6b6ab882841763b6293fa4e5`
- Linux x86_64: `e513bc4581b850ec5a4de57ceff183fc065f319f88bb6f9ece14228f7fa8cafe`

## Remaining Validation

Chihiro game boot awaits a FATX netboot image from the developer. Available
Xbox-patched Chihiro discs are Xbox content and cannot establish Chihiro board
compatibility. No conversion or duplicate game media was created.

Windows, macOS x86_64 and Linux AArch64 builds of this integration have not yet
been verified. The earlier checkpoint's remote build results do not validate
this merge. All GitHub Actions workflows remain disabled.
