# Chihiro Drive-Board Centering

## Current Behavior

The 2026-10-28 WizzardSK merge (`84570ac21d`) replaces our earlier
**Driving Steering Deadzone** with the donor's **Analog Dead Zone (Player 1/2)**.
The developer selected the donor implementation: a radial 0–30% filter,
default 10%, on both sticks, with the remaining travel rescaled. It applies
to both Xbox and Chihiro, including arcade aiming/board controls, not only
racing games. Pedals and digital steering retain their mappings. The old
`xemu_chihiro_steering_deadzone` option is removed and stale values are ignored;
there is only one core filter, after RetroArch's own filtering.

The evidence below records the original 12% fix and is historical; it does not
claim fresh runtime validation of the donor's 10% filter. The updated native
steering test exercises the donor filter together with the JVS mapper.

## Original Reproduction And Fix

OutRun 2 can stop at `Drive board : Centering error` if the steering input
moves off center while the drive board initializes. The original Libretro
mapper passed the gamepad's left X axis directly to JVS; keyboard-only tests
rested exactly at center and did not expose gamepad drift.

The integrated standalone donor's `ui/xemu-input.c`,
`chihiro_axis_travel()` (reference commit `bad4d18933`), filters gamepad axis
travel below 12%. The earlier Libretro adapter provided the same default center
threshold through **Driving Steering Deadzone**
(`xemu_chihiro_steering_deadzone`). Values are 0, 2, 5, 8, 10, 12, 15, 20, 25
and 30 percent; 0 disables the additional filter. It can be changed while
running. Only the racing profiles (Crazy Taxi High Roller, OutRun 2 and
Maximum Tune) use it. Pedals, D-pad ramping, light guns and Ollie King retain
their previous mappings. The Xbox profile does not expose the option; the
optional Full profile exposes it for its arcade path.

## Frontend Interaction

The [RetroArch default](https://github.com/libretro/RetroArch/blob/master/config.def.h#L1771)
is `DEFAULT_ANALOG_DEADZONE 0.0f`. The local configuration checked on
2026-10-27 also has `input_analog_deadzone = "0.000000"` and sensitivity 1.0.
Consequently, the core's default 12% yields the donor's 12% center threshold
with that configuration.

RetroArch normally zeroes the inner zone and rescales the remaining axis
travel. Libretro supplies that processed value, without a standard API to
query the frontend's configured deadzone or the unfiltered input. Following
the developer's choice to keep the implementation inside the core, this
option is explicitly an **additional** filter, not `MAX(frontend, core)`.
For a single axis with ordinary linear frontend scaling, the physical
threshold is `RA + (1 - RA) * core`: RA 20% plus core 12% reaches 29.6%.
Nonlinear curves, radial two-axis input and controller-profile overrides can
change that relationship. No frontend settings are overwritten by the core.

The core snaps input within its threshold to exact JVS center `0x8000`,
retaining the analog values outside it instead of rescaling them again.

## Focused Validation

macOS ARM64, RetroArch 1.22.2, Vulkan/MoltenVK synchronous submission,
GDX-0004A CHD, the same read-only source media and separate copies of the
user's arcade save/EEPROM. A diagnostic Libretro proxy fixes only port 1's
left X axis at 2000/32767 (about 6.1%); other frontend callbacks and inputs
pass through normally. No game executable or drive-board protocol is patched.

| Core / Option | Result With The Same 2000-Unit Input |
| --- | --- |
| Previous installed core | `Drive board : Centering error` |
| Corrected core, default 12% | Intro and advancing attract scene, no centering error |
| Corrected core, explicitly 0% | The same centering error returns |

All three runs close normally with exit 0 and join the emulation thread;
protected source media/support/configuration hashes remain unchanged.
The 12% run reaches attract with the user's copied save, so no NVRAM reset
is needed. This controlled reproduction identifies the missing drift filter;
it is not a physical-controller travel/rumble certification or a new full
playthrough. The physical controller was not enumerated by the standalone
SDL read probe in this session.

The original `scripts/test-libretro-chihiro-steering.py` checked 1,310,720 actual mapper
results across all 10 settings, both orientations and every signed 16-bit
axis value, plus digital steering, boundary and endpoint behavior, under
ASan/UBSan. Profile registration checks cover APIs 0/1/2 and translated and
English-only tables: Xbox 18 options, Chihiro 19, Full 23. Both normal local
core artifacts are built; no remote CI, commit or release is performed.

The compact evidence is kept under `build/drive-centering-20261027/`;
regenerable test fixtures and the diagnostic proxy are removed after testing.
