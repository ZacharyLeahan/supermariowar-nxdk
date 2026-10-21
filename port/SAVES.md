# Xbox settings/save work

Ordinary builds still default to persistence disabled. `PREFERENCES=1` now
connects the validated two-slot store to explicit preference schemas. Schema 1
contains audio levels/toggles, CPU difficulty and several gameplay display
toggles. Schema 2 adds the selected powerup preset, map/game override choice,
and all 17 × 26 drop weights. Schema 3 adds 44 global menu settings.
Schema 4 adds mode-specific settings; packs and controller choices
remain unfinished. The volume fields have passed an in-game reboot round trip, as recorded below;
this is not a completed release save system.

`SaveRecord.h` is an I/O-free foundation for a two-slot store. Each bounded
record has an eight-byte magic, little-endian schema/generation/length, CRC32
covering header fields and payload, and at most 16 KiB of payload. Decode
validates the entire record before replacing the caller's value. Selecting the
newest valid generation handles rollover and a missing/corrupt alternate slot.
CRC protects against accidental corruption, not malicious editing.

Run `python3 ports/nxdk/tests/run-save-record.py` from the game checkout. Host
address/undefined-behavior sanitizer tests cover the standard CRC vector, exact
round trips, every truncated prefix and every single-bit change to a sample
record, extra bytes, forged lengths, unknown schemas, the payload bound,
generation rollover and fallback from a torn newer record. The same test source
also cross-compiles with nxdk C++20. Real Xbox file-write evidence is recorded below.

## Integration requirements

1. Alternate two short FATX-safe filenames in a title-local writable folder.
   Write the older slot while retaining the current valid one; never delete
   the only valid settings copy before committing its replacement.
2. Check write/flush/close results. nxdk has `NtFlushBuffersFile`; ordinary
   `fflush` only establishes C-library flushing. Do not promise power-loss
   durability without checking the platform path and testing interrupted writes.
3. Encode explicit fields rather than pointers or native object layout. Validate
   enum/range values and asset indices before applying a complete snapshot.
   Validate each slot's payload schema before selection, so an unsupported
   newer config can fall back to an older supported one.
   Keep control-device detection and framebuffer setup safe on changed hardware.
4. Treat missing, unsupported, truncated or corrupt data as recoverable. Preserve
   defaults or the older valid slot and log a diagnostic rather than exiting.
5. Test round trips, alternating generations, corrupt/truncated copies, unwritable
   folders and reboot recovery on Xbox before enabling release persistence.

`SaveStore.h` implements alternating-slot selection behind an injected I/O
interface. It writes the older slot and validates a complete readback before
reporting success. Tests simulate every interrupted-write prefix, fallback
from a corrupt newest slot and generation rollover. The adapter and initial game-config integration are described below; full
release persistence remains disabled while the broader settings schema and
game-level failure recovery are unfinished.

`SaveFileSlots.h` now supplies a bounded Xbox file adapter, with explicit paths,
write-through handles, `NtFlushBuffersFile`, checked close and complete readback.
It is compiled into the separate `probes/save-store` test and opt-in game
preference integration. The probe tests FATX writes/recovery and a second-launch reboot round trip before
game settings integration. See its README for the exact scope.

## Real Xbox file-store evidence

`test-results/save-store-xbox.*` records two probe runs on the 128 MB Stellar
Xbox. Every file-store check passed, including explicit flush, readback, recovery
from a deliberately truncated newer slot, repair, and an unwritable-path error.
The second launch loaded the expected generation-3 final record after reboot.
This verifies the isolated file layer, not sudden-power-loss durability or game
configuration integration.

To build the opt-in initial preference integration, run the workspace
`scripts/build-game.sh PREFERENCES=1 AUDIO_FAILURE_DMA_LOG=1`. The port configuration stamp rebuilds game objects when these flags change. It writes only `smw-pref0.bin` and
`smw-pref1.bin` beside the executable, separate from desktop `options.bin`.
Both record CRC/schema and preference semantics are checked before choosing a
slot. Host tests prove fallback from a newer CRC-valid but unsupported payload.

With preferences enabled, returning from the Options menu to the main menu
commits the snapshot, as do the existing start-match and normal-exit paths.
Back+Start recovery remains a direct reboot and does not start new disk I/O.

## Initial in-game round trip

`test-results/preferences-ui.*` records the first preference-enabled XBE saving
both volumes at 64, loading those values after reboot, and restoring both to
128. The UI sliders matched the record values. That run also exposed a separate
shutdown assertion from destroying an unused null texture; the source fix is
committed and the subsequent guarded clean-exit regression is recorded below. Do not label that entire
run error-free or treat it as acceptance of every preference field.

