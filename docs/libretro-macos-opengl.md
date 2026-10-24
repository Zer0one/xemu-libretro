# macOS OpenGL Validation

## Symptom And Cause

On Apple M4 with macOS 26.5.1, the original `ff483bdb95486b00ad9a7e38a27212cd61b42e13`
core aborted during OutRun 2 boot in `render_display`, at the framebuffer
completeness assertion in `hw/xbox/nv2a/pgraph/gl/display.c:339`. This reproduced
in RetroArch 1.21.0 (`05f94af4`) and 1.22.2 (`9be7ec92`).

RetroArch Cocoa creates a new CGL share group when video is reinitialized. With
`cache_context = true`, its cached-context path can skip `context_destroy` while
still calling `context_reset` on the new group. The port replaced NV2A worker
contexts but retained renderer objects created in the previous contexts. FBOs
and VAOs are context-local. Merely keeping the old worker contexts removed the
assertion but left white output after the frontend changed share groups.

Temporary CGL diagnostics recorded different group addresses before and after
the video reinitialization. Those diagnostics were removed from the final patch.
Removing a redundant geometry update alone did not fix the failure; geometry
reporting remains unchanged.

## Correction

OpenGL requests the normal context destruction/reset lifecycle instead of the
cached-context path. Before frontend context destruction, the core switches
to xemu's null renderer using its existing bounded renderer-switch helper.
Upstream's renderer-switch machinery flushes guest surfaces and finalizes GL
resources on the emulation renderer thread. The subsequent reset releases the
old worker contexts, creates new contexts sharing with the frontend, and switches
back to OpenGL. Vulkan's existing context policy is unchanged.

The emulation renderer and framebuffer-format code are unchanged. No assertion
was removed and no CPU readback fallback was added.

## Standalone Reference

The comparison source was exported with `git archive 478b4f4961` into the ignored
`build/standalone-source/` directory. This is the same upstream commit named in
`upstream.version` (`0.8.136-53`), not current upstream master. The standalone
was compiled as ARM64 using the project's MacPorts libraries and upstream build
recipe. Only archive version metadata files were added; emulator sources were
not patched. Downloaded third-party sources already present locally were reused.

Both standalone and core used the same MCPX 1.0 boot ROM, debug 4627 BIOS,
128 MB RAM, copied EEPROM, retail 4034 disk image, and external OutRun 2 XISO.
Each emulator had separate writable copies of disk and EEPROM. The test BIOS
copy is named `Complex_4627.bin` for the core's existing autodetection; the actual
source is `xbox-4627_debug.bin`, as recorded in the local input manifest.

The user visually confirmed that the standalone rendered OutRun 2. Its log
reports commit `478b4f4961` and Apple M4 OpenGL 4.1. CUA could not inspect the
standalone window reliably, and this xemu build does not implement QMP
`screendump`; no standalone screenshot is claimed.

After the comparison, the temporary standalone source, build, app bundle and
standalone-only helper scripts were removed from this checkout. Future standalone
builds and tests belong to the dedicated standalone project at
`/Users/andrea/dev/xemu`. Historical paths in
the retained logs describe the original test and no longer identify local builds.

## Frontend Evidence (2026-10-22)

The successful short core run used RetroArch 1.22.2, OpenGL `glcore`, 128 MB RAM,
native internal resolution, and disabled networking. It completed 1,800 frontend
frames in 33.25 seconds, exited with status 0, and saved the OutRun 2 title screen
with the Italian Start prompt. The final essential patch, with geometry reporting
restored unchanged, completed 6,000 frames in 102.83 seconds and exited with
status 0. Its final screenshot shows the title, and live observation also showed
the attract animation after frontend video reinitialization. The log records
renderer parking and restart. This is boot and title-screen/attract evidence,
not an interactive gameplay or physical-controller qualification.

RetroArch 1.21.0 also completed 2,400 frames in 43.38 seconds with exit status 0
using the final patch and the reusable runner. Its saved screenshot was inspected
as a separate regression check. No test emulator processes remained afterward.

The local evidence directories are ignored build artifacts:

- `build/runtime-outrun2/`: original RetroArch 1.21.0 failure.
- `build/runtime-outrun2-nightly/`: original RetroArch 1.22.2 failure.
- `build/runtime-outrun2-cachefalse/`: successful short probe and `final.png`.
- `build/runtime-outrun2-final/`: final patch extended probe.
- `build/runtime-outrun2-final-stable/`: RetroArch 1.21.0 regression probe using
  the reusable runner.
- `build/evidence/opengl/standalone-comparison/`: archived standalone comparison
  logs and process results; no standalone executable, sources or writable images.
- `build/evidence/opengl/`: build recipes, logs and temporary diagnostic records.

## Repeat The Probe

Build and bundle an ARM64 core using the macOS recipe in
`.github/workflows/libretro.yml`. For development, use
`xemu_dev_libretro.dylib` with matching `.info` and the adjacent
`xemu_libretro_libs/` directory. The tested local package is in `build/dist/`;
it does not replace an installed updater-managed core.

Prepare a private system directory containing `xemu/` and writable copies of
the BIOS, EEPROM and disk files. The runner uses the supplied system directory
directly, so do not point it at originals that must remain untouched.

```
python3 scripts/test-retroarch-boot.py \
  --retroarch '/path/to/RetroArch.app/Contents/MacOS/RetroArch' \
  --core '/path/to/xemu_dev_libretro.dylib' \
  --system '/path/to/private/system' \
  --rom '/path/to/OutRun 2.xiso.iso' \
  --output 'build/boot-probe' --memory 128 --max-frames 6000 --timeout 180
```

Use `--memory 64` for a retail BIOS unless a documented requirement says
otherwise. The runner creates isolated frontend configuration, core options,
saves, states, histories and logs. It records the exact command, core SHA-256,
PID, exit status and screenshot presence, and terminates/reaps only its own
process on timeout. Inspect `final.png` before claiming boot or gameplay.

These tests cover macOS ARM64 only. Current MacPorts ARM64 libraries target
macOS 14 even though the inherited core build recipe requests 13.7.4; this
package is not verification of macOS 13 compatibility. No Intel, Windows or
Linux runtime result is implied by the macOS tests.
