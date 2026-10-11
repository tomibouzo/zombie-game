# Procedural house blockout

The first milestone is a deterministic, single-storey house with a rectangular footprint,
connected typed rooms, matching furniture blockouts, one entrance on the local south edge, variable window openings, and a flat roof.
All geometry uses collision-enabled instanced boxes. No PCG plugin is required.

The navigation pass adds deliberate **Central Hub** and **Deep Branches** plans with
Easy/Normal/Hard/Custom navigation settings. Existing actors default to **Original Random Layout**.

## Try it

1. Build the `prototype3Editor` target with UE 5.8, then open the project. After a C++ update,
   save your current work and restart Unreal to load the new reflected fields/components.
2. Open `/Game/Procedural/Lvl_HouseBuilder`. Select **House Builder - Select and Generate House**.
3. In Details, select **All**, open **House Controls > Layout**, change **Seed**, press Enter,
   and click **Generate House**. **Generate New House** changes the input seed for you.
4. Click **Generate House** once after upgrading an older map to add room types, furniture, and
   variable windows. The existing map is not automatically overwritten. Press Play to enter through the south doorway. The example uses the project's default game mode.
5. Enable **Hide Roof** to inspect the layout. **Draw Layout Debug** shows colored room outlines
   and room names/IDs for 30 seconds; enable viewport realtime to refresh debug drawing.
6. **Show Labels** toggles top-facing room and furniture names, easiest to read from above with
   the roof hidden. These labels are for the blockout stage.
7. **Clear House** removes this actor's generated instances. Generate/Clear support editor Undo.

You can also drag the native **HouseGenerator** class from the Content Browser's C++ Classes
into any level and click Generate House. Use unit actor scale; translate and rotate freely.
The actor origin is the southwest footprint corner at the finished floor height, with +Z up.
The slab extends below that origin. Provide level ground outside the entrance.

The example's lights and Player Start are ordinary level actors, not owned by the generator.
Move them as needed after changing the footprint/seed. The example has no zombie/navigation setup.

## Linux editor popup workaround

An observed NVIDIA 615.71.09 crash occurred in `FVulkanFenceManager::WaitForFence` while
opening the Pattern dropdown, with the X11 backend already active. It happened in Vulkan
presentation, before house generation. `Editor/HouseDetails.cpp` now supplies Linux-only
in-window enum pickers for Navigation, Household, Room Overrides and furniture-rule types.
These controls do not create a native menu window; their native hover tooltips are suppressed
as well. Other editor dialogs, asset pickers and enum-array elements are not covered.

Restart Unreal after rebuilding this change; existing editor processes retain the old UI code.
Use the existing House Builder launcher. No graphics driver or engine source patch is applied.
Property edits still use Unreal property handles for notifications, undo and reset support.
`Prototype.House.EditorInlineMenus` opens and closes the real Pattern picker in a virtual Slate
window, checks all three pattern values, and verifies that no additional native window appears.
This headless regression test does not establish GPU-driver stability; retry the dropdown in
the normal editor to confirm the reported crash is avoided.

## Settings and behavior

- Defaults: seed 12345, 12 x 10 metre footprint, four rooms, minimum room side three metres.
- Default walls are 300 cm high and 20 cm thick. Doors are 140 x 220 cm; windows are
  nominally 120 x 100 cm with a 100 cm sill, before room rules and variation. Openings are empty: no door leaves or glass yet.
- Width/depth are grid-cell counts. CellSize defaults to 100 cm. In planned modes they are a
  requested envelope, not a demand to fill unused land. Allow Larger Footprint permits automatic
  growth when needed; turn it off to enforce a maximum. Min Room Cells applies to the legacy layout;
  planned rooms use Room Sizing in metres and square metres.