The subsequent guarded regression (`test-results/exit-match-regression.*`)
loaded generation 3 at full volume, completed a one-human/three-CPU timed match,
committed preferences on match start and Exit, and returned to XBMC with no
error output and no watchdog marker. The null-texture cleanup assertion did not
recur. This covers the guarded test XBE; the final unguarded release build still
needs its own regression.

## Powerup preset schema 2

The payload uses 455 explicit little-endian 32-bit values (1832 bytes including
its schema/game-version/count header), never a native object dump. It validates
preset indices 0–16, override choices 0–3 and every weight 0–10, matching
`MI_PowerupSelection.cpp`. Array dimensions are compile-time checked. Schema 1
saves remain readable and preserve defaults for newly added powerup fields.
New saves use schema 2; older binaries that only understand schema 1 can fall
back to a surviving schema-1 slot but cannot read schema 2. Back up saves before
downgrading.

Host sanitizer tests verify every encoded weight and scalar position, reject
every truncated payload and every out-of-range field, test migration, and keep
runtime pointers unchanged. `test-results/powerup-save-v2.*` records native
legacy generation-7 loading, a schema-2 generation-8 commit with valid CRC, and
the selected preset/override/one edited weight restored in the UI after console
reboot. This does not test every weight through the UI or establish full settings
persistence. Controller reconnect remains unverified: the KVM reattach request
did not produce an SDL device-change event in that run.

## Global menu settings, schema 3

Schema 3 appends 44 fields after the unchanged schema-2 fields (499 values,
2008 payload bytes). `PreferenceGameplayFields.inc` fixes their order and exact
allowed values, audited against the pinned option menus. These cover gameplay,
team behavior, eye candy, item timing, weapon behavior/limits and frame timing.
The desktop fullscreen flag remains a platform choice; Xbox video is fixed by
the port. Mode-specific settings, controller configuration, skins and asset pack
selection remain open.

Readers migrate both schemas 1 and 2 while retaining defaults for fields absent
from the older payload. Decode validates ranges before narrowing to game field
types, then checks discrete menu choices before applying the complete staged
configuration. Host sanitizer tests cover both migrations and unsupported values
inside otherwise valid numeric ranges. `test-results/global-save-v3-ui.*` records
native UI changes to respawn timing and team collision, a CRC-verified generation
12 save, matching UI after console reboot, restoration of defaults and clean
unguarded Exit with zero stderr. This covers two edited fields through the UI;
mode-specific settings and the remaining release checks are still open.

## Native recovery fixture

`tools/run-save-recovery.py` backs up both existing schema-3 slots, installs one
temporary bad newer slot, launches an explicitly hash-bound executable, resets
through the KVM, checks that the older generation loaded without error output,
and restores both original records with exact readback. It refuses ambiguous
generations or an existing evidence directory. Inspect `result.json` before
further launches if recovery or restoration cannot be confirmed.

From the game checkout, use the preserved schema-3 candidate and its manifest:

```sh
python3 ports/nxdk/tools/run-save-recovery.py --kind invalid-choice \
  --xbe ../../artifacts/global-save-v3-late-pad/default.xbe \
  --reference-manifest ../../artifacts/global-save-v3-late-pad/build.json \
  --node /absolute/path/to/node \
  --reset-helper ports/nxdk/tools/kvm-reset.mjs \
  --output ../../logs/hardware/save-recovery-new-run
```

Other fixtures are `bad-crc`, `truncated` and `unsupported-schema`. The reset
helper requires Node 22 or later and the established ESP KVM transport; its
default host is 192.168.1.169 (`KVM_URL` overrides it). Xbox FTP defaults to
192.168.1.254 and the test title folder. This is a warm-launch recovery test,
not sudden-power-loss or physical-controller acceptance.

The first native invalid-choice fixture passed: a valid-CRC generation-14 record
with respawn 21 was rejected in favor of generation 13, with zero stderr. Both
original slots were restored and verified byte for byte. See
`test-results/save-recovery-invalid-choice.*`. The other fixture kinds require
their own native results; availability in the runner alone is not a pass.

## Mode settings, schema 4

Schema 4 appends all 105 values in `GameModeSettings`: 604 values total and
2428 payload bytes. `PreferenceModeFields.inc` encodes each scalar and array
element explicitly, with choices audited against `ModeOptionsMenu.cpp`,
`MI_FrenzyModeOptions.cpp` and the real constructors. Array shapes are checked
at compile time. The valid flag-speed sentinel -1 uses signed little-endian
32-bit decoding; values are checked before narrowing to the game field type.
The upstream constructor's health max-life value 1 remains supported even though
the menu offers 2–10.

