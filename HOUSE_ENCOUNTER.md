# Procedural house encounter

Open `Content/Procedural/Lvl_HouseEncounter.umap` in this checkout after building
`prototype3Editor`. Select **House Encounter - Seed and Spawn Settings**, change
the settings in **Encounter**, and press Play. Restart the editor after C++ changes.
The original House Builder and First Person maps are unchanged.

The map starts the existing first-person character on a staging platform, generates
a furnished house with the existing builder, builds runtime navigation, then places
the player just inside the south entrance facing inward. Walk back through that
door to leave. A small landing supports the exit. Room lights and an EXIT label
are supplied at runtime. The inventory test pile and damage spikes are disabled
for this map; the existing starting inventory and controls remain available.

## Settings

| Setting | Default / contract |
| --- | --- |
| House Settings | Existing builder settings, including Seed (12345), furniture, room count and navigation pattern |
| Zombie Count | 3; accepts 0..32, exact count required |
| Item Count | 6; accepts 0..64, exact count required |
| Zombie Class | `ZombieCharacter`; configurable Character subclass |
| Item Selection | Nine existing ItemIds; seeded selection with replacement; repeat an ID to increase its weight |
| Next Sections | Street, Another house, Shelter; at least two distinct nonempty names |
| House Style | Optional existing house style asset; collision is checked against emitted meshes |

The encounter uses the existing `AZombieCharacter`, with its Manny presentation,
perception, movement, combat and `/Game/Zombies/ST_ZombieBasic` behavior tree.
The existing zombie controller starts automatically for spawned characters.
See `ZOMBIE_GUIDE.md` for NPC behavior and tuning. This integration does not change
that behavior. The old `HouseZombiePlaceholder` remains available as an optional
debug spawn class; it is no longer the default or the saved encounter map's class.

Class-default capsule dimensions are used to validate routes and positions.
Classes that resize their capsule on spawn are rejected. Extra colliding components
and behavior side effects of future custom NPCs require their own integration checks.

Existing collectible types: Bandage, CannedBeans, WaterBottle, Knife, Pistol,
Flashlight, Jacket, SmallBackpack, ScrapMetal. Each is a real `ADroppedItem` with
a fresh owned `FItemInstance` and its current inventory profile. Use the existing
floor inventory or direct hand pickup controls. No new item actions are added.

## Placement and failure contract

### Generation ranges and placement rework

Enable **House Settings > Use Generation Ranges**, then expand **Generation
Ranges** for Width Cells, Depth Cells and Room Count minimum/maximum values.
The first time ranges are enabled in Details, both bounds inherit the existing
fixed width, depth and room count. Later off/on toggles preserve edited ranges;
fixed controls remain separate. Existing saved custom ranges are retained.
Equal bounds mean a fixed value. A cell is **Cell Size** centimetres (100 cm by
default). Width/depth describe the actual house footprint between exterior wall
centre lines, not individual room dimensions or merely a maximum envelope.
Hallways count as rooms. Room Sizing continues to constrain planned room interiors.

The seed resolves all three ranges once. Generation retries keep those dimensions
and count; they cannot silently grow, shrink or raise the requested count. An
incompatible sampled combination fails with its resolved values. Existing maps
keep their previous controls until range mode is enabled. Planned layouts retain
their residential hallway topology; arbitrary building types and room tags remain
future work.

Loot now uses each selected type's actual oriented box, supported at its centre
and corners, plus a separate reachable player standing position within pickup
range and with a clear view. Furniture bodies, front access and door approaches
remain reserved. Loot uses a 50 cm candidate lattice; zombies retain 75 cm.
Four deterministic arrangement attempts alternate zombie/loot priority without
changing that building or counts. If all fail, the encounter tries another house
arrangement, up to four houses total. Each house uses the existing builder's
bounded generation retries and a fresh navigation build. In range mode all four
houses keep the same resolved dimensions/count; loot selection and requested
zombie/item counts also stay fixed. Failed layouts are cleaned before the next
attempt. Larger items are placed first. Pickup approaches
stay clear of other initial occupants. Result > Placement includes Item Access Feet;
Placement Report records attempts and best counts. These are search results,
not a measurement of the building's theoretical capacity.

