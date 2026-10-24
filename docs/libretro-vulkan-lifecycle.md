# Vulkan Lifecycle Validation

## Scope

This activity checks OutRun 2 reset, content close/reload in one RetroArch
process, and frontend exit, after the Vulkan upstream integration and GPU
semaphore handoff. MoltenVK host submission stays synchronous; the separate
acquisition-fence driver experiment is not used.

The local test uses Apple M4, macOS 26.5.1 and RetroArch 1.22.2 `fce35fc7`, with
the existing installed MoltenVK 1.4.2. Every run has its own configuration,
core-info/cache/saves/screenshots, loopback command port and a cloned writable
system/HDD directory. The source media and installed core/framework remain
untouched. Legacy UDP command delivery is corroborated by core log callbacks,
frontend `GET_STATUS` replies and saved images, rather than inferred from a
successful send. The runner owns and reaps only its launched PID.

Private scripts, configurations, samples, PNGs and results are retained under
ignored `build/lifecycle-vulkan/`.

## Reproduced Failures And Corrections

The first attempt inherited RetroArch's native `confirm_reset`, `confirm_close`
and `confirm_quit` settings. `RESET` did not reach the core callback within the
bound. This is a test setup failure, not evidence of a broken guest reset. The
runner disables those confirmations only in its private configuration.

With confirmations disabled, reset reaches `retro_reset`, the guest returns to
the title, and closing content joins the emulation thread. The subsequent
`LOAD_CONTENT` request, however, is rejected by RetroArch before a second core
load:

> Serious problem. RetroArch wants to load libretro cores dynamically, but it is already linked.

RetroArch's guard checks for `retro_init` through a process-wide lookup.
The core's deliberate self-pin used `dlopen` without `RTLD_LOCAL`. On macOS,
the documented default is global visibility, which promotes the previously
local core's entry points and triggers that guard. See Apple's
[`dlopen` documentation](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/dlopen.3.html).

The fix adds `RTLD_LOCAL` to the existing retained-reference/`RTLD_NODELETE`
request. QEMU's surviving RCU/vCPU/NV2A threads and registered exit handlers
remain protected by residency; the exported entry points do not enter the
process-wide symbol search. Windows module pinning is unchanged. The explicit
local flag preserves the default local behavior on Linux, which has not been
runtime tested in this activity.

A small native loader probe reproduces the difference without running the
emulator. Both variants first load a local test library, pin it, close the
initial handle and successfully call its exported function through the retained
handle. The old flags make its symbol visible through `RTLD_DEFAULT`; the fixed
flags keep that lookup empty. Outputs are in `pin-probe/result.json`.

The first successful reload also exposes a diagnostics issue: timing counters
survive between content sessions while the renderer's changed-picture counter
starts again at zero. Unsigned subtraction produces an invalid huge rate in
the first post-reload window. Loading content now requests a new statistics
window; the next `retro_run` resets the counters and scanout baseline before
collecting samples. This changes reporting, not guest timing.

## Validation Boundary

The first full local-pin run completes reset, two loads, two unloads and normal
exit, with 3D images retained in each observation phase. It also reports twenty
`VUID-VkViewport-y-01776` errors around screenshot notifications (ten per Vulkan
instance), with viewport Y around 2^32. All core viewport constructions use
zero Y. The inspected RetroArch widget draw path calculates Y by subtracting
widget position/height from the unsigned display height; an offscreen animated
notification can underflow that calculation. This is separate from the core
pinning correction. The existing ten unused-fragment-output warnings remain.

For the final lifecycle run, only screenshot notifications are disabled in the
private frontend config (`notification_show_screenshot=false`); screenshots
still capture the game. The final run also includes the statistics correction.
Vulkan validation loads through the private loader, with the installed framework
as its ICD and `MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS=1` explicitly set only for
the child process. That avoids relying on deprecated MoltenVK provider discovery
through the validation loader. Normal direct-provider launches retain the core's
per-instance synchronous request.

## Final Result