Host ASan/UBSan tests instantiate the real mode structures and constructors,
round-trip all fields and all array elements, retain runtime pointers, migrate
schemas 1–3, reject truncated/out-of-range payloads, and check valid/invalid
negative values. Native schema-4 testing is still pending. Mode goal values
stored outside `CGameConfig`, controller assignments/bindings, skins and pack
selections remain separate unfinished integration work.

The pinned nxdk libc++ lacks `std::bit_cast`, despite compiling the game in
C++20 mode. Signed wire values therefore decode arithmetically through int64_t;
no newer standard-library function or implementation-defined unsigned-to-signed
conversion is required.

Native `bad-crc` and `truncated` fixtures also passed with the preserved schema-3
XBE: each loaded the older generation 13, recorded zero stderr, and restored
both original slots with exact readback. The versioned results are
`test-results/save-recovery-bad-crc.*` and `save-recovery-truncated.*`. These
results do not qualify the later schema-4 executable.

The unsupported-schema native fixture did load the correct older generation 13
and restored both original files. Its overall run failed due to the independent
intermittent audio startup stall (73 stderr bytes). See
`test-results/save-recovery-unsupported-schema.*` and `AUDIO-STARTUP.md`; do not
count it as an error-free release run.

## Mode goals, schema 5

Schema 5 appends the 22 normal-mode goals (626 values, 2516 payload bytes).
A temporary snapshot captures the initialized modes' live goal-choice tables;
those tables and runtime pointers are never serialized. Decode checks every
goal against the current mode's choices before applying either goals or global
configuration. Slot selection performs that semantic check too, so a newer
CRC-valid but unsupported goal can fall back to an older valid record.

Schemas 1–4 remain readable and leave goal values at current defaults. Saving
a runtime/tour goal absent from its menu uses the engine's nearest-menu-goal
representation without changing the active match; this also represents the
engine's 1000-to-999 scoreboard cap safely. Host tests cover all 22 goals,
unlimited (-1), migration from schema 4, invalid values inside the short range,
all-or-nothing apply and older-slot fallback. Native goal persistence is pending.
Controller configuration remained unfinished at schema 5; schema 6 below adds skins and pack choices.

## Asset names and random skins, schema 6

Schema 6 has 630 explicit integer fields, then a bounded name section for seven
pack choices (announcer, game music, world music, sound, menu/world/game graphics)
and four player skins. Four random-skin flags are included in the integer prefix.
Each name is length-prefixed, 1–255 bytes, and matched case-insensitively for ASCII
against the live enumerated choices. It never stores unchecked file paths or
restores list indices from a different asset installation. Runtime choice tables
and pointers are not serialized. The maximum payload is checked against the
16 KiB record limit at compile time.

Invalid lengths, control bytes, path separators and trailing bytes reject the
complete snapshot. A syntactically valid name for a removed pack/skin retains
that slot's current valid default and logs `fallback=1`, while preserving other
settings. Loaded choices are applied only after full record/semantic validation.
Schemas 1–5 remain readable and retain current asset defaults.

Host sanitizer tests cover all names, reordered catalogs, missing assets, ASCII
case matching, maximum lengths, malformed lengths/bytes, every truncated prefix
and legacy migration. Native asset/skin persistence is pending. Controller
configuration remains unfinished.

## Controller profiles and player choices, schema 7

Schema 7 appends 144 explicit integers before the asset-name section: for each
of four players, player type (off/human/CPU), selected keyboard/joystick profile,
and the two profiles' device IDs and sixteen bindings. Keyboard device IDs must
be -1; joystick indices must be 0–3. Joystick bindings must fit the game's
30-entry binding-name table. Keyboard bindings accept Unicode scalar values,
SDL2 scancode keycodes 0–511, and the existing unbound sentinel -1. Runtime
pointers and held/pressed states are never serialized. Load rebinds each
pointer to the destination configuration and clears transient input state.
Schemas 1–6 retain caller defaults for fields they did not store.

Startup installs Xbox defaults before loading preferences, so loaded controls
are retained. If player one's selected device is absent, or is keyboard, the
first attached joystick supplies standard D-pad/A/B/X/Y menu navigation only.
This lets the user reach Options without rewriting saved gameplay assignments.
Changing recovery-device availability clears held state. Gameplay retains the
configured profile; an absent controller is not silently replaced by a bot.
Physical USB port stability still requires hardware qualification.