During preparation a centered status prompt owns movement/look input. Success
removes it and restores control. Exhausting the attempts shows the failure reason,
**Try another seed**, and **End Play** (or **Exit game** outside the editor).
Retry starts a deterministic new request seed with the original snapshotted
settings; it can choose new values within the original ranges. It never changes
editor controls or drops requested occupants. The retry button is disabled during
preparation and after success. Invalid configuration requires ending Play to fix
settings; a fixed one-room house with zombies is rejected before geometry is built.
Result > Request Seed, Layout Attempts and Generation Status identify the request
and outcome. Re-enter the reported Request Seed to reproduce a manual retry.

On a generated House Builder, **Draw Layout Debug** shows red furniture bodies,
orange furniture access and green doorway approaches for 30 seconds. These shared
clearance regions are mathematical rules, not extra physical obstacles.

- The house uses the current builder, including furniture and deterministic builder
  retries. `House.GeneratedSettings` records the effective house settings.
- The zombie candidate lattice is shuffled by the requested seed. Candidates
  must have supported floors, capsule clearance from static/dynamic/pawn/physics
  geometry, a complete Recast path to the entrance, and a capsule sweep along that
  path. NPC and player dimensions are both considered. Loot uses the separate
  body and pickup-access checks described above.
- Player and exit positions reserve the doorway. Spawn centres stay at least
  250 cm from the player. Zombies are separated by at least 110 cm (increased for
  larger capsules); loot spacing follows its physical size and pickup access.
  Loot, NPCs and player never share a spawn position.
- Zombies cannot initially spawn anywhere in the player's starting room. The
  exclusion includes a zombie capsule margin at doorways and supports translated
  and rotated houses. Zombies can enter that room afterward through normal AI
  navigation. Loot remains eligible there, subject to the existing clearance and
  250 cm player-distance checks. Loot has its own candidates, so the zombie-only
  room exclusion does not discard loot positions.
- A single-room house with any requested zombies cannot satisfy this rule and
  fails safely. Counts remain exact; no zombie is placed in the starting room as
  a fallback.
- Generation, navigation and all spawn counts must succeed before the player is
  moved. A rejected layout destroys the encounter's house, landing and spawned actors,
  preserves unrelated world actors and the player's location/loadout. Exhausted
  recovery sets `State = Failed` and reports `LastError`. It never silently reduces counts.
- The search is deliberately bounded and conservative. It may reject a house that
  a finer placement solver could use. Change the seed or lower counts and restart
  Play after failure. Do not regenerate an active encounter's owned house.
- Use an upright actor at unit scale; translation and yaw are supported. The
  encounter creates and owns runtime navigation bounds covering its actual house
  and exit landing, including enlarged layouts beyond the example map's original
  bounds. Failure/destruction also removes these bounds. Runtime navigation is
  Dynamic on this map's `HouseEncounterNavMesh`, without a project-wide nav change.

Settings are snapshotted at preparation; editing counts, item selection, seed or
NPC class while navigation builds does not change that encounter.

The same requested seed, settings, generator version and surrounding collision
produce the same house, spawn transforms and loot types. Fresh instance GUIDs are
intentionally unique on every run; physics motion and future NPC behavior are not
made deterministic. Changing engine navigation settings can change accepted paths.

## Debug tools

During Play, open **Esc > Options > Debug** and enable **Invisible to zombies**.
This immediately cancels existing zombie pursuit/attacks, clears perception clues,
prevents sight and footstep detection, and blocks zombie damage. Turning it off
restores normal sensing and damage. It does not hide the character visually or
disable environment damage. The toggle applies and saves immediately; both on
and off choices survive new Play sessions, editor restarts and recompilation.
It defaults to off until changed. The local preference is stored in
`GameUserSettings.ini`, in the separate `/Script/prototype3.PrototypeDebugSettings`
section, and is applied before player startup. It does not require Apply changes,
change control bindings, or get cleared by resetting controls. Maps and source
files do not store the user's choice.

