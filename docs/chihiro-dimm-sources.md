# Chihiro DIMM Source Register

Research date: 2026-10-27. This register supports the shared 14-family title
catalogue in `hw/xbox/chihiro/chihiro-titles.h`. Eight entries have a reference
capacity; six remain unresolved. It does not replace image-layout validation.

## Interpretation

A manufacturer's illustrated System Information screen documents a reference
configuration for that manual's cabinet. It is not proof that every revision
shipped only with that capacity, nor a minimum/maximum requirement. A MAME
comment describing an inspected board is stronger evidence for that exact
board/revision than a hard-coded value in emulation code.

512 MiB and 1024 MiB images can carry different FATX layouts for the same game.
The runtime continues to select the unique layout confirmed by FATX/boot.id,
including variants differing from the catalogue reference. No value is copied
from a CHD's compressed size, a buffer allocation or a download filename.

## New Online References

| Key | Catalogue ID | Reference | Primary Material And Scope |
| --- | --- | ---: | --- |
| CTHR-UK | SBFY / crtaxihr | 512 MiB | SEGA Crazy Taxi High Roller UK manual, section 6.2.4 System Information; illustrated media-board memory |
| VC3-DLX | SBFZ / vcop3 | 512 MiB | SEGA Virtua Cop III Deluxe owner's manual, Assembling Check p.38 and System Information; Type 3 illustration |
| GHOST-DLX | SBHU / ghostsqu | 512 MiB | SEGA Ghost Squad Deluxe owner's manual 4201-6831-01, November 2004, System Information printed p.68 (PDF index 73) |
| OLLIE-DLX | SBHF / ollie | 512 MiB | SEGA Ollie King 29-inch Deluxe owner's manual 999-2026, May 2004, System Information printed p.53 |
| MAME-WMMT2 | SBKD / wangmid2 | 512 MiB | MAME hardware notes explicitly taken from Wangan Midnight Maximum Tune 2 Japan Rev A: JP5 at 2-3, one 512 MB module |

### Source Links And Access Record

- **CTHR-UK:** [SEGA manual mirror](https://pixelsarcadegames.com/wp-content/uploads/2025/07/Crazy_Taxi_High_Roller_-_2003_-_Sega.pdf),
  [second transcription](https://manualzilla.com/doc/7355644/sega-crazy-taxi-3--high-roller-owner-s-manual).
  The primary manual's indexed section 6.2.4 and assembly-check illustration
  explicitly show the 512 MB reference. Direct binary retrieval returned HTTP
  406; the reviewed evidence is the indexed manual excerpt, not a locally
  inspected complete PDF. This access limitation is retained with the value.
- **VC3-DLX:** [readable manual transcription](https://manualzilla.com/doc/5999015/owner-s-manual---play-it--amusements),
  [indexed SEGA manual PDF](https://www.gamesdatabase.org/Media/SYSTEM/Arcade/Manual/formated/Virtua_Cop_3_-_2003_-_Sega.pdf).
  The transcription identifies Virtua Cop III Deluxe and contains the
  illustrated 512 MB System Information screen. The original PDF URL currently
  returns 404; use the readable mirror rather than treating it as absent data.
- **GHOST-DLX:** [SEGA manual PDF](https://device.report/m/b0fcdf75a5f8cf237490a85bc167682ac2b6549b5739bcfcbc4d7ec637e4f590.pdf).
  The web reader exposes the cover, Ghost Squad Deluxe identification and the
  complete relevant System Information page. The illustration shows 512 MB.
  Direct download returned HTTP 403; no complete local PDF was retained.
- **OLLIE-DLX:** [SEGA manual transcription](https://manuals.plus/m/e9356906969c8bfa696b1b566f901aa7e54235c74b74fa98b998ce38987c4b25).
  The cover identifies manual 999-2026, game code OLS. Section C, printed p.53,
  illustrates a Type 3 DIMM board with 512 MB. This is a manual example.
- **MAME-WMMT2:** [MAME chihiro.cpp hardware notes](https://github.com/mamedev/mame/blob/master/src/mame/sega/chihiro.cpp#L350-L368).
  Notes identify the inspected title/revision and JP5 setting. They also explain
  the 1 GiB jumper configuration, so 512 MiB is not encoded as the only
  supported image variant for the family.

These are SEGA/Namco-era manufacturer materials and MAME's original board
observations, reproduced by archive hosts. The hosting site is not the author.
No copyrighted manual pages, firmware, keys or game images were added to this
repository. Only original summaries and source links are maintained here.

## Existing References

- **SGBZ / OutRun 2:** 512 MiB image geometry and actual boot/attract from
  archived GDX-0004A in this core. This is local emulation/image evidence,
  not an independent physical-cabinet measurement.
- **SBJE / OutRun 2 SP:** 512 MiB in the
  [SEGA owner's manual](https://manuals.plus/m/e23ecf82da858b0cd5b48f512fa4a7944b7063ee9026934b34f189373f7bbf74),
  section C System Information; four archived GD revisions also pass native
  FATX/boot-file checks with that layout.
- **SBJK / Gundam B.O.S.:** the integrated donor's `config_spec.yml` explicitly
  requires a 1024 MiB DIMM for its 603 MB image. Retain this as a donor
  requirement; this research did not establish a manufacturer's specification.

## Unresolved References

| ID | Title | Result Of This Bounded Research Pass |
| --- | --- | --- |
| SBFN | The House Of The Dead III | Manuals were located, but no sufficiently reviewed primary capacity statement was obtained; no value added |
| SBHC | Sega Network Taisen Mahjong MJ 2 | No primary per-title capacity confirmation found |
| SBHQ | Wangan Midnight Maximum Tune | Operator manual located, but no per-title DIMM capacity confirmed |
| SBKK | Sega Network Taisen Mahjong MJ 3 | No primary per-title capacity confirmation found |
| SBLF | Sega Club Golf 2006 Next Tours | No primary per-title capacity confirmation found |
| SBME | Sega Network Taisen Mahjong MJ 3 Evolution | No primary per-title capacity confirmation found |

Forum download lists and general hardware specifications suggest multiple
configurations but were not promoted to per-title hardware facts. References
for Maximum Tune 3 and later concern a different platform and were excluded.
The presence of a CHD and security dump identifies media, not physical DIMM size.

MAME's generic Chihiro media-board register returns a fixed factor of 2
(512 MiB), and its DIMM loader can round a host buffer to a power of two.
Neither operation establishes a physical per-game configuration. Keep those
implementation values separate from MAME's inspected-board notes above.
