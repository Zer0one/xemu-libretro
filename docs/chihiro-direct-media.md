# Direct Chihiro Disc Loading

Status: native optical-to-DIMM integration, 2026-10-27. Chihiro and Full accept
FATX netboot `.bin`, supported GD-ROM `.chd`, `.cue`/BIN, `.gdi`, MODE1 `.iso`
and raw MODE1 `.bin` content. The Xbox profile retains ISO/XISO input. File
contents distinguish optical BINs from FATX netboot images. A generic
ISO9660 signature admits a candidate; security-loader and decrypted FATX/boot.id
checks establish supported Chihiro content before emulation starts.

## Native Loading Path

`ui/libretro-chihiro-disc.c` owns the selected read-only disc. It uses
`ui/libretro-chihiro-optical.c` with the pinned libchdr adapter or descriptor/
raw readers, selects the high-density MODE1 volume, bounds
ISO9660 traversal, finds the security dump's loader and its FATX payload, and
verifies decrypted FATX geometry before QEMU or graphics startup. Directory
and record limits, extent/endianness checks and cycle checks reject malformed
input. Unsupported track layouts receive an explicit load error.

The reader supports CHGD, CHT2 and CHTR metadata, MODE1/MODE1_RAW,
2048/2352/2448-byte CHD units, four-frame track padding, and stored pregaps
marked by MAME's `V` convention. It selects track 3, or a unique data track.
The five archived OutRun 2/SP CHDs previously tested use CHGD track 3,
2448-byte units and no track-3 pregap/postgap.

CUE supports BINARY FILE, MODE1/2048, MODE1/2352 and AUDIO tracks, INDEX 00/01,
virtual PREGAP/POSTGAP and Redump SINGLE-DENSITY/HIGH-DENSITY markers. GDI
supports 2048/2352-byte tracks and file offsets. Quoted filenames, spaces,
relative track paths and separate track files are supported; tracks sharing
a BIN must use the same sector stride. Plain ISO/raw BIN input must contain a
MODE1 volume at logical sector 16. GD absolute extents use the high-density
base of 45000; ordinary ISO extents use zero.

Only the selected contiguous data track supplies the ISO filesystem. Split
filesystems spanning multiple data tracks, MODE2, WAVE audio files and parent/
delta CHDs are unsupported. Descriptor files are limited to 512 KiB/99 tracks;
each image/track is bounded to 16 GiB. All referenced tracks must exist and
have valid sizes. Unsupported or malformed layouts receive a load error.
Synthetic CUE/GDI/CHD/raw tests cover these boundaries and encrypted payload
loading. OutRun 2 (World, Rev A) also passed a real Redump CUE/BIN game boot;
GDI remains covered by synthetic fixtures rather than a real GDI set.
Select the CUE with its referenced BIN files extracted together. Compressed
game ZIPs are not frontend content; ZIP support is for security/firmware files.

The existing Sega DES implementation decrypts bounded chunks directly into
the destination DIMM. No converted game image or full decrypted host copy is
created. The plaintext view also supplies size, `boot.id`/region checks,
reload capacity checks and snapshot identity/reconstruction. Snapshot format
and the donor's ordinary FATX file path remain intact.

Preparing/replacing the source happens while the previous emulation thread
is stopped. Guest-side startup/reload/snapshot reads are serialized by the
BQL. Content close joins the frontend emulation thread before closing the
source. No extra pacing thread, sleep or graphics synchronization is added.

## Dependencies And Portability

`subprojects/libchdr.wrap` pins upstream revision
`47d794151b2b64f6aba60304e6480f4f5a2bc24b` and its archive SHA256. The small
Meson overlay builds upstream's portable decoder sources, LZMA and zstd
decoder statically, reusing xemu's zlib. It introduces no host-specific zstd
dylib and no second miniz implementation. Existing miniz is compiled once for
ZIP security dumps on Chihiro/Full, as well as its prior Windows use.

Only Chihiro/Full link the CHD/security components. Xbox gets the inactive
adapter boundary and keeps its existing package dependencies. The code is
portable C; Windows/Linux/Android runtime validation has not been performed
for this new path. Android release builds remain excluded.

