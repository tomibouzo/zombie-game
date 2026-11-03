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
| Zombie Class | `HouseZombiePlaceholder`; configurable Character subclass |
| Item Selection | Nine existing ItemIds; seeded selection with replacement; repeat an ID to increase its weight |
| Next Sections | Street, Another house, Shelter; at least two distinct nonempty names |
| House Style | Optional existing house style asset; collision is checked against emitted meshes |

The current repository has no zombie NPC. The visible labelled characters are
stationary placeholders with no AI or combat behavior. Assign the actual zombie
Character class when available; behavior remains the responsibility of that NPC.
Class-default capsule dimensions are used to validate routes and positions.
Classes that resize their capsule on spawn are rejected. Extra colliding components
and behavior side effects of future custom NPCs require their own integration checks.

Existing collectible types: Bandage, CannedBeans, WaterBottle, Knife, Pistol,
Flashlight, Jacket, SmallBackpack, ScrapMetal. Each is a real `ADroppedItem` with
a fresh owned `FItemInstance` and its current inventory profile. Use the existing
floor inventory or direct hand pickup controls. No new item actions are added.

## Placement and failure contract

- The house uses the current builder, including furniture and deterministic builder
  retries. `House.GeneratedSettings` records the effective house settings.
- A fixed 75 cm candidate lattice is shuffled by the requested seed. Candidates
  must have supported floors, capsule clearance from static/dynamic/pawn/physics
  geometry, a complete Recast path to the entrance, and a capsule sweep along that
  path. NPC and player dimensions are both considered. Loot uses conservatively
  cleared standing space and a separate actual body check.
- Player and exit positions reserve the doorway. Spawn centres stay at least
  250 cm from the player and are separated by at least 110 cm (increased for
  larger capsules). Loot, NPCs and player never share a spawn position.
- Generation, navigation and all spawn counts must succeed before the player is
  moved. Failure destroys the encounter's house, landing and spawned actors,
  preserves unrelated world actors and the player's location/loadout, sets
  `State = Failed`, and reports `LastError`. It never silently reduces counts.
- The search is deliberately bounded and conservative. It may reject a house that
  a finer placement solver could use. Change the seed or lower counts and restart
  Play after failure. Do not regenerate an active encounter's owned house.
- Use an upright actor at unit scale; translation and yaw are supported. Keep the
  house and landing inside the map's navigation bounds. Runtime navigation is
  Dynamic on this map's `HouseEncounterNavMesh`, without a project-wide nav change.

Settings are snapshotted at preparation; editing counts, item selection, seed or
NPC class while navigation builds does not change that encounter.

The same requested seed, settings, generator version and surrounding collision
produce the same house, spawn transforms and loot types. Fresh instance GUIDs are
intentionally unique on every run; physics motion and future NPC behavior are not
made deterministic. Changing engine navigation settings can change accepted paths.

## Exit state

Crossing the entrance outward with the assigned player offers the section prompt.
The prompt closes the inventory, stops item/melee actions and movement input, and
shows clickable choices. Selecting one records `ExitDecision.Selected`, marks the
encounter Complete, and disables every choice, including resubmission. Close
returns control to the same world. There is no travel or save persistence, and
walking through the exit again cannot reopen the decision. This is local,
single-player state for the lifetime of that encounter.

## Verification

Run `Scripts/VerifyHouseEncounter.ps1 -EngineRoot C:\UE_5.8` to build and run the
focused regression set. It creates a fresh timestamped report and fails on missing,
unrun or failed tests. Use `-SkipBuild` only when the compiled source is current.

`Prototype.Encounter` contains six headless checks for the saved map, exit
decisions, full placement/navigation/pickup, all nine collectible ownership
transfers, reproducibility, and transactional failure. The map
creation path is explicitly opt-in (`-CreateHouseEncounterMap`) and refuses to
overwrite an existing asset. Ordinary runs load and validate the saved map.

Automated checks use physics worlds and real Recast path queries. They do not
establish visual quality, gameplay feel, NPC movement behavior, or packaged builds.
The user performs Play testing. Verified commands/results are recorded separately
in the project continuity notes and the run's `Saved/HouseEncounterVerification`.

### Verified October 30, 2026

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
- `Core/PlayerControllers/prototype3PlayerController.*`: prompt and interface/input lifecycle.
- `Gameplay/Items/World/DroppedItem.*`: fresh world-owned item factory; existing pickup path.

Paths above are relative to `Source/prototype3`.