## Exit state

Crossing the entrance outward with the assigned player offers the section prompt.
The prompt closes the inventory, stops item/melee actions and movement input, and
shows clickable choices. Selecting one records `ExitDecision.Selected`, marks the
encounter Complete, and disables every choice, including resubmission. Close
returns control to the same world. There is no travel or save persistence, and
walking through the exit again cannot reopen the decision. This is local,
single-player state for the lifetime of that encounter.

## Verification

### Verified November 4, 2026 — recovery and range controls

Win64 Development Editor build passed with no compiler warnings. All **36/36**
checks passed: `Prototype.House`, `Prototype.Encounter`,
`Prototype.Inventory.PauseMenu`, `Prototype.Inventory.InputPreferences`.
The real Details properties inherit fixed dimensions/counts on first enable on
both actor types and retain authored ranges across subsequent toggles.
Seed 2235 at 12 x 10 cells, 4 rooms and 3 zombies/6 items succeeds on its second
house arrangement. This does not establish that the previously requested 15/23
population fits that footprint.

Blocked-world tests exhaust four distinct layouts, preserve dimensions and player
position, remove failed actors, and reproduce successful manual retries while
retaining snapshotted counts. An automated Play test uses the actual status/retry
widget, checks disabled retry during preparation, and verifies control restoration
after success. The zombie movement fixture now uses explicit in-memory settings
instead of depending on personal map edits; the authored map is never saved.

Reports: `Saved/GenerationRecoveryVerification/Build.log`, `Checks/index.json`,
`Checks.log`; adjacent `RunChecks.ps1` repeats with isolated preferences and map/
settings hash guards. 22 checks passed cleanly and 14 with 17 existing crowd-manager
teardown warnings; none failed or were unrun. No compiler warnings remain.
Initial verification caught an editor property callback issue and a NullRHI UI
attribute-refresh issue in the test; both were corrected and the full suite rerun.
Earlier reports are retained. No manual visual or packaged-build validation was done.

### Verified November 3, 2026 — generation rework first pass

Win64 Development Editor build passed with zero compiler warnings. All **32/32**
checks passed: `Prototype.House`, `Prototype.Encounter`,
`Prototype.Inventory.PauseMenu`, `Prototype.Inventory.InputPreferences`.
Coverage includes 128 range-resolution seeds, 24 reproducible furnished ranged
layouts, exact planned footprints across both residential patterns/difficulties,
range serialization and invalid/incompatible settings, shared clearance geometry,
existing generator regressions, rollback, starting-room exclusion, ownership,
exit choices, actual zombie movement and saved Debug preferences.

Compact DeepBranches seed 217 now passes at the full 3 zombies/6 items, including
pickup from each recorded approach. All nine collectible bodies pass rotated-house
placement, reproducibility, approach clearance, failed/successful pickup and unique
ownership checks. This replaces the historical compact-house limitation below.

Reports: `Saved/GenerationReworkVerification/Build.log`, `Checks/index.json`,
`Checks.log`; repeat with adjacent `RunChecks.ps1`. The first run had one stale
test assertion checking provisional positions instead of actual typed placements;
it was corrected and the full suite rerun. Eleven tests passed with warnings:
13 existing crowd-manager teardown messages and one Recast tile recreation.
Startup also lists unavailable non-Windows SDKs; Win64 is valid. No warnings were
suppressed. Actual local map and user settings hashes remained unchanged.
No manual Play/visual or packaged-build check was performed.

This remains a bounded search: some feasible placements may still be missed,
incompatible range samples fail, and furnishing/topology are not redesigned.
Physics motion after spawning is outside seed reproducibility. Initial pickup
positions are validated; dynamic zombie movement can subsequently occupy them.

### Verified November 3, 2026 — saved Debug preferences

