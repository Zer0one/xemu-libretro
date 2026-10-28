# Core Options Across Content Loads

This audit follows the Wizzard Libretro base while accounting for the retained
QEMU machine and frontend/renderer resources recreated on each content load.
It extends the [Free Play correction](chihiro-freeplay.md) without adding an
alternative machine teardown or changing the guest/save/shader-cache formats.

## Applied Settings

| Lifetime | Options / Behavior |
| --- | --- |
| Runtime readers | Core Aspect Ratio, Video Debug Logging, DSP emulation/JIT, HRTF, volume, per-player deadzone/rumble, arcade gun aim/crosshair/force-feedback strength |
| Content start | Free Play, Xbox System Aspect Ratio, Internal Resolution Scale, Audio Thread, Vulkan Fast Paths, Smooth First Play |
| Content close | Optional Xbox HDD trimming |
| RetroArch process restart | RAM, Xbox network backend, AV pack, skip animation, CPU Boost, x86_64 Hardware FPU, voice-worker count, Graphics API; MoltenVK queue mode remains explicitly process-restart-only |

Construction-time fields are read by `system/vl.c`, the SMC, the CPU translator
or APU worker initialization. This port keeps those objects resident when
content closes, so some settings require a new RetroArch process. This is a
port limitation, not the general frontend convention. The Libretro
[documentation](https://docs.libretro.com/meta/core-template/#core-options)
defines `(Restart)` as closing the core and applying the value on its next
launch. It does not prescribe rejecting content when an option changes.

The experimental eight-option rejection added during this audit has been
removed. Content follows the donor's load path; construction-time changes stay
pending until the required process restart. The earlier rejection probe records
what that discarded policy did, not a correct-behavior regression test.

Hardware FPU Emulation is registered only on x86_64, matching the translator's
compile-time host-x87 path. The same filter ignores stale values on ARM hosts.

## Evidence And Interpretation

| Change | Evidence | Classification |
| --- | --- | --- |
| Free Play | Old and corrected builds tested on the same save, with visible on/off/on results | Reproduced functional bug |
| Dashboard | Old early-return path retains forced bits; native transitions and corrected real Xbox reload verified | Defect relative to the documented Dashboard behavior; author intent not established |
| Vulkan warmup | Process-static completion prevents subsequent renderer warmup; native instances and off/on real reload verified | Defect relative to the option's next-launch behavior; author intent not established |
| Audio flush/fraction | Actual function tests and both frontend audio paths | Lifecycle/contract hardening; no audible malfunction reproduced |
| Rumble reset | Actual callback/state tests | Lifecycle hardening; physical controller behavior not tested |
| Hardware FPU visibility | Translator compilation excludes host x87 on ARM | UI capability correction, not an emulation failure |
| Eight-option load rejection | Introduced by this fork during the audit | Discretionary policy, now removed |

Tests establish the effects listed above, not the original author's intent.
Only the evidence actually collected is claimed here.

## Retained Changes

- **Dashboard aspect ratio:** previously returning to Dashboard skipped the
  write, leaving the previous in-memory widescreen/letterbox override. Read the
  saved EEPROM's video bits, preserve unrelated live flags, and recompute the
  checksum through the existing writer. Retain the initial flags if the file
  cannot be read. Skip the path for arcade content. Bounds/alias-safe
  little-endian accesses replace the old intermediate out-of-bounds pointer.
- **Synchronous audio:** the first-pull flush was a function-local static that
  remained complete after the first game. Use the existing per-load flush flag
  for either audio consumer and reset the fractional sample accumulator on
  load. Both boot silence and active audio deliver 800/801 stereo frames per
  `retro_run` at 48 kHz / 59.94 Hz. Keep the APU queue bound unchanged.
- **Rumble:** stop all four ports at content close and clear the retained
  guest request plus the last-sent cache. Start each new frontend interface
  with cleared output, allowing an identical new request to be delivered.
  This also prevents a driving game's last force-feedback value leaking into
  a later arcade title without a drive board.
- **Vulkan warmup:** the service's process-static `done` suppressed every
  later renderer's seed warmup. Store it in `PGRAPHVkState` and initialize it
  with that renderer's shader resources. Preserve the donor's seed loader,
  disabled setting and work scheduling; change only the completion lifetime.

The live audio, input and force-feedback selectors already have consumers;
those paths are retained. Host-specific options are filtered by capability,
without creating separate emulator implementations for the dedicated cores.

## Verification And Limits

`scripts/test-libretro-option-lifecycle.py` compiles the actual functions under
ASan/UBSan, with undefined behavior terminating the test. It checks repeated
forced/Dashboard video transitions, changed saved flags, preserved unrelated
EEPROM data, checksums and arcade exclusion; two audio loads and 10,000 fractional sample counts; rumble
on/off/unload/new-interface behavior; and independent shader-service state
for two renderer instances. The first sanitizer pass exposed the pre-existing
EEPROM pointer-formation issue, corrected above.

`scripts/test-libretro-profiles.py` covers all profiles, translated and English
tables and option APIs 0/1/2, including absent Hardware FPU on this ARM64 host.
The existing audio and Free Play regressions are also rerun. Dedicated macOS
ARM64 option counts are Xbox 24 and Chihiro 22; optional Full has 28.

Real frontend probes use the existing OutRun 2 Xbox disc and OutRun 2 Special
Tours Chihiro CHD, in place, with private writable support files. They cover
16:9 → Dashboard and synchronous → threaded audio for Xbox; disabled → enabled
Smooth First Play with synchronous audio on both Chihiro loads; and a changed voice-worker request. The original refusal test belongs to the
discarded policy; a follow-up confirms that the request no longer prevents a load. These checks do not certify physical rumble,
all games, subjective audio quality or Windows/Linux execution.

Both real frontend processes exit with code 0, join their emulation threads,
and preserve original configuration, EEPROM/save hashes and source-media
metadata. Xbox logs restore `00010000` to `00000000`; the second capture is
4:3. The second Chihiro load reports warmup covering 294 shader siblings and
442 states after the first load had it disabled. Both synchronous Chihiro loads
submit only 800/801-frame batches. The earlier third-load refusal is preserved only as historical evidence of the
removed policy. It is not classified as an upstream bug or desired behavior.

Builds remain based on HEAD `0f07a84bb5` with local changes. Evidence, patch and
binary manifests live under `build/option-lifecycle-20261028/`. The runtime manifest records the tested
package; the final rebuild also uses QEMU's UTF-8-aware file opener on Windows
(the same POSIX file access on macOS). Keep source
configuration, firmware and saves unchanged; remove temporary profiles, test
libraries and the fresh build tree after installing both normal local cores.

## Rejection Policy Review

The follow-up evidence is in `build/option-lifecycle-review-20261028/`.
The same RetroArch process accepts a first Chihiro load requesting one voice
worker and a second requesting two, then exits normally. This confirms loading
is no longer blocked; it does not claim that the resident APU worker pool was
rebuilt. Both normal local core packages are updated and checked by hash,
code signature and metadata probe. Source configuration and saves are unchanged.