- ExtraConnectionChance adds doors beyond the spanning tree connecting every room.
- Generation runs only when requested. Editing settings alone does not rebuild the house.
- Generated layout and mesh instances are serialized in the map. Save the map after generating.
- Failed generation leaves the previous valid layout/geometry intact and exposes `LastError`.
- `GeneratedSettings` records the settings actually used for the current house.
- Optional `HouseStyle` data assets override floor/wall/roof materials and the block mesh.
  The replacement mesh must be a solid box with simple collision; its bounds are fitted to
  each generated box, including correction for an off-center pivot. This is not yet an
  authored modular wall/door/window kit interface. Only material slot zero is overridden.
- Hide Roof changes visibility, not collision. Blueprint callers can use `SetRoofHidden`.
- `GenerateHouse` and `ClearHouse` are callable from Blueprint at runtime. Enable
  `GenerateOnBeginPlay` if desired. Runtime generation is synchronous and intended for
  small houses, not continuous per-frame rebuilding.
- Reproducibility requires the same settings and generator version. A seed alone is not a
  permanent save format across generator changes. Multiplayer replication is not implemented.

## Household profiles and room purposes

Single-floor residential profiles live under **Settings > Household**. Set Navigation Pattern to
Central Hub or Deep Branches, then choose a profile. **None** disables household-specific purposes and furniture selection.
Start with **Normal, 8 rooms, a 24 x 24 cell envelope, and 100 cm Cell Size**. Use 7 rooms for Easy
or 11 for Hard with the default household requirements. These are starting configurations, not
a promise that arbitrary overrides will fit.
Room Count includes the living room, kitchen, bathroom and all hallway segments.

| Profile | Required private rooms | Spare rooms |
| --- | --- | --- |
| Single Resident / Couple | One primary bedroom | Guest bedrooms |
| Family | Primary bedroom + Children Bedrooms (default 2) | Guest bedrooms |
| Working From Home Couple | Primary bedroom + two separate offices | Guest bedrooms |
| Retired Couple | One primary bedroom | One guest bedroom, then storage; sparse furnishing |

The planner reserves enough private leaves before selecting a layout. An impossible program fails
with an explanation and preserves the generated house. Household needs may change the chosen
layout, but never silently reduce navigation difficulty. Primary/guest/child bedrooms, office and
storage are **Purpose** values on the private-room structural type; all retain hallway access.
Structural Room Type remains Bedroom for compatibility with the navigation graph. Use Purpose
for gameplay queries about beds versus offices. The deepest room initially becomes the primary
bedroom. Manual overrides can change that destination's purpose without changing graph depth.

House-wide controls are independent:
- **Furniture Style:** Traditional, Modern, Mixed Secondhand. No age-to-style assumption is forced.
- Houses are always **Settled**. Old saved Moving In, Moving Out and Vacant values are ignored;
  occupancy is no longer an editable control. Explicit Unfurnished room overrides remain available.
- **Density:** Sparse retains essential pieces, Standard adds normal optional furniture, Full also
  enables children's desks. Placement can still skip pieces to preserve clearance.

**Room Overrides** use the IDs printed on room labels. Set Purpose to Default to keep the preset
purpose, or choose Primary Bedroom, Guest Bedroom, Child Bedroom, Office or Storage for a private
room. Overrides cannot remove the household's primary bedroom, required children's rooms or offices.
Enable Override Density for a room-specific setting, or Unfurnished to leave that room empty.
Room IDs are specific to the current seed/layout: review overrides when changing either.

**Furniture Rules** now filter by Allowed Purposes and Allowed Styles (empty means any), mark
Household Only rules, and distinguish Essential pieces from those requiring a Minimum Density.
Essential pieces are attempted before optional pieces; they are not a guarantee of fit.
Primary bedrooms use a double bed, guest/child bedrooms use single beds. Offices attempt a desk,
chair and shelving; children's rooms can add toys and a desk. Skipped Furniture Count counts
eligible pieces that did not fit, not items intentionally omitted by occupancy/density.

Existing actors may have a saved eight-rule catalogue. Click **Reset Furniture Rules To Defaults**
once to load the new bedroom/office rules before generating a household. This replaces custom
rules (editor undo is available), but does not change other settings or regenerate the house.