Win64 Development Editor build passed with no compiler warnings. All **12/12
encounter/settings checks passed**, none failed or unrun: `Prototype.Encounter`,
`Prototype.Inventory.PauseMenu`, and `Prototype.Inventory.InputPreferences`.
The Debug test uses the actual checkbox, reads saved on/off values directly from
disk, reloads configuration, and verifies startup state and menu state across
three fresh Play worlds. Existing visibility/noise/pursuit/attack/damage checks,
encounter placement, pickup, rollback, reproducibility and controls checks pass.

Reports: `Saved/DebugPersistenceVerification/Build.log`, `Checks/index.json`,
`Checks.log`; repeat with the adjacent `RunChecks.ps1`. Tests use an isolated
user-settings file through `-GAMEUSERSETTINGSINI`. Hash checks confirm the user's
actual settings file and edited encounter map remain unchanged. Eight tests
passed with warnings: ten familiar crowd-manager teardown messages and one
Recast tile-allocation recreation. No warnings were suppressed. No manual Play,
editor-restart or packaged-build pass was performed.

### Verified November 3, 2026 — starting-room exclusion

Win64 Development Editor build passed with no compiler warnings. All **9/9
`Prototype.Encounter` checks passed**, none failed or unrun. Coverage includes
starting-room exclusion for six furnished seeds across all three layout patterns,
translated/yaw-rotated repeatability, complete zombie paths toward the starting
player, pickup/ownership, exit decisions, and actual zombie movement/Debug behavior
in automated Play. A large single-room case verifies that loot remains eligible
there, even beyond the old 250 cm player radius, while a request for one zombie
fails and rolls back without moving the player or leaving partial occupants.

Reports: `Saved/HouseStartRoomVerification/Build.log`, `Checks/index.json`, and
`Checks.log`. Eight tests passed with warnings: nine existing crowd-manager
teardown messages and one Recast serialized tile-allocation recreation. No
warnings were suppressed. The user's locally edited map was left unchanged.
No manual Play/visual or packaged-build validation was performed.

### Verified November 3, 2026 — real zombies and Debug tools

