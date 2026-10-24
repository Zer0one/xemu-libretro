# Vulkan Depth Candidates: Reassessment

Review date: 2026-10-24. Official upstream reference:
`25e1ee2fd59a451f2ab57e825b4f8a5414220c8b` (`0.8.138-3`).

## Decision And Scope

The two depth experiments remain plausible independent improvements. Their
failure to remove OutRun 2's horizon band does **not** establish that they are
incorrect. That defect was subsequently traced to packed-depth texture border
sampling; see [the confirmed correction](libretro-vulkan.md#confirmed-packed-depth-border-correction).

The original full A/B experiments are preserved below. A restricted W-only
implementation was added on 2026-10-25 after the focused checks described in
the next section. The Z varying and Z-buffer slope changes remain outside the
active patch. Exact hardware or upstream bit equality is not claimed.

| Candidate | Static Assessment | Remaining Decision |
| --- | --- | --- |
| A: reciprocal W and guest-pixel slope bias | Independent improvements confirmed; neutral host Z removes the measured Z-induced W instability | Restricted W-only subset implemented; original full A and its Z-buffer bias change remain archived |
| B: unnormalized Z varying | Plausible precision improvement; ideal affine interpolation is equivalent, but hardware rounding can differ | Second priority; demonstrate a measured depth-accuracy benefit before adopting the extra shader interface |

These are capability-based renderer proposals for devices without geometry
shaders, including but not limited to macOS. They do not change host queue
submission policy, presentation, OpenGL, or the native geometry-shader path.

## Restricted W-Only Implementation: 2026-10-25

The implementation changes only Vulkan W-buffer draws on devices without
geometry shaders. A vertex-module key flag selects neutral host Z after the
guest outputs have been assigned. The fragment shader reconstructs guest W
from `gl_FragCoord.w`, retains the upstream positivity/NaN handling and existing
guest depth culling/clamping/encoding, and applies W slope bias in guest pixels.
Derivatives run before window discard and only when the slope factor is nonzero.
No core option, Z varying, Z-buffer bias adjustment or presentation change is
introduced. The scoped [implementation patch](patches/vulkan-depth-candidates/03-w-only-neutral-host-z.patch)
is preserved separately from the earlier full A/B proposals.

Actual baseline and candidate core generators produced 24 shader variants:
fixed-function/programmed vertices, GL/Vulkan generation, D16/D24 fragments,
depth culling/clamping and native/fallback depth modes. All 18 unaffected
variants are byte-identical; only four no-GS W fragments and two Vulkan W
vertices change. All 20 Vulkan modules compile to SPIR-V. The historical
offscreen probe can still reconstruct its original GLSL byte-for-byte by
reversing this scoped PSH patch in a temporary source copy.

The macOS ARM64 core builds and loads the same private OutRun 2 race snapshot
through RetroArch. Baseline and candidate each advance for 30 seconds, capture
the scene, unload content and exit normally. Visual review finds no new gross
horizon/building defect in those captures; particle/spark phases and elapsed
guest frames differ, so this is not a frame-aligned pixel comparison. Excluding
the first two five-second windows, changed-picture medians are 59.9/s for the
baseline (59.6–60.0) and 59.105/s for the candidate (58.61–60.0). No performance
improvement is claimed from this short comparison.

The candidate's separate Khronos/synchronization-validation snapshot run exits
normally with zero API errors/hazards and ten unused-shader-output warnings.
No full gameplay campaign, primitive coverage campaign or cross-platform runtime
qualification is added. The earlier numerical and exact-equality limits still
apply; this scoped change does not solve every no-GS interpolation difference.

The installed canonical RetroArch core and matching `.info` were updated, with
strict signature/hash checks and recoverable backups. Global configuration,
user options and the external system directory are hash-verified unchanged.
The tested pre-publication core SHA-256 is
`c2392a3257056cea2de514fd87d038c904807cb42fa58f195ebed592b6ce2fd2`.
Detailed evidence is in `build/w-buffer-neutral/final-verification.json` and
`build/w-buffer-neutral/deployment/receipt.json`; a compact
[implementation receipt](patches/vulkan-depth-candidates/w-only-implementation.json)
records the source/build/test hashes. Those probes predate the source commits;
the merge and subsequent Libretro adaptations are recorded separately in this
fork's history. A metadata-only publication rebuild uses the committed source
identifier; its local installation receipt is retained under
`build/publication/deployment/receipt.json`.

The following sections retain the earlier investigation and its historical
build fingerprints; statements about an unchanged installed core describe
those earlier stages.

## Candidate A: Reciprocal W And Slope Units

Relevant sources are `glsl/vsh-prog.c`, `glsl/vsh-ff.c`, `glsl/vsh.c`,
`glsl/geom.c` (`calc_triz`) and `glsl/psh.c`, under
`hw/xbox/nv2a/pgraph/`. Both vertex paths preserve guest W in clip-space W.
The Vulkan fragment coordinate supplies interpolated reciprocal W, as specified
by [Vulkan's FragCoord interface](https://docs.vulkan.org/spec/latest/chapters/interfaces.html#interfaces-builtin-variables-fragcoord).

For screen-space barycentric weights `lambda_i`, with sum 1 and positive W:

```text
q = sum(lambda_i / W_i)
upstream perspective weights: beta_i = (lambda_i / W_i) / q
upstream interpolated W: sum(beta_i * W_i) = 1 / q
candidate W: 1 / gl_FragCoord.w = 1 / q
```

Thus reciprocal reconstruction matches the existing barycentric W expression
in ideal arithmetic for a nondegenerate triangle. By contrast,
`gl_FragCoord.z * clipRange.y` reconstructs guest Z; it cannot generally
represent W when guest Z and W differ. This is a concrete reason to retain A
as a correctness candidate independently of the shadow-band investigation.

The geometry shader's perspective `calc_triz` computes the maximum absolute
gradient of `1/W` in guest screen coordinates. The fragment shader then applies
`depthFactor * triMZ * W * W`, followed by the existing depth handling. A uses
derivatives of `gl_FragCoord.w` to approximate that gradient and preserves the
upstream offset, positivity and NaN handling.

There is a second, separable change in A. Host derivatives are measured per
framebuffer pixel, whereas upstream's geometry calculation uses guest pixel
coordinates. With `hostXY = guestXY * surfaceScale`, the chain rule gives:

```text
dq/dguestX = dFdx(q) * surfaceScale.x
dq/dguestY = dFdy(q) * surfaceScale.y
```

The same conversion applies to Z gradients. `surfaceScale` already includes
both antialiasing factors and internal resolution scaling. The current
fallback omits this conversion. The dimensional argument supports a bias fix,
but actual rasterizer derivatives and sample locations still need checking.

The preserved ideal CPU check covers 4,000 positive-W right-triangle samples
at scales `(1,1)`, `(2,2)`, `(4,4)` and `(4,2)`, with W between `1e-2` and
`1e6`. Re-running it during this review gives a maximum double-precision
relative error of `1.4130785430666037e-10`. Its slope check divides and
multiplies analytical gradients; it does **not** measure GPU derivatives.
It proves neither FP32 equivalence nor correct final integer depth values.

### GPU Follow-Up: Isolated W And Bias Measurements

The manual [offscreen Vulkan probe](../tests/xbox/vulkan_depth/README.md) now
extracts the active fallback and archived A expressions, renders them on Apple
M4 through the selected RetroArch MoltenVK provider, and reads back the fragment
results. The reference uses the upstream Kahan determinants, barycentric
interpolation and `calc_triz` equations, executed in the fragment stage because
this device has no geometry shaders. A double-precision CPU oracle separately
checks that reference. This is a same-source equation comparison, not execution
of the complete upstream GS pipeline or a hardware-accuracy claim.

The test isolates the active baseline, W reconstruction without slope scaling,
and W/Z reconstruction with slope scaling. Its 48 configurations combine four
scales, D16/D24 guest encodings, Z/W modes and three bias settings. They reuse
one nondegenerate triangle with positive, varying W and deliberately different
Z values. The analyzer checks 89,712 interior fragment/configuration samples,
excluding the outer 4% barycentric margin. All bounded improvement gates pass;
Khronos API/synchronization validation reports zero errors. A repeat through
the reusable runner produces identical analysis results.

Representative D24 results at asymmetric scale `(4,2)` follow. Errors are mean
absolute differences from the upstream equations in raw guest depth units,
before normalization. Each row contains 2,061 interior samples.

| Draw | Active Baseline | W Reconstruction Only | With Guest-Pixel Slope Scaling |
| --- | ---: | ---: | ---: |
| W, no bias | 3,261,183.948 | 0.233 | 0.233 |
| W, slope factor 1 and offset 0.75 | 3,057,709.881 | 179,608.626 | 0.397 |
| Z, slope factor 1 and offset 0.75 | 281,327.985 | Unchanged | 0.763 |

These measurements establish two independent improvements in this bounded
fixture: the existing fallback uses the wrong quantity for W, and its slope
bias has the wrong units when rendering at a larger scale. The Z varying was
not enabled or tested here.

The corrected approximation is still not bit-exact. Across all configurations,
maximum raw differences from the upstream equations are `0.01953125` for W/D16,
`4.0` for W/D24, `0.0234375` for Z/D16 and `6.0` for Z/D24. After the core's
encoding expression, the candidate differs from the reference on 35/22,428
W/D16 samples and 6,221/22,428 W/D24 samples; Z differences are 70/22,428 and
10,800/22,428 respectively. These counts include fractional values near integer
boundaries and must not be presented as depth-equality qualification. The CPU
oracle also detects rounding differences in the FP32 reference itself, so a
different encoded value does not alone identify which path is more accurate
to Xbox hardware.

Readback targets are RGBA32F color attachments carrying raw depth and the core's
encoded fragment-depth expression. Actual depth attachments, comparison
operations, discarded helper lanes, clipped primitives, complete generated
vertex shaders and game draws remain outside this test. Scaling is simulated
by a larger framebuffer; this does not validate native multisample sample
shading. Those limits prevent treating this result as complete adoption
qualification.

An additional upstream limitation matters: `pgraph_glsl_set_psh_uniform_values`
already marks nonzero W-buffer slope bias as partially implemented, because
Xbox appears to use the slope at the first visible pixel rather than the
per-fragment value. A deliberately follows the existing upstream expression;
it does not resolve that hardware-model TODO.

The [compact GPU evidence](patches/vulkan-depth-candidates/gpu-review.json)
records the selected provider hash, source/test hashes, numerical summaries and
raw-evidence locations. Reproducible shaders, readbacks and validation logs are
under `build/depth-validation/reproducible/`. Active shader sources and the
canonical installed core are hash-verified unchanged after this investigation.

### Real Depth Buffers, Clipping And Compare Operations

The follow-up `--depth` suite writes actual depth attachments and replays the
draws with `EQUAL`, `LESS` and `GREATER` against a reference depth draw. Apple
M4 selects `VK_FORMAT_D16_UNORM` and `VK_FORMAT_D32_SFLOAT_S8_UINT` for guest
D16 and D24 respectively, matching the core's format selection. It enables
the supported `depthClamp` feature and pipeline state just as the shared-device
core does. Guest shader depth culling is tested both enabled and disabled.
Host clamping and guest culling are different operations; see the
[Vulkan depth-clamp definition](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineRasterizationStateCreateInfo.html).

There are 512 configurations: scales `(1,1)`/`(4,2)`, two guest depth formats,
Z/W mode, bias off/on, eight triangle shapes, window discard off/on, and guest
depth culling off/on. Shapes cover ordinary and viewport-crossing triangles,
near/far-Z inputs, one negative W, tiny positive W, fully negative W, and a
degenerate triangle. Five expression arms isolate baseline, W alone, scaled
candidate after discard, scaled candidate before discard, and upstream
equations. The reference equations still execute in the fragment stage;
this is not execution of upstream's full GS pipeline.

The final run reports zero API/synchronization errors, zero nonfinite raw
outputs, and zero unexplained comparison-mask results. All stored depth values
match their encoded fragment output after the destination format conversion.
Fully negative-W and degenerate cases produce no covered fragments. The scaled
candidate's coverage matches the reference across all configurations, including
guest depth culling; the baseline has 7,254 coverage differences. Computing the
candidate before versus after window discard changes no stored depth in this
run, but this single-driver observation does not make derivatives after
nonuniform termination portable.

**Exact depth equality does not pass.** Of 509,922 reference-covered
sample/configuration pairs, the scaled candidate differs on 232,900 and passes
`EQUAL` on 277,022. The reference's self-replay passes on all 509,922. There is
no hidden API failure: `EQUAL`/`LESS`/`GREATER` outcomes agree exactly with the
independently read back stored depths. This establishes that the remaining
numerical differences can change a real depth-test decision; it does not
establish a particular game's visible impact.

The following maxima include edge pixels, unlike the earlier interior-only
arithmetic experiment. D24 units are normalized stored-depth differences
multiplied by `2^24`; D16 uses stored integer steps.

| Input Shape | Maximum Candidate W/D24 Difference | Maximum Candidate Z/D24 Difference |
| --- | ---: | ---: |
| Ordinary | 6 | 6 |
| Viewport Crossing | 463 | 61 |
| Near-Z Input | 6 | 5 |
| Far-Z Input | 2,997 | 1,629 |
| One Negative W | 702 | 182 |
| Tiny Positive W | 0 | 6 |

The largest W/D16 difference is 11 stored steps. These values supersede any
interpretation of the earlier four-/six-unit interior maxima as general bounds.
The candidate remains much closer to upstream than the baseline in the tested
W draws, but the improvement is not exact emulation of the upstream equations.

A smaller counterexample isolates an unexpected Z dependency: keep triangle
XY and W unchanged, disable bias and guest culling, and change only vertex 1's
Z from `0.73 * maxDepth` to `1.5 * maxDepth`. With host depth clamping enabled,
the reference W result is unchanged. On D24 at `(1,1)`, the reciprocal-W
candidate changes at all 341 covered pixels, with maximum raw guest-depth
difference `2,150.5`. The same test at `(4,2)` changes 2,647 pixels with maximum
`353.0`. This identifies the host rasterization/interpolation route as the
next investigation target; it does not prove a MoltenVK specification violation
or establish that the driver's clipping precision is the sole cause.

The cross-API follow-up below instruments reciprocal-W directly and compares
native OpenGL. Moving derivatives before discard is justified by the language
rules, but it did not resolve this measured counterexample. Candidate B's Z
varying remains separate and has not been validated by this W investigation.

The [depth/clipping evidence](patches/vulkan-depth-candidates/depth-clip-review.json)
contains format/feature selection, totals, counterexample measurements, source
hashes and artifact hashes. Full writes and compare masks are retained under
`build/depth-validation/depth-clip-core-policy-final/`. The core implementation
and installed RetroArch binary remain unchanged; no candidate is promoted on
the strength of a synthetic average-error improvement alone.

### Cross-API Interpolation And Neutral-Z Control

A separate offscreen fixture now reads the interpolants themselves on Vulkan
and native OpenGL, both identifying the device as Apple M4. OpenGL reports
`4.1 Metal - 90.5`; its process has no Vulkan/MoltenVK environment overrides.
The final experiment contains 64 configurations per API: two scales, host
depth clamp off/on, four Z inputs, and four host-position routes. There is no
guest bias or discard. RGBA32F readbacks carry the built-in reciprocal W
converted back to guest units, an explicit `noperspective` reciprocal-W
varying, a perspective W varying, and the upstream barycentric W reference.

With host depth clamp enabled, changing only the far vertex Z perturbs the
actual `gl_FragCoord.w` input on both APIs. At `(1,1)`, all 341 covered pixels
change. The maximum induced reconstructed-W changes are:

| Host Position Route | Vulkan | Native OpenGL |
| --- | ---: | ---: |
| Original Guest Z | 2,150.5 | 2,153.0 |
| Homogeneous Scale `2^-16` | 2,150.5 | 2,153.0 |
| Homogeneous Scale `1 / maxGuestDepth` | 2,149.0 | 2,151.5 |
| Neutral Host Z (`0.5 * clipW`) | 0 | 0 |

The explicit affine and perspective varyings change by similar amounts in
the original-Z route. Therefore this counterexample is not confined to the
built-in variable or fragment reciprocal arithmetic. Normalizing the magnitude
of the homogeneous coordinates also does not remove it. The reference W
remains unchanged in all matched Z-variation comparisons.

The neutral-Z route keeps guest XY/W and the original guest depth data intact,
but sets only the host vertex position Z to `0.5 * clipW` before the API's clip
convention conversion. It eliminates the observed Z dependence on both APIs,
with or without host depth clamp. With clamp enabled, the maximum built-in-W
error against the reference is then 2 raw guest units over this fixture,
compared with 2,149.5 for Vulkan and 2,152.0 for OpenGL in the original route.
Residual rounding remains; zero Z-induced change does not imply exact upstream
depth equality. All 128 final draws complete with zero Vulkan validation errors
and zero OpenGL errors.

This is evidence that the guest-Z-fed rasterization/interpolation route causes
the measured dependence, and that the effect is **not specific to MoltenVK**
on this hardware. It is not a proof of the driver's internal clipping algorithm
or a specification violation. OpenGL separates primitive clipping and output
interpolation, including generated vertices; see sections 13.7 and 14.6 of
the [OpenGL specification](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf).
Neither API path is an independent hardware platform reference.

The neutral-Z result is a test-only control, not a core change. A production
proposal must preserve guest depth culling and clamping, use a separate guest
Z representation when needed, and cover viewport clipping, negative W, lines,
points, quads, complete generated shaders and real guest draws. The active
fallback still needs `gl_FragCoord.z` for Z draws; replacing host Z there
without carrying guest Z separately would break that reconstruction. The
result does not qualify candidate B, alter native GS/OpenGL rendering, or
justify a macOS/game-specific workaround.

The reusable `run-interpolation-macos.py` runner and
[interpolation evidence](patches/vulkan-depth-candidates/interpolation-review.json)
record the shaders, driver/source hashes, direct-input measurements and limits.
Raw final captures are under
`build/depth-validation/interpolation-neutral-control/`. The preceding
48-configuration-per-API run under `interpolation-cross-api-v2/` supplies the
initial cross-API reproduction; the final run adds the neutral-Z control.
The installed canonical core remains unchanged.

## Candidate B: Unnormalized Z Varying

B exports `vtxPos.z` from the vertex shader as
`layout(location = 14) noperspective out float vtxDepth`, consumes the matching
fragment input, and uses that value and its derivatives for the Z branch.
The vertex-generation option is selected with the same no-geometry-shader
capability condition as the fragment fallback.

The intent is to avoid normalizing vertex Z by `clipRange.y` and multiplying
interpolated hardware depth back by that range. In ideal arithmetic both
routes interpolate the same screen-space Z plane. Avoiding that round trip
could reduce precision loss, especially around D24 quantization boundaries;
it can also change interpolation rounding. A different result alone does not
establish that the varying is closer to upstream or Xbox hardware.

The preserved experiment was **A plus B**, not B alone. Its unchanged shadow
band therefore gives no isolated verdict on Z precision, W reconstruction or
the slope-unit change. Future comparisons must vary these independently.

Static interface review finds location 14 unused by the current common vertex
header, whose explicit locations are 0 through 12. The added vertex option is
part of `ShaderModuleCacheKey`, which is zero-initialized and hashed as a whole.
These observations support the proposal's wiring; shader compilation,
device interface limits and GPU depth accuracy remain unqualified by this
documentation review.

## Preserved Proposals And Historical Evidence

The review artifacts are stored beside this report rather than relying only
on ignored build outputs:

- [A patch](patches/vulkan-depth-candidates/01-reciprocal-w-and-bias.patch):
  exact diff of preserved `psh-before.c` and `psh-candidate.c`.
- [B incremental patch](patches/vulkan-depth-candidates/02-unnormalized-z-after-01.patch):
  apply after A. The fragment delta is exact; supporting vertex, header and
  cache-key edits were reconstructed from preserved pre-experiment sources
  and the recorded edit contract. This is a replay proposal, **not** a
  byte-exact archive of the complete historical B source tree.
- [Ideal arithmetic check](patches/vulkan-depth-candidates/check-ideal-w-math.py)
  and [review evidence](patches/vulkan-depth-candidates/review-evidence.json):
  historical binary hashes, bounded process results, source hashes and limits.

Both patches pass sequential `git apply --check` and application against
temporary copies of the current border-corrected sources. No patch was
applied to the active renderer. This establishes applicability, not build or
shader validity of the reconstructed proposal.

Historical RetroArch Vulkan runs loaded the copied race snapshot, captured the
scene, closed content and exited normally: A took 10.172 seconds, A+B took
10.227 seconds, both exit code 0. Neither removed the horizon band. Raw logs,
captures and result files remain under `build/shadows-or2/fix/` in
`w-depth-candidate/` and `z-varying-candidate/`. Those runs do not establish
that the relevant W-buffer or precision-sensitive draw was exercised.

## Required Validation Before Adoption

1. Use the current packed-depth border correction in every comparison arm.
   Isolate W reconstruction, slope scaling and the Z varying. Trace the actual
   depth mode and bias registers of each measured draw.
2. Compare GPU depth readbacks with the same-revision upstream geometry-shader
   equations/path, covering fixed-function and programmable vertices, D16/D24,
   depth equality tests and values near integer rounding boundaries. Agreement
   with upstream is a port-fidelity result; hardware accuracy needs its own
   reference. Floating-point depth formats already have a separate upstream
   TODO and are not qualified by these experiments.
3. Exercise zero and nonzero constant/slope bias across internal resolution
   and antialiasing scales, including asymmetric scales. Check boundaries,
   clipped triangles, negative/near-zero W, degenerates, lines, points and
   converted quads, rather than relying on the positive-W triangle identity.
4. Audit derivative placement and helper invocations. The existing fallback
   and both archived candidates compute derivatives after window-clip
   `discard`. This inherited placement requires review at clip boundaries;
   [GLSL's derivative rules](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.10.pdf)
   leave derivatives in nonuniform control flow undefined. Do not assume that
   `precise` makes rasterizer interpolation or derivatives bit-exact.
5. Compile and validate the complete shader variants, interface limits and
   cache behavior. Check no-GS devices beyond macOS and regress the native GS
   path. Measure matched-scene throughput and normal unload after accuracy
   has been established.

The initial reassessment and synthetic GPU stages left the installed core at
`991de76a815edceaabb695ca001507c4a1e8abd6056cb1c3227ead527abd697c`.
The later restricted implementation and canonical deployment are recorded
above. The source history records the later merge/adaptation commits; no release
is created by these validation activities.
