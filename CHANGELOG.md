# Changelog of the libretro core

What changed in the libretro core of this fork, newest first. Changes that come with a merge of upstream xemu are in upstream's own history; a merge is noted here with the upstream version it brought in.

## Unreleased

- Include the bundled dashboard and its dependencies' license notices in every future core package; keep the existing build workflows disabled.

- Merge WizzardSK Libretro through `84570ac21d`: frontend-threaded audio (enabled by default after review), per-player rumble and radial deadzone, standalone DSP JIT/HRTF/voice-worker options, optional bundled Xbox dashboard HDD and opt-in HDD trimming. Replace our Chihiro-only 12% steering filter with the donor's 10% per-player filter, ignoring stale retired values. Keep Xbox disk options out of the dedicated arcade profile and preserve existing user HDDs during automatic creation.
- Follow the donor's load-time Internal Resolution Scale and always-on shader cache. Adopt its Skip Boot Animation, 16:9 console video setting, Analog Stick gun aim, 100% arcade force feedback and No Pipeline Libraries defaults. Preserve macOS MoltenVK Sync as the default and the two dedicated core identities. Reset/unregister asynchronous audio state across content loads and register only after successful validation.

- Merge official xemu through `0.8.138-9` (`dc10b4cea9`), updating VulkanMemoryAllocator to 3.5.0 and adopting color-component GLSL notation while retaining the donor's back-face lighting. Keep standalone workflows and contributor documentation excluded.

- Add native Chihiro CUE/BIN, GDI, MODE1 ISO and raw optical BIN loading through the shared CHD-to-DIMM path. When MAME lookup cannot supply a working key, try image-named DATA/PIC files, ZIPs and folders beside the image, then in the profile's System `keys/` and `redump/` directories. Try all supported dumps in each named container, prefer DATA over PIC, and accept a key only after decrypted FATX/boot.id validation. Keep Xbox ISO/XISO behavior unchanged. OutRun 2 (World, Rev A) boots from its Redump CUE/BIN into introduction/attract; its full decrypted payload matches the MAME CHD reference. GDI game boot remains unverified.