Host sanitizer tests cover schema-7 round trips, distinct per-player bindings,
invalid keycodes, all truncated payload prefixes and schema-6 migration.
`python3 ports/nxdk/tests/run-controllers.py` exercises production nxdk input
processing with four host SDL virtual devices, simultaneous held inputs,
instance-ID translation, menu recovery and restoration of custom assignments.
This is host execution, not four physical Xbox controller testing. The schema-7 XBE compiled at `e3458ae1`, SHA256
`8b9744d7bbd0b9f407bd4cb593e4c2964233dfd8681df5e670dd10be8d5aefb1`.
Native save/reload qualification remains pending.

The Xbox controls selector now refreshes when opened, keeps all four logical
controller assignments visible with disconnected labels, and does not let
CPU/off slots reserve a controller. Binding capture uses the same SDL instance
lookup as gameplay, so another pad cannot change the selected pad's binding.
The capture wait polls the platform every 16 ms to preserve reconnect and
Back+Start recovery. A disconnected target exits capture; a keyboard binding
prompt can be canceled with B on an attached pad. These UI changes require
native visual/capture qualification; host tests cover shared instance lookup
and the existing input-routing paths, not the entire graphics menu.

Returning from the Controls selection screen to Main now commits preferences,
matching the existing Options-to-Main save point. Match start and normal Exit
also commit; the direct Back+Start recovery reboot still does not save.

## Native schema-7 checkpoint

`test-results/controller-save-v7-roundtrip.*` binds the first controller round
trip to XBE `0146f93a...` / source `bfac4a33`: Jump A→Y, Use Item Y→A,
Controls-to-Main commit of generation 15, reboot, and visible restored bindings.
The raw record's envelope CRC and explicit binding positions were independently
checked. The first session had zero stderr and progressing audio. This is one
virtual pad; it does not qualify four physical controllers or every preference.
Subsequent tests restore defaults and exercise missing-device/menu recovery.

The recovery fixture runner now supports bounded schema-3..7 baselines,
including the variable-length asset-name section in schemas 6–7. Structural
preflight is separate from the game's semantic validation. It rejects malformed
lengths/counts/names and CRC failures before touching either live slot.
`invalid-controller` requires a schema-7 newer slot and writes device index 4
with a recomputed CRC to test semantic fallback. Existing `invalid-choice`,
`bad-crc`, `truncated` and `unsupported-schema` fixtures retain backup/readback/
restore behavior. Use the exact preserved build's manifest and a new output
folder. Never overlap a fixture runner with another hardware session.

The host preflight check takes actual saved records as inputs:

```sh
python3 ports/nxdk/tests/run-save-preflight.py /path/to/smw-pref0.bin /path/to/smw-pref1.bin
```

It passed on the native generation-15 schema-7 record and the older schema-3
record, covering every truncated prefix and one-bit corruption per byte plus
CRC-valid invalid headers, counts, asset lengths and names. This is fixture
preflight coverage; native recovery for these later schemas still requires its
own exact-XBE runs.

## Physical controller slots, schema 8

The pinned nxdk Xbox SDL backend documents player indices as physical ports
1–4 (`src/joystick/xbox/SDL_xboxjoystick.c`), whereas its device enumeration is
dense and can change after disconnect. Schema 8 retains the 774-integer layout
but identifies joystick devices as zero-based physical slots. Gameplay, menu
recovery, binding capture and device labels use the same slot/handle lookup;
removing port one no longer makes the remaining pads become different players.
Initial defaults assign detected physical slots in ascending order, so a lone
pad in another port can still be player one. Saved choices survive hotplug.

Schema-7 bindings/player choices are retained and old dense device indices are
imported through the current enumeration's physical-port map. An absent old
index retains its numeric slot, and the game logs this migration limitation:
its historical physical identity cannot be reconstructed. Schemas 1–6 retain
the caller's new controller defaults. Schema-8 loads never reinterpret slots
through a later enumeration order. When the SDK cannot report USB topology,
its device-index fallback remains available; physical stability is unverified
for that fallback and nonstandard hubs/devices.

Host sanitizer tests now cover four virtual devices with nxdk-style 1–4 port
indices, permuted enumeration, removal with dense compaction, menu recovery
from a missing first port, and reconnect at the end of the device list. Save
tests cover schema-7 migration under a permuted map, invalid-map all-or-nothing
failure, and unchanged schema-8 assignments under a later map. Native build,
7→8 migration and one-pad qualification remain pending at this change; four
physical controllers are unavailable. Earlier schema-7 XBE results are not
transferred to this new executable.
