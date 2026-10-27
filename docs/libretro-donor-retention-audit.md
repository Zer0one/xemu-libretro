# Donor Retention Audit

## Question And References

Did the merge retain behavior from the old WizzardSK Libretro port that the
new WizzardSK port, based on Tovarichtch, had removed or replaced? This audit
is separate from deciding which deliberate fork and MJR adaptations to keep.
A removal in the donor is evidence to investigate, not proof of a defect.

The comparison is pinned to:

| Input | Revision |
|---|---|
| Old WizzardSK port and merge base | `c3cf3a923cdd3fc54d783b137c96ed6fbea0262e` |
| New WizzardSK donor | `252a7accfb5313523928b8b9fd2f44d2f6d9ce06` |
| Commit introducing the Tovarichtch-based port | `f5c841d4d9395ddea5b1a357cd7d6a9d7603e436` |
| Fork before the pending merge | `18a331175c09736e0d6835cf0f95830f95606c6b` |

The result under review includes the staged merge and subsequent uncommitted
MJR adaptations. Neither reference branches nor published history are changed.
The experimental `tovarichtch-fork` branch is a cross-check, not the merge target.

## Findings And Decisions

| Area | Finding | Decision |
|---|---|---|
| Vulkan surface retirement | The merge resolution retained the old immediate `vkDestroyImageView` and cleared the handle before the new donor's deferred-retirement entry could receive it. | Remove those two statements. Keep the compositor wait, retire dependent framebuffers, and pass the live view/image/allocation to the donor's deferred queue. |
| Spotlight and specular lighting | Two isolated old-looking shader statements remain inside the official xemu spotlight implementation from `136fa4fac9`, not inside the old port's angle-reconstruction algorithm. The new donor instead uses a linear cooked ramp. | Keep the official falloff implementation already integrated by a full upstream merge. It consumes the cooked vector without reconstructing its length, while also applying the falloff coefficients. No lighting changes are made by this audit. |
| Old reduced-buffer integration | `xemu_vk_reduced_buffers` and `inline_vertex_batches` were removed in the new donor. | They remain absent from the active option definitions, reader and buffer-sizing code. Keep the donor's growth/rotation model, with the separately reviewed MJR allocation adaptation. |
| Replaced renderer helpers | Old `gl_fence`, `render_display`, `render_display_pvideo_overlay`, `destroy_framebuffers` and `apply_uniform_updates` implementations are no longer defined. | Keep their new donor replacements. No old helper is restored. |
| Chihiro organization | The old top-level `hw/xbox/chihiro.c` and `chihiro-usb.c` were replaced by the donor's board implementation in `hw/xbox/chihiro/`. | Both obsolete paths remain absent; keep the combined Xbox/Chihiro architecture chosen for this integration. |
| New core controls | All eight new donor option keys are present, including CPU boost, smooth first play, Vulkan fast paths and Chihiro controls. | No new donor option was lost during the merge. This registration check is separate from full functional coverage. |
| Snapshot dispatcher | The dispatcher is identical in the old port, experimental Tovarichtch branch and new donor, including the missing main-loop notification. | The separately tested wakeup correction addresses code the new donor itself still retains; it is not a resurrection of something Tovarichtch removed. |

The Vulkan issue is an integration error: the old statement was valid with its
old lifetime model, but no longer implements the new donor's deferred ownership
contract. Waiting for the frontend compositor alone does not settle all renderer
submissions or an open command recording. The fix is platform-independent and
adds no queue/device-idle wait, API requirement or user option.

Other official lighting updates already carried by the fork include specular
parameter handling (`5c516ff05f`), texgen view model (`5c59ae22e4`) and material
ambient/emissive sources (`04f12b57d0`). They are distinct from old-port features
accidentally restored during conflict resolution. This audit does not establish
visual equivalence of every dynamic and unrolled lighting path.

## Method And Verification

Evidence and reproducible checks are in `build/donor-retention-audit-20261027/`:

- Compare the old-to-new donor changes with the actual merged working tree;
  inventory the 83 differing source/build/metadata paths and the 66 original
  conflict regions. Scan deleted blocks, isolated surviving statements, removed
  function definitions and new definitions missing from the result, then review
  the matches and their conflict resolutions.
- Review the statement-level matches semantically and trace their origin.
  Block matching alone did not expose the single premature-destruction call.
  After correction, only the two classified spotlight statements remain.
- Verify that both deleted Chihiro paths and all six removed helper definitions
  stay absent, the retired buffer option is not registered, all eight new
  donor options remain registered, and no newly added donor function definition
  is missing from the reviewed result.
- A recording harness executes the actual `destroy_surface_image` and
  `pgraph_vk_trash_drain` functions with mocked Vulkan/VMA calls. Eight cases
  combine both pending-slot flags with optional scratch images. The old merge
  fails all eight; the correction passes all eight under ASan/UBSan. It checks
  deferred ownership, framebuffer-before-view retirement, cleared source handles,
  exactly-once destruction and repeated draining.
- The macOS ARM64 core rebuilds. RetroArch 1.22.2 runs OutRun 2 under Vulkan
  validation for 132.9 seconds, with visible 3D attract, pause/resume, reset and
  content close/reload in one process. Both loads and joined unloads complete;
  the process exits 0 without force. Validation reports no API errors or sync
  hazards; ten unused shader-output warnings remain. Reset and reload captures
  reach the title screen. The original frontend configuration and source HDD/
  EEPROM fixture hashes are unchanged.
- Update the normal local macOS `xemu_libretro.dylib`, preserving the preceding
  installed core and checking dependency/configuration hashes. Remove the
  private runtime copy after collecting evidence. Linux and Windows were not
  rebuilt or run for this two-statement correction; its recording harness uses
  the common renderer code, without a platform-specific branch.

No other unintended restoration was identified in this comparison. This is a
source/integration audit of the pinned donor delta, not proof that either port
has no remaining bugs. It does not revert deliberate fork features, enable CI,
change firmware/media, commit, push or publish a release.