The `final-validation` run finishes in 197.95 seconds with exit code 0, no
forced termination and its child process reaped. It records two successful
content loads, two emulation-thread joins on unload, the reset callback, and
the returning-machine reset path. Saved images at 55 seconds in each phase
show OutRun 2's attract sequence after initial boot, reset and content reload.
The Khronos validation layer is confirmed active: zero validation errors or
synchronization hazards, with ten existing unused-fragment-output warnings.
The screenshot-widget viewport errors do not recur with screenshot
notifications disabled; this run does not qualify that frontend UI path.

Steady five-second windows report about 60 changed pictures/s in all three
phases (roughly 59.4–60.01 after boot transitions). The first post-reload
window now reports 28.34 pictures/s during guest boot rather than an unsigned
overflow. These are diagnostic picture-update rates, not a controller-driven
gameplay benchmark.

The tested packaged core SHA-256 is
`b7f7314faf57f6e11289f5512132f80fb13cc4724a534007f553bb4704bbc6d9`.
The local source includes the pending official upstream merge through
`25e1ee2fd5` and the uncommitted core integration; the logged repository HEAD
alone does not identify this dirty source state. The retained final source
patch, file hashes and package copy identify the tested candidate.
Results and reproduction scripts are under `build/lifecycle-vulkan/`, with
the final log, phase images, process environment and `analysis.json` in its
`final-validation/` subdirectory. The installed RetroArch core and MoltenVK
framework are not replaced. No commit or push is performed.

## Limits

This is a bounded, no-input OutRun 2 attract-mode lifecycle check on one Apple
Silicon Mac. It does not establish controller gameplay, rumble, audio quality,
other games, long campaigns, OpenGL reload support or Windows/Linux behavior.
The existing OpenGL second-content restriction remains. Earlier failed runs,
including forced cleanup, are retained and are not counted as clean exits.

## Local RetroArch Installation Follow-Up

The user's installed-core attempt failed before graphics initialization:
`MCPX Boot ROM not found`, followed by `Failed to load content`. The core loaded
correctly, but the configured external system directory had no `xemu/` folder.
The earlier isolated lifecycle fixture had not been installed there.

The local setup now has its own copies of MCPX, the verified 4627 debug BIOS,
EEPROM and HDD under
`/Volumes/MacBook External Drive/Retro/_Media/_Multi (RetroArch)/system/xemu/`.
The BIOS copy uses the canonical `Complex_4627v1.03.bin` lookup name; its actual
source is the debug BIOS described in the OpenGL validation report. The HDD
and EEPROM are copies, not links to source media. The new core-named frontend
configuration preserves the old `xemu.opt` choices except `xemu_memory=128`,
which matches this debug BIOS. The original options are retained unchanged.

Runtime `library_name`, info `display_name` and info `corename` now all report
`Microsoft - Xbox (OG)`. Technical filenames and option keys remain `xemu`.
The updated canonical core, matching info and ten support libraries are
installed in the active local RetroArch directories, with backups and hashes.

An installed-core smoke run uses isolated copies of the installed external
system files and the installed binary itself. It completes 4,200 frontend
frames in 74.09 seconds, captures the OutRun 2 attract scene, and exits with
code 0 without forced cleanup. Steady diagnostic windows are about 60 changed
pictures/s. Direct provider discovery confirms synchronous MoltenVK host
submission. This is still boot/attract evidence, not controller gameplay.
An initial sandboxed launch aborted before producing a frontend log; the
successful run had desktop-session access. No new standalone comparison was
needed for the missing-files setup error.

The test also exposed automatic frontend asset extraction replacing installed
info files with bundled metadata. The boot runner now disables that extraction
and uses private info/cache/log paths. The requested xemu metadata was restored
after the run and the info cache invalidated for the next frontend start.
All installed payload hashes were then reverified; global `retroarch.cfg` and
the old `xemu.opt` were unchanged. Evidence is in `build/local-ra-fix/`, including
the original failed-load log, installation receipt and
`installed-smoke-desktop/` results. The tested core SHA-256 is
`5674a40d3f795127baa426200ca77d74458ce04d2423f64b80adcfd60e32af54`.