For actual art, **House Style > Household Palettes** maps each furniture style to meshes by type
and an optional material. Missing entries fall back to the existing Furniture Meshes/material,
then cubes. Current supplied visuals remain labelled blockouts; style names do not fabricate
traditional/modern models. Desks and chairs are independently placed blockouts, not authored sets.

## Intentional navigation patterns

After rebuilding C++, restart the editor. Select the builder, expand **Settings > Navigation**,
and choose a **Pattern** and **Difficulty**. Start at **Normal, 8 rooms, 24 x 24 cells, Cell Size 100 cm**.
Use **Generate House** after each change. Room Count includes hallway sections.

| Pattern | Easy | Normal | Hard |
| --- | --- | --- | --- |
| Central Hub | Deepest room 2 doors away | 3 doors | 4 doors |
| Deep Branches | Deepest room 2 doors away | 3 doors | 4 doors |

These are exact shortest distances in the room/door graph, starting inside the entrance living
room; the exterior entrance is not counted. They are **navigation presets**, not combat ratings,
walking distances, or measured player difficulty. Changing the seed varies room sizes and wing
assignment while preserving the selected topology constraints.

Both plans put the living room and kitchen at the front, with a distribution hallway and
subdivided private-room wings behind them. The entrance living room has exactly two interior
doors: one to the kitchen and one to the hallway. Bedrooms and the bathroom open only onto
hallways and never serve as passageways. Hallways may connect to other hallways and have no
default furniture. These rules concern **doorway connections**; rooms may share a solid wall.

Each hallway section must directly serve **at least two private rooms** (bedroom, office,
storage or bathroom). Hallway-to-hallway links and the entrance living room do not count.
No single-bedroom corridor stubs or chains of empty, room-sized hallways are allowed.

Both patterns now use a narrow central circulation spine with room groups on both sides.
Central Hub places extra rooms in the entrance hallway group; Deep Branches distributes extra
rooms among the hallway groups. With the minimum room count the patterns share a topology.
These remain single-storey rectangular templates, not arbitrary corridor/loop floor plans.
**Original Random Layout** retains its previous sizing and room assignment; select a planned
pattern to use these rules.

The deepest route ends at a bedroom. Its marker is for inspection; no objective or loot is spawned.
Furniture/window controls remain independent of door planning, and furniture retains geometric
walking-clearance checks. **Extra Connection Chance** applies only to Original Random Layout.

**Custom** exposes an exact **Custom Route Depth** (2..12 interior doors) and a hard
**Custom Max Dead Ends** limit (3..31). Dead ends include the kitchen, bedrooms and bathroom,
but exclude the entrance living room. Unsatisfiable requests fail clearly and preserve the
previous generated house, settings snapshot and report. No silent difficulty downgrade occurs.

A target depth `D` needs `H = D - 1` useful hallway sections and at least `2 + 3*H` total
rooms: **5 for Easy, 8 for Normal, 11 for Hard**. Every section needs two private destinations.
The household can impose a higher minimum. A plan has `Room Count - D` dead ends, including the
kitchen, so Custom Max Dead Ends must cover that count. The room-count limit makes some high
custom depths impossible. Excess rooms may also exceed hallway length/room size limits; increasing
the envelope alone cannot make a corridor or bathroom arbitrarily large.

**Navigation Summary** in the actor's result fields reports target/actual depth, average depth,
dead ends, junctions, loops and the deepest room ID. **Layout > Navigation** stores the per-room
depths, main route and dead-end IDs for Blueprint inspection and saved-map persistence.
**Draw Layout Debug** draws the main room route in cyan through door centres, other connections
in grey, and dead-end labels in red. Hide the roof and enable viewport realtime to inspect it.
These lines show room connections, not collision-tested paths around furniture or a baked navmesh.

Difficulty currently controls door topology. It does not change door width, furniture density,
window sightlines, enemy behavior, locks or exits. Both planned patterns are trees with no loops;
adding shortcuts requires extending the planning/validation rules rather than a random extra door.

## Realistic room sizes