- Refine profile-specific Core Options: hide Xbox System Aspect Ratio in the dedicated Chihiro core and skip its EEPROM write for Chihiro content in Full. Group Chihiro Free Play under System and steering/light-gun/rumble settings under Controls; retain the separate arcade category in Full. Clarify Chihiro's output aspect ratio and remove the unused core Display Filtering selector. Ignore stale values for removed or excluded options.
- Load Chihiro firmware from System (including the profile's `Chihiro/` subfolder), associated game sets, then shared MAME `chihiro` / `seganetw` ZIPs or folders beside content and its directory. Keep System precedence per file, read archives unchanged and cache extracted firmware under the profile root by checksum. Avoid frontend firmware checks rejecting valid alternative layouts; retain runtime EEPROM/save locations.
- Resolve Chihiro security dumps beside the CHD, beside its containing directory, then in the profile's system keys directory. Support associated MAME ZIPs, set folders and loose dumps through a pinned CHD/set/security SHA-1 catalogue. Prefer matching hashes before filename fallback and DATA before PIC in each location; notify and log unverified key hashes without blocking valid decryption. Preserve existing short key filenames and merged parent sets.
- Diagnose OutRun 2 drive-board centering failures caused by resting-stick drift and preserve the controlled reproduction. The original driving-only 12% fix is superseded by the donor per-player filter described above; frontend and core filtering remain cumulative.

- Add sourced 512 MiB DIMM references for Crazy Taxi High Roller, Virtua Cop 3, Ghost Squad, Ollie King and Wangan Midnight Maximum Tune 2 Japan Rev A. Document manufacturer-manual examples and MAME board observations separately; keep image-layout validation authoritative for actual variants and leave six unverified hardware references explicit.
- Use one title catalogue and the same validated FATX/boot.id DIMM selection for all Chihiro titles, including ordinary FATX images. Preserve all 14 donor game/save identities, record unknown hardware references explicitly, and refuse ambiguous layouts instead of guessing memory capacity from file length.
- Load supported MAME Chihiro GD-ROM CHDs directly into DIMM using native libchdr, ISO9660 and existing Sega DES code. Resolve per-game `.data`/`.pic`/security ZIP files under `system/xemu-chihiro/keys/`, preserve FATX input and snapshot image semantics, and avoid converted game files or runtime tools. OutRun 2 boots into its introduction and attract in RetroArch/Vulkan.
- Verify 512 MiB DIMM layouts for OutRun 2/SP using boot IDs and FATX metadata; preserve the donor configuration override. Keep Xbox dependencies unchanged and bundle the CHD decoder statically in arcade/Full builds.
- Show the arcade core as **Sega - Chihiro** in frontend metadata while retaining **Xemu-Chihiro** as its internal name and `xemu_chihiro_libretro` as its library stem.
- Add build profiles for Xbox (`xemu_libretro`), Xemu-Chihiro and opt-in Xemu-Full from the same source base. Dedicated cores register only their relevant options across all Libretro option APIs, ignore stale settings for the other machine, and reject wrong-profile content. Keep the current CPU/GPU/audio implementations shared. Xbox/Vulkan has reached OutRun 2 attract and closed normally. Both dedicated macOS cores are installed for local testing; broader arcade gameplay and lifecycle validation remain pending.
- Flush Chihiro backup and DIMM data through the existing writers when the guest stops on content close, without prematurely shutting down netboard workers retained for reload. Preserve the standalone exit order; real arcade save/reload validation remains pending.
- Generate matching per-profile metadata and provide a local builder and disabled CI recipes that select Xbox and Chihiro by default. Full requires explicit selection. Preserve Xbox data in `system/xemu/`; use separate writable roots for Chihiro and Full, with legacy read-only firmware lookup. Do not migrate or copy existing games, HDDs or saves.
- Preserve the new donor's deferred destruction of Vulkan surface image views. Remove an old immediate-destruction instruction retained during merge resolution, while keeping the frontend compositor wait and deferring framebuffer/view/image retirement until renderer work has settled.
- Apply GPU/RAM coherence callbacks to mapped DMA, so device transfers cannot be overwritten by older GPU surfaces. After snapshot restoration, discard old OpenGL/Vulkan surfaces and visibility reports before processing pending downloads. Preserve ordinary reset writeback and the existing snapshot format.
- Recover NVNet receive packets queued while the guest's RX ring was full. Retry on register/link changes and a 1 ms virtual-clock timer, gated by running state, link, RX and DMA readiness independently of backend transmit readiness.
- Wake QEMU's main loop when dispatching an experimental snapshot request from RetroArch, avoiding a timeout while the frontend has stopped calling `retro_run`. Save states remain unsupported in published core metadata.
- Adapt MJR's OHCI and NV2A reset corrections: write the USB frame's completion data before raising interrupts, preserve a completion head cleared by the guest, and clear stale GPU interrupt enables/pending state on reset.
- Reduce CPU work for vertex-shader fog carryover using MJR's dependency slices and a bounded cache keyed by program content. Keep the full interpreter for unusual register/constant writes and release the host cache with its PGRAPH instance.
- Bind Vulkan staging buffers directly when all active and alternate allocations are host-visible, coherent and device-local. Retain the donor's rotation/fence and buffer-growth handling, skip redundant device copies and the copy command buffer on that path, and preserve separate device buffers elsewhere. Explicitly flush noncoherent staging writes on the copy path. Selection uses allocation capabilities on every platform, without an OS or GPU-name whitelist.
- Integrate the WizzardSK Libretro update through `252a7accfb`: one core for Xbox and Sega Chihiro, including the donor's renderer/CPU updates, shader seeds, title-based aspect ratio, EEPROM video settings and frontend pause handling. Retain this fork's macOS renderer, video diagnostics, Linux lifecycle and Raven OpenGL corrections.
- Require Vulkan 1.2, matching the donor shader compiler. OpenGL remains available. Keep the donor shader-cache layout while distinguishing this fork's device-specific shader variants.
- Load Chihiro without requiring retail Xbox MCPX, BIOS or HDD files. Chihiro uses its separate board firmware and FATX netboot images; Xbox-patched Chihiro discs still run as Xbox content. OutRun 2 Chihiro boot has since been verified using the direct CHD loader described above.
- Preserve the Xbox/Chihiro separation assessment as deferred research; the integrated core uses the donor's combined architecture. Existing experimental snapshots and shader caches are not a compatibility guarantee across this update.
- Exclude Android from future release builds and packages until requested again. Default and `all` workflow selections cover Windows, Linux and macOS; retain the Android source and explicit build recipe.

## 0.8.138-3.1 - 2026-10-26

- Preserve the current Xbox-only core as the checkpoint before further upstream and Chihiro integrations. Document the dependency assessment and proposed separate Xbox/Chihiro build profiles; Chihiro support is not included in this release.
- Automatically avoid the RadeonSI point-size shader trigger on Raven GPUs in OpenGL. Keep point output intact while omitting unused geometry-shader point-size writes for lines and triangles, with separate shader cache keys; no Mesa replacement is required for this workaround.
- Document a separate Mesa 25.3.6 RadeonSI driver patch validated against the OutRun 2 OpenGL hang on Raven. The driver patch is not bundled with the core.
- Prevent APU register writes from being starved when an unpaced frontend drains audio faster than it is produced. Hand the APU mutex to pending guest voice-register writes between complete processing blocks, without adding frontend pacing sleeps.
- Deliver VBlank at the declared refresh interval on the guest virtual clock. Extra frontend calls may repeat the image instead of flooding the guest with interrupts; the frontend remains responsible for presentation and audio pacing.
- Protect the core during metadata queries as well as initialization. Linux and Android ELF builds set NODELETE at link time, preventing constructor-started RCU threads from surviving an unmapped and reinitialized library.
- Extend optional Video Debug Logging with VBlank requests, callback completion, interrupt delivery and acknowledgement counts for diagnosing guest progress separately from frontend call rates.
- Correct the setup guidance: the tested 4627 BIOS boots OutRun 2 and Halo at the retail 64 MB setting. Do not require 128 MB as a workaround for a boot failure.

## Test build 4 - 2026-10-25

- Add macOS **MoltenVK Queue Submission (Restart)** with Sync (default) and Async choices. Apply the selection to the frontend's Vulkan instance and log its provider; Async requires a compatible patched MoltenVK driver. Restart RetroArch to apply changes.

- Add optional Video Debug Logging with the standalone's integer FPS, MSPF and completed-frame NV2A counters, plus decimal event rates and rolling frame intervals. Keep guest measurements distinct from frontend calls and physical display presentation.

- Align development and build recipes with `main`, preserving separate official upstream and original Libretro port reference branches. Keep all of this fork's GitHub Actions workflows disabled until explicitly re-enabled; builds and translation updates are not published automatically.

- Reconstruct guest W depth on Vulkan devices without geometry shaders, using neutral host Z to prevent guest Z from perturbing W interpolation. Preserve guest depth clipping and bias; Z-buffer and native geometry-shader paths retain their existing behavior.

- Correct the grey horizon/building overlay in OutRun 2 on Vulkan devices without custom border colors: packed depth-texture lookups now use the guest's border color outside the selected border axes. Preserve native sampling when the extension is enabled.
- Respect the enabled portability feature for sampler LOD bias, avoiding invalid sampler creation on MoltenVK devices that do not support it.
- Keep the APU stopped through snapshot restoration, accept the standalone's snapshot metadata prefix, and log detailed snapshot errors. Save states remain experimental and are still marked unsupported in core metadata.

- Show the core as "Microsoft - Xbox (OG)" in frontend lists and runtime information. Core filenames, option keys and the system/xemu support directory are unchanged.
- Keep the isolated macOS boot probe's info/cache/log paths private and disable automatic frontend asset extraction, which can otherwise replace installed core metadata during a test.

- macOS: keep the pinned core's symbols local so RetroArch can load it again after content closes. The core remains resident for QEMU's surviving threads without making the frontend mistake a reload for a statically linked core.
- Restart diagnostic timing windows when content is loaded, avoiding an overflowing changed-picture rate inherited from the previous renderer session.

- Provide an experimental MoltenVK acquisition-fence patch and measured report: OutRun 2 reaches about 60 changed pictures/s with asynchronous host submission in the private Apple M4 probe. The normal core retains synchronous host submission; broader driver validation remains open.

- On a dedicated Vulkan renderer queue, hand the display copy to the frontend through a GPU semaphore without immediately waiting on the CPU. Keep separate compositor command buffers, descriptors and reuse fences per frontend sync index, and drain producer work before destroying its resources. MoltenVK host submission remains synchronous.

- Correct Vulkan API declarations found by validation: enable supported shader demotion and portability texture swizzles, retain a lower SPIR-V target when demotion is unavailable, and declare the compute-output buffer as a transfer source.

- Vulkan diagnostics now distinguish guest scanout flips, changed displayed pictures and frontend calls, so a 60 Hz RetroArch loop cannot be mistaken for 60 FPS gameplay.

- Merge official xemu through `0.8.138-3` (`25e1ee2fd5`), including vertex-register carryover and texture, surface-dirty tracking and Vulkan shader-target fixes. Keep the standalone UI and packaging excluded.
- Enable Vulkan on macOS through MoltenVK: convert unsupported primitives on the CPU and use hardware depth interpolation when geometry shaders are unavailable. OpenGL remains selectable; NV2A depth and flat-shading parity on the Metal path still need broader testing.
- Keep frontend drawable waits off the Vulkan renderer queue: use another graphics/compute queue or family when available, with concurrent frame-image sharing, producer semaphores and frontend sync-index waits. MoltenVK requests synchronous host submission per instance, including patched drivers; the combination restores about 60 guest flips/s in the measured OutRun 2 VSync-on probe. Keep the locked shared-queue fallback.
- Reuse unchanged Vulkan display frames instead of submitting and waiting on an identical image under the emulated GPU's lock. Memory uploads, video overlays, output recreation and changed display uniforms still redraw.
- macOS: export only the Libretro entry points, preventing incompatible glslang C++ definitions in RetroArch and the core from being coalesced and crashing shader compilation.

- macOS OpenGL: keep the picture working when RetroArch reinitializes video during game boot. The renderer now flushes and releases its old GL objects before worker contexts are replaced, preventing framebuffer assertions and white output observed with OutRun 2 on Apple Silicon.

## Test build 3 - 2026-10-12

- The console's files are also found in `system/` itself, where an older core info file had them, and the BIOS also as `Complex_4627.bin`. `system/xemu/` and `Complex_4627v1.03.bin` stay the documented place and name (boardsofcanada).
- Reduced Vulkan Buffers (Video, on by default): the Vulkan renderer's buffer for inline vertex data is a tenth of xemu's size on every graphics card, as it already was on those with less than 4 GB - about 1.2 GB less video memory and as much less system memory (3.16 to 2.01 GB in NNshi's run, frame times unchanged). Off reserves xemu's size.
- Linux: the core no longer links libX11, so it loads where X11 is absent. The one Xlib function the OpenGL renderer's GLX path called is found at run time, in the frontend's own libX11.
- Every 300 runs the core logs where a run's time went on the shared Vulkan device: how often it ran, how many of the pictures were another frame of the game's, and the time spent waiting for the renderer and in RetroArch's video and audio callbacks.
- A recipe for the libretro buildbot (`.gitlab-ci.yml`): Linux x64, Linux aarch64 and Windows x64, built by the scripts in `.ci/`; the Linux core there has libslirp linked in, as the buildbot ships one file. QEMU's own GitLab CI files are removed. A GitHub workflow replays the jobs in the same images.
- Vulkan on RetroArch's own device: the renderer draws on the device RetroArch makes with the core and each frame is handed over as an image, with no copy between two devices. The core's and RetroArch's frame times are no longer disconnected, and scrolling that was choppy on Vulkan is smooth (tested by NNshi).
- Graphics API option (Video): Vulkan by default, OpenGL as the fallback. Android always uses Vulkan, macOS always OpenGL.
- Aspect Ratio option (Video): Auto, 4:3 or 16:9. Auto follows the console's aspect line, as standalone does; 16:9 is for a game that renders widescreen but is shown squashed.
- Audio paces the core: RetroArch gets one frame of audio per run, and the sound chip waits once about 64 ms is queued.
- Rumble through RetroArch's rumble interface.
- Internal Resolution Scale works on OpenGL: the frame is handed over at the size it was rendered at. The largest picture RetroArch is told to expect follows the scale, so its shader passes are not sized for a resolution that is not in use.
- Closing content stops the emulated console and flushes its disk. Before, the console ran on and was cut off mid-write when RetroArch exited, which could break the hard disk image.
- Another game can be loaded without restarting RetroArch, on Vulkan: the emulated console stays built when content is closed, and the next game puts its disc in the drive, resets the console and starts it again (six games in a row without a crash, tested by NNshi). On OpenGL a second game is still refused with a message. Core options that need a restart, such as the memory size and Graphics API, keep the first game's values until RetroArch is restarted.
- Start Core without content boots the dashboard with an empty tray.
- The core's version names upstream's version and commit and this repository's.
- Vulkan starts on Linux and on GPUs with little memory: the surfaceless EGL display is made in a way libepoxy accepts, and the compute and vertex buffers are sized for the surface scale and the memory there is.
- CI uploads the Linux core unstripped as its own artifact, for profiling and backtraces.
- The core option texts can be translated through libretro's Crowdin project: the options header is on the standard template, with the `intl/` scripts and the upload and sync workflows. All texts are English until the first sync.
- Closing content or RetroArch no longer crashes. The core stays loaded once it has been loaded: QEMU's RCU, vCPU and nv2a threads, which upstream never joins, and its exit handlers ran on code that was no longer there once RetroArch unloaded the core.
- Redump ISOs load. A whole-disc image is attached from its game partition's offset (XGD1, XGD2 or XGD3), found by the XDVDFS volume descriptor; before, the guest was shown the disc's video partition.
- Disc images through the frontend's VFS: a `saf://` or `content://` path, as Android's Play Store RetroArch hands over, is read through RetroArch's VFS, Redump offset included.
- The core logs at start which commit of this repository it was built from.
- The core info file, matching the core, is in every build's zip.

## Test build 2 - 2026-10-07

- Merged up to upstream xemu 0.8.136-53 (478b4f4961), with its changes since February, among them the new DSP with a JIT for the audio processor; on Android, which upstream's prebuilt DSP library does not cover, the core uses the C interpreter. The core reports upstream's version.
- macOS cores, Apple Silicon and Intel. The renderer there is OpenGL, through contexts shared with RetroArch's: MoltenVK has no geometry shaders, which xemu's Vulkan renderer needs.
- The repository holds the core only: the standalone app's UI, packaging and CI are removed (`.upstream-excluded`), and the core is built without SDL. The Xbox Live Communicator headset is not emulated, as it needs SDL audio.
- Linux: the cores load again. The bundled libpcap needed RDMA and D-Bus libraries most systems lack; the core's networking is NAT or off, so the build is without libpcap and carries only libslirp.
- README for the core.

## Test build 1 - 2026-10-05

- The first builds of the core from CI: Windows x86_64, Linux x86_64 and aarch64 (glibc, with libslirp next to the core), Android arm64-v8a and x86_64 (Vulkan only, with build fixes from X-OG Mobile).
- The core needs `mcpx_1.0.bin`, `Complex_4627v1.03.bin` and `xbox_hdd.qcow2` in `system/xemu/`, and stops with a message naming the file that is missing.

## February 2026

- libretro support, by danprice142: the original port this fork builds on.