The first probe with libchdr 0.3.0 opened OutRun 2 but failed a subsequent
header check. That release's public header reader leaves `file_size`
uninitialized. Upstream fixed this in
[`798a4f72`](https://github.com/rtissera/libchdr/commit/798a4f72fecd1855987fda2413ef44c10a72e689).
The pin includes that fix and later upstream checks. No local libchdr patch
was introduced. The failed probe log is retained as historical evidence.

Format references:

- [MAME optical formats and density/pregap conventions](https://github.com/mamedev/mame/blob/master/src/lib/util/cdrom.cpp)
- [Tovarichtch/chihiro-netboot](https://github.com/Tovarichtch/chihiro-netboot)
- [Libretro loading contract](https://docs.libretro.com/development/cores/developing-cores/)
- [Flycast CHD build integration](https://github.com/libretro/flycast/blob/master/Makefile.common)

Tovarichtch's loader/security format is adapted natively, with MIT attribution
in `licenses/chihiro-netboot.license.txt`. The shared DES keeps its BSD-3-Clause
license. Python/chdman are not runtime dependencies. There are no runtime
downloads.

## Firmware And Security Files

The active installation uses the external frontend system directory. The
following firmware was originally extracted without changing its contents.
On 2026-10-28, at the developer's request, the installation switched to the
unchanged MAME `chihiro.zip` and `seganetw.zip` archives under
`system/xemu-chihiro/`, retaining the member names below. Keys
remain under `keys/` / `redump/`; writable `chihiro_eeprom.bin`, saves and
snapshots remain outside the archive.

| Firmware Member / Key Path | MAME Source |
| --- | --- |
| `chihiro_xbox_bios.bin` | `bios/chihiro.zip` |
| `fpr21042_m29w160et.bin` | `bios/chihiro.zip` |
| `ic10_g24lc64.bin` | `bios/chihiro.zip` |
| `ic11_24lc024.bin` | `bios/chihiro.zip` |
| `pc20_g24lc64.bin` | `bios/chihiro.zip` |
| `ver1305.bin` (optional network board) | `bios/seganetw.zip` |
| `keys/outr2.data` | `roms/outr2.zip`, `317-0372-com.data` |
| `keys/outr2st.pic` | `roms/outr2st.zip`, `317-0396-com.pic` |

### Firmware Lookup

The firmware resolver searches independently for each board file:

1. System locations, in order: `system/<profile>/`,
   `system/<profile>/Chihiro/`, legacy `system/xemu/Chihiro/`, then `system/`.
   Each location supports loose firmware, the relevant BIOS set ZIP, and its
   set folder. Profile is `xemu-chihiro` or `xemu-full`.
2. The associated game ZIP/folder beside the content, then beside its containing
   directory. CHD identities use the same MAME set/parent association as keys;
   content basename and directory name are also aliases.
3. Shared `chihiro.zip` / `chihiro/` and `seganetw.zip` / `seganetw/` in those
   two content-relative roots, then loose firmware in each root.

The BIOS, media-board flash and baseboard EEPROM dump filenames remain MAME
names listed above. The media flash also accepts the donor's aliases
`fpr-23887_29lv160te.ic4` and `fpr-23887.bin`. `ver1305.bin` comes from the
`seganetw` set. Firmware contained only in an unmerged game's shared BIOS set
therefore does not have to be duplicated into its game ZIP.

Keep the two standard MAME sets separate: `ver1305.bin` is read from
`seganetw.zip`. Firmware ZIP loading materializes verified immutable files
under the profile's `firmware-cache/` because the shared machine code consumes file paths. The
archive is the maintained source; loose top-level firmware copies are not
needed. The local switch checked source-archive identity, ZIP CRC, all six
firmware SHA-256 values and the native resolver before installing the two
sets. Machine EEPROM, snapshots and saves remain unchanged.

ZIP input is read-only, bounded to 64 MiB / 4096 members, and only recognized
firmware names/sizes are extracted with CRC checking. Required BIOS absence
fails before machine/graphics startup; ancillary firmware retains donor
handling. Selected source and resolved path are logged. Frontend `.info`
entries mark loose paths as optional alternatives because static metadata
cannot express this search; this does not make the BIOS optional.

The existing machine code consumes paths, so ZIP members are materialized
under `system/<profile>/firmware-cache/<SHA256>/<canonical filename>`.
The raw-byte hash selects an immutable cache entry; intact files are reused,
and a corrupt entry is repaired atomically. No ZIP entry path becomes a host
output path, and source archives are not modified. Loose firmware is read in
place. Runtime EEPROM, cards and saves retain their existing profile paths;
firmware cache files are read-only inputs to that runtime logic. Cache files
can be removed while the core is stopped and are rebuilt from source on load.

There is no proprietary key format. The native reader accepts an 80-byte
`.data` dump, a supported 16 KiB PIC dump (also the reference's 16 extra bytes),
or a bounded security ZIP containing them. Both DES bytes and the loader
filename are needed. No key bytes are logged.

The MAME route uses these search roots, in order:

1. The directory containing the CHD.
2. The parent of that directory (beside the directory containing the CHD).
3. `system/xemu-chihiro/keys/` (`system/xemu-full/keys/` for Full).

Within each root, search the associated set ZIP, the associated set directory,
then loose files directly in the root. Merged clone sets may also use their
MAME parent when it has the same security dump. The content basename remains
a supported alias, including existing `keys/outr2.data` / `keys/outr2st.pic`.
`.data` precedes `.pic` inside each location. Formats remain unchanged and
ZIP members are read without extracting to disk.

`ui/libretro-chihiro-media-table.h` maps 47 MAME CHD identities to set names,
parent sets, security filenames and security SHA-1 values. CHD identification
uses the declared combined SHA-1 in its header first, then the filename.
This is an identity lookup, not a fresh whole-disc checksum audit. The table
is generated from the pinned [MAME source](https://github.com/mamedev/mame/blob/4f44364b25efebc20359f5fef337933aa0028b0e/src/mame/sega/chihiro.cpp)
by `scripts/gen-chihiro-media-catalog.py`; it includes associations for media
layouts the current decoder does not yet support and does not imply playability.

A complete security SHA-1 pass over all roots precedes name fallback, so a
renamed matching dump takes priority over a named dump with a different hash.
In the fallback, only associated filenames/aliases are tried. The SHA-1 is
computed from the original dump bytes, including any optional PIC trailer.
A differing SHA-1 produces a frontend notification and log warning but does
not by itself block loading. Unknown media loaded by name also receives an
unverified-key warning. Format/ZIP CRC checks, ISO loader matching, decryption
and full FATX/boot.id validation still apply; candidate keys are tried in search order.
No key bytes are logged. A malformed container is skipped, rather than hiding
another complete source. Scans are nonrecursive and bounded to 4096 candidate dumps per directory / ZIP members; set ZIPs are limited to 64 MiB (including unrelated MAME board firmware).

For the usual MAME layout, `romroot/outr2/gdx-0004a.chd` finds
`romroot/outr2.zip` or `romroot/outr2/317-0372-com.data`. A flat CHD beside
`outr2.zip` works as well. Multiple arbitrary MAME `rompath` roots are not
exposed by the Libretro API: security sets stored elsewhere can be placed or
linked under the profile's `keys` directory. ROM archives are not moved.

### Image-Name Fallback (Redump-Style)

If no MAME set can be associated by declared CHD SHA-1/name, or the MAME route
cannot supply a key that passes decryption/content validation, try:

1. The directory beside the selected image/descriptor.
2. `system/xemu-chihiro/keys/`.
3. `system/xemu-chihiro/redump/`.

Full uses `system/xemu-full/keys/` and `system/xemu-full/redump/` instead.
The basename is that of the selected CHD/CUE/GDI/ISO/BIN, with its final
extension removed; spaces and dots are retained. Each root tries:

1. `<basename>.zip`, all supported DATA then PIC members.
2. `<basename>/`, all immediate DATA then PIC files in sorted order.
3. `<basename>.data`, then `<basename>.pic` directly in the root.

Names inside the associated ZIP/folder need not match the image. This route
does not search the image directory's parent or guess unrelated loose keys.
The same archive, file-size and candidate limits apply, but candidates stream
through validation rather than stopping at a fixed 16-key buffer. A valid
later key remains eligible after earlier keys fail. ZIP CRC is checked before
a member is tried. Searching stops on a key that produces a valid loader,
FATX geometry, BTID/XBAM boot ID and an unambiguous DIMM size. This is a
content check, not a claim that an uncatalogued title is playable. The key's
SHA-1 is recorded; a missing/mismatched catalogue checksum warns without
blocking otherwise valid content. No decrypted game copy is written.

The installed security filenames identify their game sets; bytes/formats are
unchanged. Both security dumps match MAME SHA1. The BIOS matches the hash
recognized by the donor; ZIP CRC, lengths, destination hashes and provenance
are recorded in local `system-install.json`. Source archives remain intact.
The CHDs are read from My Passport without duplication.

## Uniform Title Catalogue And DIMM Selection

The same path applies to every title and to both CHD and FATX input. Shared
`hw/xbox/chihiro/chihiro-titles.h` contains all 14 donor boot-ID families and
preserves their existing MAME/save names. It is the single catalogue for the
board and the frontend, rather than an OutRun-only list.

| Boot ID | Title | Reference DIMM | Evidence Status |
| --- | --- | ---: | --- |
| SBFN | The House Of The Dead III | To verify | No per-title hardware value assumed |
| SBFY | Crazy Taxi High Roller | 512 MiB | SEGA UK manual System Information example; [CTHR-UK](chihiro-dimm-sources.md) |
| SBFZ | Virtua Cop 3 | 512 MiB | SEGA Deluxe manual System Information example; [VC3-DLX](chihiro-dimm-sources.md) |
| SGBZ | OutRun 2 | 512 MiB | GDX-0004A FATX layout and core boot/attract |
| SBHC | Sega Network Taisen Mahjong MJ 2 | To verify | No per-title hardware value assumed |
| SBHF | Ollie King | 512 MiB | SEGA 999-2026, p.53; [OLLIE-DLX](chihiro-dimm-sources.md) |
| SBHQ | Wangan Midnight Maximum Tune | To verify | No per-title hardware value assumed |
| SBHU | Ghost Squad | 512 MiB | SEGA 4201-6831-01, p.68; [GHOST-DLX](chihiro-dimm-sources.md) |
| SBJK | Gundam Battle Operating Simulator | 1024 MiB | Donor config requirement for its 603 MB image; not a physical-cabinet audit |
| SBJE | OutRun 2 Special Tours | 512 MiB | SEGA SP manual and GDX-0011/0011A/0014/0014A layouts |
| SBKD | Wangan Midnight Maximum Tune 2 | 512 MiB | MAME observed hardware: Japan Rev A, JP5 2-3; [MAME-WMMT2](chihiro-dimm-sources.md) |
| SBKK | Sega Network Taisen Mahjong MJ 3 | To verify | No per-title hardware value assumed |
| SBLF | Sega Club Golf 2006 Next Tours | To verify | No per-title hardware value assumed |
| SBME | Sega Network Taisen Mahjong MJ 3 Evolution | To verify | No per-title hardware value assumed |

The catalogue now has eight evidence-backed references and six unresolved
entries. [Source coverage and limitations](chihiro-dimm-sources.md) distinguish
manufacturer examples, MAME board observations and local/donor evidence.
No new game boot is claimed from a manual entry.

These references distinguish documented or tested configurations from missing
hardware evidence. They do not claim that a title has only one possible DIMM
variant. The [SEGA SP owner's manual](https://manuals.plus/m/e23ecf82da858b0cd5b48f512fa4a7944b7063ee9026934b34f189373f7bbf74)
shows a Type 3 board with 512 MB. Main RAM remains separately fixed at 128 MiB.

One portable parser, `chihiro-image-layout.c`, checks the donor's FATX mount
geometry for 128/256/512/1024 MiB boards, follows the root's boot.id entry,
and validates BTID/XBAM. Exactly one matching layout is required. The chosen
DIMM therefore comes from the image's actual filesystem geometry for every
title, including catalogue entries awaiting hardware evidence and unlisted
boot IDs. There is no fallback that guesses from compressed or plaintext file
length. Missing/ambiguous boot layouts are refused before guest startup.

A file with no game is distinct: firmware-only startup retains the donor's
empty-board 512 MiB default. The guarded standalone donor Auto path and
explicit board configuration are retained. Supported root-directory handling
matches the donor's first root cluster; additional filesystem variants need
separate validation.

## Validation

Repository focused checks:

- `scripts/test-libretro-media.py`: 14 admission fixtures under ASan/UBSan.
- `scripts/test-libretro-chihiro-layout.py`: 65 ASan/UBSan cases covering all
  14 catalogue IDs plus an unknown ID, all four DIMM capacities, ambiguity
  and malformed input. Ten raw-file adapter cases confirm that files with
  identical lengths select different capacities from their geometry.
- `scripts/test-libretro-chihiro-keys.py`: 20 format/archive cases plus 512
  differential comparisons with the reference's pure security parsers.
- `scripts/test-libretro-chihiro-des.py`: independent CommonCrypto known-answer
  and 256 Sega byte-order comparisons against existing DES.
- `scripts/test-libretro-chihiro-chd.py`: 144 synthetic stream, metadata,
  boundary and repeated-open cases against pinned libchdr.
- `scripts/test-libretro-chihiro-disc.py`: actual native ISO/security/DES path,
  full payload hash comparison, unaligned/end-of-image reads, bounds, missing
  keys and preserving the previous source on preparation failure. A focused
  shared test build exports this internal interface; release cores do not.
- Native library coexistence: all three profiles, correct extensions/options,
  local symbols and eight pre-graphics refusals. No guest runs in that probe.

Five archived compressed CHDs pass full native payload reads/decryption and
match the independent probe's SHA256. Separate checks locate `boot.id` through
FATX, validate BTID/XBAM and `OUTRUN2.XBE`, and check its XBEH header. Stored CHD
header SHA1 identities match MAME's driver; this is not a whole-disc digest
recomputation. No decrypted game image is written.

Actual RetroArch/Vulkan OutRun 2 CHD testing loads 377360384 plaintext bytes
into the 512 MiB DIMM, accepts Export region, executes OUTRUN2.XBE and displays
the AM2 introduction and attract scenes (snow, Ferrari, title). A white image
captured earlier was an introduction transition; later captures show the
scene and title progressing. The initial boot/attract probes used the null frontend audio driver and private
writable state. A later macOS ARM64 keyboard run selected a car, automatic
transmission and music, then drove in Palm Beach until Time Over. Observed
System Menu -> JVS Test -> Input Test captures confirm coin count 0 -> 1,
steering movement, accelerator channel 2 and brake channel 3 reaching FFFF.
This tests keyboard routing, not physical controllers or analog trigger travel.

The later run used CoreAudio (hardware output 44100 Hz) and recorded 48000 Hz
stereo PCM. A 20-second gameplay sample measures -4.01 dBFS peak and -19.13
dBFS RMS; this confirms non-silent, non-saturated output, not subjective audio
quality or freedom from dropouts. The frontend WAV recorder wrote a padded
56-byte header; representative clips were remuxed without changing samples.

Closing content flushed the 56648-byte version-2 arcade save and joined the
emulation thread. A same-process reload completed load and resumed telemetry;
a fresh-process run visibly returned to attract after loading a copy of that
save. The final input-test run closed normally with exit 0. The earlier long
recording run also exited 0 without forced termination, after its initial
10-second QUIT wait expired and QUIT was retried. An initial black image after
a fast, unverified test-menu key batch did not recur on the observed JVS route.
Savestate/reset, physical listening and cross-platform campaigns remain open.

Local evidence is retained under `build/chihiro-media-20261027/` and
`build/profile-separation-20261027/`, with normal-exit reports, captures,
source/build hashes and installed package records. No additional media are
needed for the current OutRun 2 checks.

For the subsequent controlled gamepad-drift reproduction and fix, see
[Driving Steering Deadzone](chihiro-drive-centering.md).

## Optical And Image-Name Fallback Verification (2026-10-27)

- 41 native media/key-search cases cover MAME precedence, all fallback roots,
  arbitrary names inside associated containers, DATA/PIC priority and a valid
  21st key after 20 rejected candidates.
- 26 native optical/container cases cover raw MODE1, CUE/GDI descriptors,
  INDEX00/01, quoted names, GD density markers, shared BIN boundaries, CHGD/
  CHT2/CHTR metadata and stored pregaps. Independent CommonCrypto encryption
  supplies synthetic ISO/FATX/boot.id fixtures for end-to-end CUE/GDI tests.
- 18 admission fixtures, 19 firmware lookup cases and 1332 Core Options
  registrations pass; Xbox and Chihiro macOS ARM64 libraries build.
- A renamed real MAME OutRun 2 CHD boots using only an image-named DATA dump
  under private System/redump, reaches introduction/attract and exits normally.
- The developer subsequently supplied Redump ZIP archives for OutRun 2 and
  Crazy Taxi High Roller. Only OutRun 2 (World, Rev A) was temporarily extracted,
  with explicit authorization, and all four members passed ZIP CRC validation.
  Its CUE selects high-density MODE1/2352 track 3 and `BGZ.BIN` resolves the
  512 MiB DIMM payload. The 377360384-byte decrypted payload has SHA-256
  `385d4a4bc0d765bb8d29160ac0ead222263be51a307865fe90fafbf2dfae134b`,
  identical to the independently recorded MAME CHD result.
- One RetroArch/Vulkan CUE run shows the Ferrari license introduction and
  attract sequence, closes content and exits with status 0. This is boot/
  attract evidence, not a gameplay/audio/performance validation. Crazy Taxi
  was inventoried read-only and was not extracted or booted.
- Both normal cores and matching metadata are installed locally, with code
  signatures/dependency hashes checked. The real external System/redump now
  also has `OutRun 2 (World) (Rev A).data`, copied from the existing 80-byte
  DATA dump without changing its format or original. User configuration,
  EEPROM and saves remain unchanged. Temporary builds/media/runtime files are
  removed; results and representative captures are under
  `build/redump-path-20261027/`.

## Remaining Work

- Real GDI game boot and additional track layouts beyond the supported
  contiguous MODE1 volume.
- Fill remaining per-title hardware reference values using primary evidence;
  the common geometry-based runtime mechanism already applies to every title.
- Physical-controller/audio-listening checks, other Chihiro games, extended
  persistence/reset checks and experimental snapshot round-trips.
- Linux/Windows runtime checks for the newly integrated path.