**Settings > Room Sizing** exposes usable area ranges, minimum clear widths, and maximum
aspect ratios per room purpose. The limits apply to Central Hub and Deep Branches. They are game
planning targets informed by residential references, not universal building-code compliance.

| Room | Clear internal area target | Example internal dimensions |
| --- | --- | --- |
| Living room | 18–32 m² | 4.8 × 3.8 m |
| Kitchen | 8–20 m² | 2.8 × 3.8 m; elongated galley layouts allowed |
| Primary bedroom | 11.5–20 m² | 3.8 × 3.8 m |
| Guest / child bedroom | 7.5–16 m² | 2.8 × 3.8 m |
| Bathroom | 4–8.5 m² | 1.8 × 3.8 m or 2.8 × 2.8 m |
| Home office | 7.5–14 m² | 2.8 × 2.8 m |
| Storage room | 3–12 m² | 2.8 × 2.8 m |

A bathroom cannot exceed the smallest actual bedroom's area. Corridors target 1.2–1.8 m clear
width and at most 10 m per section. With the default 1 m grid and 20 cm walls, the generated
corridor is **1.8 m clear** (2 m between wall centre lines). Door width/corner clearance and cell
size constrain that rounding; infeasible combinations fail explicitly. Walls are subtracted when
checking clear dimensions; gross room grid rectangles are not treated as usable floor area.

The generated house can occupy less than Width/Depth Cells. **Size Summary** reports the actual
footprint and requested envelope. **Layout > Footprint Cells** records the actual rectangle, and
room labels show clear dimensions and areas. The roof, exterior walls and coverage checks use
the actual rectangle. Existing saved geometry is left as-is until Generate House is pressed.
Sizing changes can alter seeded layouts and room IDs, so review manual overrides afterward.

