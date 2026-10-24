# No-Geometry-Shader Depth Probe

This manual macOS GPU probe compares the active fallback, reciprocal W alone,
reciprocal W plus guest-pixel slope scaling, and upstream's barycentric/plane
reference. For Z draws, the third channel isolates slope scaling. It extracts
shader expressions from current sources and the archived A patch; no patch is
applied to the active renderer. The executable contains no emulator or window.

Build dependencies are the core's existing glslang static library, its checked
out glslang source, a C++17 compiler, Vulkan headers, a loader, a selected ICD,
and Khronos validation. Nothing is downloaded or installed. Supply the local
paths explicitly, for example from the existing validation environment:

```sh
python3 tests/xbox/vulkan_depth/run-macos.py \
  --output build/depth-validation-repeat \
  --vulkan-include /path/to/Vulkan-Headers/include \
  --loader-dir /path/to/vulkan/loader \
  --icd /path/to/selected/icd.json \
  --layer-dir /path/to/validation/layer
```

The output directory must be new. Metal access is required; a sandbox that
hides the GPU can report `VK_ERROR_INCOMPATIBLE_DRIVER`. Run the same command
with GPU access rather than changing the driver or interpreting that error as
an emulation failure. Driver settings and caches are scoped to the probe.

`source-manifest.json` and `test-manifest.json` identify the source, executable
inputs and selected driver. Each `.f32` file contains two consecutive
`width * height * 4` arrays of little-endian FP32 values: raw guest depth and
encoded fragment depth. The four channels are baseline, W-only, scaled
candidate and reference. `gpu.json` identifies each draw configuration;
`analysis.json` contains per-configuration errors and encoded-value mismatch
counts. Error units are guest depth units, before normalization.

There are 48 configurations: four resolution/AA scale factors, D16/D24 guest
encodings, Z/W mode, and three bias settings. They reuse one nondegenerate
positive-W triangle with deliberately different Z and W. The analyzer excludes
the outer 4% barycentric margin, checks finite results, compares the FP32
reference with a double-precision CPU oracle, and requires a reduction greater
than 1,000 times in the errors targeted by W reconstruction and scaled bias.
The absolute error gate is `maxGuestDepth * 1e-5`; passing it does not imply
integer-depth equality. This is an exploratory accuracy gate, not an FPS test.

The upstream geometry equations execute in the fragment shader because the
selected device has no geometry-shader support. Readbacks are RGBA32F color
attachments carrying raw depth and the core's encoding expression; they are
**not** depth-attachment readbacks or depth-equality tests. No window-clip
`discard`, clipped/degenerate primitives, native MSAA sample shading, complete
vertex-program generation, game draw or Xbox hardware reference is exercised.
See [the reassessment report](../../../docs/libretro-vulkan-depth-candidates.md)
for adoption requirements and the measured result.

## Real Depth-Buffer Suite

Add `--depth` to the runner command and use a new output directory. This
selects `prepare-depth.py`, `depth.cpp` and `analyze-depth.py`; the original
48-configuration arithmetic probe remains available without the flag.

The suite creates D16 and the core-selected D24 host depth/stencil format. The
Apple implementation decodes D16 UNORM and D32 float/stencil readbacks; a device
selecting native D24 is refused explicitly rather than silently misdecoded.
The available `depthClamp` feature is enabled and used as in the core. Guest
depth culling, which occurs in the shader, is tested separately.

There are 512 configurations: two scales, two depth formats, Z/W mode, two
bias settings, eight triangle shapes, window discard off/on and guest depth
culling off/on. Each configuration writes five arms independently, then seeds
depth with the reference and replays each arm using EQUAL, LESS and GREATER
with depth writes disabled. A reference self-replay controls the comparison.
The arms are baseline, W-only, scaled after discard, scaled before discard,
and reference. All pixels, including edges, are analyzed.

For each configuration, `.writes` contains five consecutive blocks in arm
order. Each block contains `width * height * 4` FP32 color values (raw guest
depth, encoded depth, coverage marker, unused), followed by the depth aspect:
one uint16 per D16 pixel or one FP32 per D24-host D32 pixel. `.compare` contains
15 byte masks in arm-major, operation-minor order (EQUAL, LESS, GREATER).
`depth-analysis.json` checks stored values against encoded outputs, derives
expected comparison masks independently from the write readbacks, reports
coverage/depth differences and compares the ordinary/far-Z W-only inputs.

API/readback consistency and upstream equality are separate outcomes. The
measured run passes the former and **fails exact upstream equality**. Runner
success therefore does not mean the candidate is ready for adoption. Its
analyzer asserts zero unexplained comparison outcomes, exact write conversion,
matching candidate/reference coverage and empty fully negative/degenerate
draws. It preserves equality mismatch counts instead of hiding them behind an
average-error tolerance.

Native multisample shading, lines, quads, full guest vertex programs, real game
draws and Xbox hardware remain outside this suite. After-discard derivatives
remain nonportable even when a tested driver produces the same values as the
before-discard arm.

## Direct Interpolation: Vulkan And Native OpenGL

`run-interpolation-macos.py` accepts the same path arguments as the main runner
and a new output directory. It builds the Vulkan probe and a native CGL/OpenGL
4.1 probe with the macOS SDK, then executes the same shared fixture on both.
For example, replace `run-macos.py` in the first command with
`run-interpolation-macos.py`. No `--depth` argument is needed. Both processes
need GPU access; no emulator, visible window or installed core is involved.

`interpolation-fixture.h` defines 64 configurations per API: two framebuffer
scales, depth clamp off/on, four guest Z inputs and four host-position routes.
The routes retain original coordinates, multiply all homogeneous coordinates
by `2^-16`, multiply by `1/maxGuestDepth`, or replace host Z with `0.5*clipW`.
The latter is an experimental control only. Guest XY/W and the depth values
used by the reference remain intact. GL uses its required `[-W,+W]` clip Z
conversion while Vulkan uses `[0,W]`; within-API invariance is measured
separately rather than assuming cross-API pixel identity.

The four channels of each `vk-*.f32`/`gl-*.f32` RGBA32F image are built-in
reciprocal W in guest units, an explicit affine reciprocal-W varying, an
explicit perspective W varying, and the upstream barycentric W reference.
The first two reciprocals are inverted in FP32 by the analyzer when reporting
guest W errors. A background pixel has four negative values. Framebuffers are
read in each API's native row order, using matching numeric framebuffer XY
for the reference. No bias, guest discard or texture lookup is present.

`vk.json`/`gl.json` identify the devices and each draw, while
`interpolation-analysis.json` records Z invariance, homogeneous conditioning
and cross-API differences. The analyzer asserts API success, finite positive
covered values, invariant reference W and zero Z-induced variation for all
three neutral-Z interpolant controls. It does not assert exact depth equality.

The measured Z dependence reproduces on both APIs and survives explicit
varyings and homogeneous rescaling. Neutral host Z removes that dependence
in the fixture, but is not a general core fix or driver-conformance verdict.
See the report before using it as a basis for production shader changes.
