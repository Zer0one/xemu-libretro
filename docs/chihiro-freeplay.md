# Chihiro Free Play And Content Reload

## Behavior

`xemu_chihiro_freeplay` is applied when content starts. Change **Free Play
(Restart)**, then close and reopen the content; restarting RetroArch also
works. Changing the selector alone does not switch a running game's coin mode.
There is no need to delete cabinet saves, reset NVRAM or edit firmware dumps.
The selected value is logged as `Chihiro Free Play for this load`.

## Cause And Correction

The user log `retroarch__2026_10_28__15_01_06.log` identifies OutRun 2 Special
Tours Rev A (`outr2st/gdx-0014a.chd`) on core commit `0f07a84bb`, with three
loads in one process. The saved core option is disabled, while the cabinet
save's two ACBU coin records still contain mode 10 and Free Play flag 1.
The old log does not record the selected Free Play value for each load;
it cannot establish exactly when the user changed it.

The Wizzard donor copies `g_config.chihiro.settings.freeplay` into
`chihiro_freeplay_setting` during `xbox_init_common()`. Libretro creates the
QEMU machine only once per process. Subsequent loads update `g_config`, then
reuse and reset that machine, so its EEPROM override still reads the first
load's copied value. The defect is reproduced independently below; identifying
it does not depend on guessing the user's menu actions.

Read the current configuration directly in `chihiro_apply_freeplay()` and
remove the redundant startup copy. Keep the donor's EEPROM layout, two-slot
coin-mode conversion, payload checksums and save format. Apply through the
existing realization, save-load and USB-reset paths. This shared correction
requires no OS, game-name or dedicated-profile special case and is suitable
for an upstream Libretro pull request.

## Bounded Verification

The native test `scripts/test-libretro-chihiro-freeplay.py` compiles the real
EEPROM override under ASan/UBSan. The previous function fails when the setting
changes on a retained machine; the corrected function passes repeated
on/off/on/off transitions, checks both ACBU slots and their checksums, and
preserves all unrelated EEPROM bytes and non-ACBU firmware.

Real-frontend verification uses macOS ARM64, RetroArch 1.22.2, Vulkan/MoltenVK
Sync and the same OutRun 2 Special Tours CHD read in place. Each build gets a
private copy of the existing cabinet save, DIMM system state and EEPROM.
A small proxy supplies the selected option at each load; no guest code or
save data are patched. Content is closed/reopened through RetroArch's normal
[LOAD_CONTENT command](https://github.com/libretro/RetroArch/blob/master/command.c),
which takes core and content paths separated by `|`.

| Build | Requested On First Load | Requested On Second Load | Second-Load Result |
| --- | --- | --- | --- |
| Previously installed | Enabled | Disabled | Still Free Play; both records mode 10 / flag 1 |
| Corrected | Enabled | Disabled | Coin operation; both records mode 1 / flag 0 |

The corrected build also returns to Free Play on a third load with the option
enabled. Captures show `FREE PLAY` → `CREDIT(S) 0` → `FREE PLAY`; both persisted
coin records have valid checksums after every stage. Both baseline and corrected
processes close normally and join every emulation thread. Original media metadata, user options, EEPROM and save
hashes are checked before and after the tests. No broad game-compatibility or
Windows/Linux runtime claim follows from this targeted macOS check.

The builds retain HEAD `0f07a84bb5` because this fix is uncommitted. Distinguish
them using `build/chihiro-freeplay-20261028/build-manifest.json`, the recorded
source patch and binary hashes. `runtime-build-manifest.json` identifies the
three-load runtime candidate; the final rebuilt package also adds the per-load
Free Play log line, without changing the tested setting logic. Keep compact logs, result JSON and representative
captures there; remove private writable fixtures, the proxy library and the
fresh Chihiro build tree after deployment.

## First Boot And Title Isolation

A fresh title now starts with its own private state before the existing save is
loaded. Only valid factory `ACBU0001` records are assigned the identifier from
that game's `boot.id`, with their payload checksums recomputed. This applies to
the in-memory factory seed, never to an existing user's save or the firmware
archive. Non-ACBU layouts are left to their games to initialize.

This prevents OutRun 2 from rejecting the OutRun 2 SP identifier in the common
firmware seed and replacing the requested Free Play mode with coin defaults.
No periodic EEPROM rewriting or automatic second boot is introduced; later
service-menu writes keep their normal behavior.

The same initialization path runs at first boot and when changing titles in a
retained machine. It clears only the QC EEPROM and saved backup-SRAM windows,
preserving 8051 work RAM and mailbox state. A valid existing save then replaces
that fresh state. Missing or incomplete files cannot retain another title's
backup; loaded state is committed only after the complete payload of an accepted
version 1/2 file has been read.
The `.sav` version, title filenames, revision/region grouping and shared
firmware lookup remain unchanged.

`scripts/test-libretro-chihiro-persistence.py` covers factory owner/checksum
creation, title isolation with no save, transactional truncated-file loading,
version 1/2 compatibility, and firmware/work-RAM/mailbox preservation under
ASan/UBSan. Runtime evidence is recorded separately from these native checks.


The corrected macOS ARM64 core completed this real RetroArch sequence in one
process, starting with no game saves: OutRun 2 enabled → OutRun 2 SP enabled →
OutRun 2 disabled. Captures and both persisted coin records agree: Free Play,
Free Play, then coin operation; title owners are SGBZ, SBJE, SGBZ and all payload
checksums are valid. The process exited normally and protected user-file hashes
were unchanged. The released baseline instead produced coin mode in both fresh
titles with the option enabled. An earlier candidate run exited during its
second title before capture; the complete repeat did not reproduce it. Its log
is retained as a limitation, not treated as a successful run.

The Batocera pre-seeded-state experiment validated the first-boot initialization
policy using the unchanged released Linux binary. A corrected Linux binary has
not yet been runtime-tested; the modified-source runtime sequence above is
macOS only. Records, captures, local build/source hashes and cleanup receipts
are in `build/chihiro-eeprom-fix-20261028/`.