References consulted:
- [England's nationally described space standard](https://www.gov.uk/government/publications/technical-housing-standards-nationally-described-space-standard/technical-housing-standards-nationally-described-space-standard): single-bedroom baseline 7.5 m² / 2.15 m width;
  double-bedroom baseline 11.5 m², with one at least 2.75 m wide.
- [Ireland's Quality Housing for Sustainable Communities, chapter 5](https://www.opr.ie/wp-content/uploads/2019/08/2007-Quality-Hsing-for-Sustainable-Communities-1.pdf): room-size planning, circulation access, and kitchen/bathroom fixture clearances.
  The bathroom, office, storage and upper area caps above are our gameplay design choices.

The size-aware packer currently uses paired room groups beside a straight corridor spine.
It can reject requests a more general architectural solver could accommodate; it never silently
relaxes size limits. Furniture remains best-effort with clearance checks, so smaller rooms can
omit eligible pieces. A compact plan does not by itself certify authored furniture fit or navmesh.

## Implementation

`HouseLayout.h/.cpp` owns the data and pure generator. It splits integer rectangles while
preserving capacity for the requested room count, enumerates each shared/exterior boundary once,
selects an entrance, and connects rooms through a randomized spanning tree of usable boundaries.
Optional extra doors and exterior windows follow. Attempts are bounded to 64; invalid inputs
and unsatisfied layouts report errors instead of spawning partial geometry.

`HouseNavigation.h/.cpp` validates the doorway graph and measures shortest door distances.
`HouseResidentialPlan.h/.cpp` reserves household purposes, then packs paired room groups around
useful hallway sections. Bounded search (512 attempts) varies geometry for that fixed topology;
per-wing dynamic programming finds compatible room heights. `HouseSizing.h/.cpp` checks physical
room areas, widths, proportions and the bathroom/bedroom relationship. Difficulty is never
downgraded to make a packing fit.

Validation checks settings, exact room coverage, overlap, canonical boundaries, opening
clearance, one entrance, and reachability. Walls are represented as shared room boundaries,
not as independent walls attached to each room. Short shared boundaries remain solid if an
opening cannot fit. Small intentional end-cap overlaps seal corners and T junctions.

`HouseGenerator.h/.cpp` emits floors, walls around openings, and a roof through persistent
instanced mesh components, plus one instanced component per furniture type. The actor owns its generated components and text labels; regeneration never searches
for or deletes other level actors. Geometry and layout logic have separate responsibilities.

## Room types, furniture, and windows

Each room stores a Blueprint-readable `RoomType` enum: Living Room, Kitchen, Bedroom, Bathroom, or Hallway.
For Original Random Layout:
The entrance leads to the living room. The kitchen is the largest directly door-connected neighbour.
With four or more rooms, the smallest remaining room becomes a bathroom; remaining rooms are
bedrooms. One-room houses only have a living room; two-room houses have a living room and kitchen.
This classifies the existing connected layout. Planned patterns instead use the private-room and
hallway rules above.

The default furniture rules are:

| Room | Furniture |
| --- | --- |
| Living room | Sofa, coffee table |
| Kitchen | Kitchen counter, fridge |
| Bedroom | Bed, wardrobe |
| Bathroom | Sink, toilet |

`Settings > Furniture Rules` is an editable, ordered catalogue: type, allowed rooms, dimensions,
front access clearance, and whether to prefer the centre instead of a wall. Important/larger items
should come first. Each rule attempts one item per matching room. Disable `Generate Furniture`
for an empty blockout. `Layout > Furniture` records placements and `Skipped Furniture Count`
reports pieces that could not safely fit. Placement is best-effort; it never forces an item into a
blocked doorway. Narrow or window-heavy rooms may omit some furniture.

`Walkway Width` (default 90 cm) is used to test routes between all door anchors and furniture
fronts. The solver expands obstacles by half this width and searches a coordinate-compressed
visibility grid. It also rejects furniture overlap, blocked front access, furniture above the
ceiling, and tall pieces covering windows. Low counters may sit below windows. These are geometric
clearance checks, not a baked Unreal navmesh or validation of door-leaf swing animations.

Furniture currently renders as labelled boxes. Optional `HouseStyle > Furniture Meshes` overrides
are keyed by furniture type. They need simple collision and local X=width, Y=depth, Z=height,
front facing +Y. Bounds are fitted to each rule's size; off-centre pivots are compensated. Mesh
materials use slot zero. Collision extending beyond the configured bounds needs separate testing.
No loot, physics simulation, interactions, or furniture damage are implemented yet.

`Window Chance`, `Max Windows Per Wall`, `Window Size Variation`, and `Window Spacing` control
window generation. Windows can occur beside the entrance, and a wall can have several windows. Each room samples
one width, height and sill height shared by all its exterior windows. Profiles vary between rooms
and seeds, while centres are evenly spaced within each usable wall interval. Intervals too narrow
for the room profile omit windows instead of shrinking individual openings. Living rooms favour wider openings; bathrooms use smaller,
higher openings. Windows reserve wall supports and separation from doors, corners, and each other.
`Max Windows Per Wall = 0` or `Window Chance = 0` disables windows.

Window and furniture random streams are separate from floor-plan generation. Adjusting furniture
rules or window controls does not reshuffle the partition/door plan; changing windows can change
which furniture fits safely. Save the generated map to persist rooms, furniture and labels.

## Verification

Run `Automation RunTests Prototype.House` through Unreal's automation console or Session Frontend.
For headless execution, pass:

```
-unattended -nop4 -nosound -nullrhi
-ExecCmds="Automation RunTests Prototype.House;Quit"
-ReportExportPath="<project>/Saved/HouseVerification"
```

- `LayoutSeeds`: 1,250 cases across single-room, four-room, five-room, eight-room, and tightly
  packed sixteen-room settings; room/furniture/window validation, all furniture types, window
  variety, and same-seed repeatability including furnishing and window data.
- `Validation`: malformed layouts, missing/duplicate walls or connections, impossible counts,
  invalid opening dimensions, and bounded footprint inputs. `FurnishingValidation` adds invalid
  furniture/window cases and generation-toggle checks.
- `GeometryAndCollision`: 24 original-layout seeds and 18 household-enabled planned cases spanning both patterns
  and all three presets in a physics world with a rotated/translated builder;
  34 cm radius / 96 cm half-height standing capsule sweeps through every doorway and furniture
  walking route, traces through each variable window, wall/window blocking, floor support, label
  and instance counts, regeneration, invalid-request preservation, and isolated clearing.
- `FurnishedSaveReload`: saves a temporary furnished map under `Saved/HouseFurnishingVerification`,
  unloads it, and verifies a Deep Branches/Hard house, including settings, navigation report/route,
  room/window/furniture data, mesh instances, and labels after reloading.
- `NavigationPatterns`: 240 seeded cases across both patterns and all three presets; exact depth,
  valid route edges, privacy, repeatability and independence from decoration/legacy extra doors.
- `NavigationConstraints`: impossible footprints/depths, 20 custom dead-end-budget cases,
  forbidden private-room through traffic and unplanned shortcuts.
- `HouseholdProfiles`: 150 seeded profile/pattern/difficulty cases, deterministic furniture, bedroom/office
  requirements, category eligibility, vacant houses, room overrides, and infeasible profiles.
- `SizingAndHallways`: 120 seeded compact plans across difficulties, physical bedroom/bathroom
  limits, envelope growth without room inflation, rejection of useless hallways, and invalid ranges.
- `NavigationMetrics`: hand-built graphs verify depth, averages, dead ends, junctions and cycles.
- `CreateExampleMap`: opt-in map creation and save/reload verification. Pass
  `-CreateHouseBuilderExampleMap` in a fresh verification editor. It refuses to overwrite
  an existing `/Game/Procedural/Lvl_HouseBuilder` asset. Normal test runs skip this operation.

Headless collision tests do not establish visual quality, actual character movement, or AI
pathfinding. Check those in Play before treating the house as production gameplay content.

## Next increments

Playtest doorway/corridor feel, add a navigation bounds volume and test actual zombie paths.
Editor-generated houses need a nav build; runtime generation needs suitable runtime navmesh
generation enabled in the level. Playtest navigation presets, then extend authored patterns with
loops, flexible corridor geometry, door offsets and measured walking routes. Introduce authored modular assets, interactive doors,
and furniture-specific interactions and loot.
Stairs, multiple floors, terrain fitting, destruction, and network/save integration are later work.

In Central Hub and Deep Branches, bedrooms try beds before optional furniture. A spare bedroom that cannot fit an eligible bed with
walking clearance becomes an Office and is furnished again using office rules; it must fit a desk
and meet office size limits. Required primary/children's bedrooms fail generation with an actionable
error instead of being removed. Missing or style-filtered bed rules report a catalogue error rather
than triggering an office conversion. Disabled furniture and explicit Unfurnished overrides remain
supported. Original Random Layout retains its legacy best-effort furnishing behavior.
Planned packing reserves extra space around centered doors for primary and child bedrooms;
this can change the footprint for an existing seed. Failed generation preserves the existing actor's house.

The **House Controls** category contains the editable inputs directly. The old read-only
Generated Settings tree is hidden from Details (it remains serialized and available to Blueprints).
Generation never replaces your input Seed, Room Count, dimensions or furniture array. Results show
**Generated Seed**, **Generation Summary**, and **Size Summary** separately.

Generate House uses at least the selected difficulty/household's minimum room count (Normal needs
8, Hard needs 11). It tries up to 16 deterministic seeds when packing or furnishing fails.
**Allow Larger Footprint** defaults on: after four failed attempts inside your requested envelope,
it may expand each axis to at least 24 cells. Disable this option for strict footprint limits.
Room sizing limits, difficulty, and furniture dimensions remain unchanged. The summary explains
any expansion or room-count adjustment. This also handles older maps with 12 x 10 cell inputs.

**Generate House** repeats the same request; **Generate New House** increments the input seed and
generates again. You can also type a Seed and press Enter. To reproduce the exact successful
candidate directly, use Generated Seed and the envelope/room count reported in Generation Summary.
Invalid settings fail immediately; retry exhaustion retains the previous house. Data-only callers
can use GenerateWithRetries; Generate remains a strict single-seed API.