Updated from main `c8d3289` (zombie PR #29) on local branch
`house-zombie-integration`. The saved encounter map now selects `ZombieCharacter`;
its existing seed 234, 15 rooms, and other authored settings were retained. A
pre-integration map backup is in `Saved/HouseZombieVerification/MapBeforeZombieIntegration.umap`.

Win64 Development Editor build passed with no compiler warnings. The final
headless regression run passed **27/27 tests**, zero failures or unrun tests:
eight encounter/Debug checks, six zombie checks, two house checks, four item
checks and seven inventory/UI checks. Automated Play verified running zombie
controllers/StateTrees, paths toward the player, and actual movement in the
generated house. The movement check exercises the outer house area around
X=3600..3850 cm, beyond the old navigation envelope. Debug verification uses the
actual menu checkbox, cancels an active attack, tests sight/hearing/damage
exclusion, then restores normal detection and damage by toggling off.

Reports: `Saved/HouseZombieVerification/Build.log`, `Final/index.json`, and
`Final.log`. Twelve tests passed with warnings: 18 engine crowd-manager teardown
warnings, plus one Recast message recreating the serialized tile allocation when
runtime bounds expand. No warnings were suppressed. No manual Play/visual or
packaged-build validation was performed for this update.

The test selection was `Prototype.Encounter+Prototype.Zombies+Prototype.House.GeometryAndCollision+Prototype.House.LayoutSeeds+Prototype.Items+Prototype.Inventory.FloorTransfers+Prototype.Inventory.HandPickupRules+Prototype.Inventory.BackpackInputRouting+Prototype.Inventory.PauseMenu+Prototype.Inventory.ItemActionContracts+Prototype.Inventory.HeldItemDrop`.

Run `Scripts/VerifyHouseEncounter.ps1 -EngineRoot C:\UE_5.8` to build and run the
focused regression set. It creates a fresh timestamped report and fails on missing,
unrun or failed tests. Use `-SkipBuild` only when the compiled source is current.

`Prototype.Encounter` contains six headless checks for the saved map, exit
decisions, full placement/navigation/pickup, all nine collectible ownership
transfers, reproducibility, and transactional failure, plus an automated Play
integration check for real zombie controllers, running behavior trees, complete
navigation paths and actual movement through the generated furnished house. The map
creation path is explicitly opt-in (`-CreateHouseEncounterMap`) and refuses to
overwrite an existing asset. Ordinary runs load and validate the saved map.
`-UpgradeHouseEncounterZombies` with `Prototype.Encounter.ExampleMap` upgrades only
the legacy placeholder class in an existing map and saves/reloads it; other
settings, including the seed, are preserved.

Automated checks use physics worlds and real Recast path queries. They do not
establish visual quality, gameplay feel, NPC movement behavior, or packaged builds.
The user performs Play testing. Verified commands/results are recorded separately
in the project continuity notes and the run's `Saved/HouseEncounterVerification`.

### Historical verification — October 30, 2026 (placeholder version)

`Scripts/VerifyHouseEncounter.ps1 -EngineRoot C:\UE_5.8` completed successfully:
Win64 Development Editor build, **19 passing tests, 0 failures, 0 unrun**.
The compiler reported no warnings. `git diff --check` also passed.

| Checks | Coverage |
| --- | --- |
| 6 Encounter tests | Six furnished houses: seeds 12345, 7, 42, 93, -19 at 3 zombies/6 items; compact Deep Branches seed 217 at 1/3. Collision-checked Recast routes, exact counts, actual NPC path queries, clear occupants, hand pickup, entrance crossing, state lockout and cleanup. Separate repeated yaw-37-degree house checks reproduce positions/types while using fresh item identities. All nine collectible types pass failed-destination and successful hand pickup checks. Occupied and overcrowded houses roll back. Saved map reloads. |
| 2 House regressions | 1,250 layout cases plus 42 geometry/collision cases from the existing generator suite |
| 4 Item regressions | ModelValidation, BandageAsset, IntentValidation, SampleAssets |
| 7 Inventory/UI regressions | FloorTransfers, HeldItemDrop, HandPickupRules, ItemActionContracts, BackpackInputRouting, PauseMenuControls, PauseMenuDefaultSettings |

Final reports (relative to this checkout):
`Saved/HouseEncounterVerification/Build.log` and
`Saved/HouseEncounterVerification/Checks-20261030-025348/index.json` (adjacent `.log`).

Four tests passed with the same engine warning during temporary-world teardown:
`LogCrowdFollowing: Unable to find RecastNavMesh instance while trying to create UCrowdManager instance`.
The log places these after `UWorld::CleanupWorld`; route assertions pass before teardown.
No warning suppression or engine changes were made. Startup also reports an editor-layout
version migration and unavailable non-Windows SDKs; Win64 is valid. No compiler or test errors remain.

Known limit: compact Deep Branches seed 217 with the default 3/6 count is rejected
by the conservative route/placement search (only 3 zombie and 3 item positions fit
its checks). No partial encounter survives; 1/3 is verified. These checks validate
initial placement and path availability, not future moving-NPC behavior, physics
determinism, visual presentation, actual section travel, save/load, multiplayer or
packaged builds. No assistant Play session was run; user Play testing is pending.

## Code

- `Gameplay/Procedural/Encounter/HouseEncounter.*`: lifecycle, transactional spawning,
  lighting, entrance crossing and terminal decision.
- `HouseEncounterPlacement.*`: occupied geometry, floor support, Recast routes,
  capsule sweeps and deterministic selection.
- `HouseEncounterSupport.*`: visible NPC stand-in and map-local navigation classes.
- `HouseEncounterGameMode.*`: existing player/controller with test fixtures disabled.
- `HouseEncounterTypes.*`: placement result and exit-decision state.
- `Core/Settings/PrototypeDebugSettings.h`: saved local Debug preferences.
- `Core/PlayerControllers/prototype3PlayerController.*`: prompt and interface/input lifecycle.
- `Gameplay/Items/World/DroppedItem.*`: fresh world-owned item factory; existing pickup path.

Paths above are relative to `Source/prototype3`.
