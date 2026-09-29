# Inventory test: free placement

Updated: 2026-09-29. This is a temporary inventory laboratory; integration with
character inventory is a later stage.

## Try it in Unreal

1. Open this checkout's `prototype3.uproject`, open `Content/FirstPerson/Lvl_FirstPerson`, and press Play.
2. Press **I** to open the inventory.
3. Hold **left mouse** on a silhouette to pick it up and move it.
4. While holding the item, hold **right mouse**. The item's center stays still;
   move the mouse around that center to rotate it. Rotation starts from the current
   angle without snapping. Moving straight toward/away from the center does not turn it.
5. Release right mouse to resume movement at the retained angle, without a position jump.
   You can switch between movement and rotation repeatedly during a drag.
6. Release left mouse to place, including while right mouse remains held.
   Green confirms; red cancels and restores the original position and angle.
7. Press the **mouse wheel (middle button)** to cancel placement. **Q/E** remain
   alternative continuous rotation controls. Scroll up to increase rotation speed,
   or down to decrease it, including while holding an item.
8. Try fitting a bandage in the L's opening or inside the frame. Edges may touch.

The single pocket is 560 x 560 logical units. The interface scales to the screen.
Positions and angles are continuous, with no grid or angle snapping. There are two
instances of the bandage item, eight additional saved items (canned beans, water
bottle, knife, pistol, flashlight, jacket, small backpack and scrap metal), and
three test silhouettes: L piece, bar and frame. There are 13 entries initially.
Select an item to see its name in the panel. All sample silhouettes and their
dimensions are provisional; the bottle/knife/pistol/jacket shapes are test outlines.
The interface instructions, control labels and status messages are in English.
Item actions are data only: mouse buttons in this interface manipulate placement.
They do not eat, drink, fire, equip or open the backpack item. The test pocket is
independent of that backpack. Restart Play to restore removed samples; B still adds
only bandages. See `Source/prototype3/Gameplay/Items/README.md` for the item catalog.

## Controls and preferences

| Action | Default |
| --- | --- |
| Open / close | I |
| Grab / place | Left mouse button |
| Hold to rotate with mouse movement | Right mouse button |
| Turn left / right | Q / E |
| Increase / decrease rotation speed | Scroll up / down |
| Cancel placement | Mouse wheel press (middle button) |
| Remove selected item | Delete |
| Add bandage | B |

Click a control row, then press the replacement key or mouse button. Duplicate
bindings are rejected. Wheel scrolling is reserved for rotation speed, while
pressing the wheel remains a separate, rebindable cancel button. Use **Cancel
rebinding** to leave without changing a binding, or **Reset controls** to restore
defaults.

Grab supports hold/release or click-to-pick-up/click-to-place. Mouse rotation also
works with click grab and while placing a newly added bandage. Its modifier can be
rebound to a keyboard key. The wheel and on-screen +/- buttons adjust one saved
setting in 15-degree-per-second steps, from 15 to 360 degrees per second for Q/E.
The same setting scales mouse rotation from 0.125x to 3x; its default of 120 gives
the original 1:1 mouse feel. Scrolling changes sensitivity, not the held item's
current angle. Instructions show the current bindings and speed.

Preferences live in local GameUserSettings under
`/Script/prototype3.InventoryInputSettings`. Older seven-action preferences gain
the mouse-rotation action. The old default right-click cancel moves to middle click;
other bindings, grab mode and turn speed are preserved. If a custom binding already
uses a desired button, migration selects a free fallback shown in the controls panel.
Older bindings that used wheel scrolling to turn migrate individually to free keys.
Tests use isolated preferences and restore any temporarily changed runtime settings.

## Placement and rotation

- Rotation is around the center of the rectangle enclosing the whole silhouette.
- Mouse rotation preserves the angle on entry and follows subsequent angular changes.
  Near the exact center (within 3 logical units), direction is undefined: moving away
  establishes a new direction without snapping. Crossing through that region between
  mouse events also re-establishes direction instead of flipping the item.
- Releasing mouse rotation re-anchors the grab offset to resume movement smoothly.
- Rendering, selection and collision use the same silhouette. Concavities and holes
  remain available for other items. Edge/vertex contact is valid; area overlap is not.
- The complete rotated silhouette must fit inside the pocket.
- Preview does not mutate stored items. Invalid drops, cancel, close, loss of focus
  or loss of mouse capture preserve the original placement and angle.
- Shapes are placeholders. Icons do not automatically define collision geometry.

## Technical contract

`UItemDefinition` and `FItemInstance` remain the shared item contract. Inventory
entries hold the instance, pocket, profile, continuous `FVector2D Position` and
`double AngleDegrees`, normalized to [0, 360).

`FInventoryItemProfile.ShapeParts` is a union of filled convex polygons centered
around the item. Decomposition supports concavities and holes. Profiles permit up
to 64 parts with 256 vertices each, with coordinates within +/-4096. Contact tolerance
is 0.0000001 logical units. Registration rejects empty, non-finite, degenerate,
non-centered, concave or self-intersecting parts. Registered profiles and pockets
cannot be replaced; queries return copies.

`AddItem`, `MoveItem`, `CheckPlacement` and `CheckMove` receive center and angle.
Moves preserve identity and quantity and are atomic within one component, including
between its pockets. The demo displays its first and only pocket.

## Files and verification

- `Source/prototype3/Gameplay/Player/Inventory/`: geometry, component and tests.
- `Source/prototype3/UI/Inventory/`: interface, fixtures and saved controls.
- `Source/prototype3/Core/PlayerControllers/prototype3PlayerController`: open/close and gameplay focus.
- `Scripts/VerifyInventory.ps1`: build, automated checks and optional captures/Play tests.

On this PC, run `Scripts/VerifyInventory.ps1 -EngineRoot 'C:\UE_5.8' -Capture -PlayTest`.
Use `-SkipBuild` only with a current build. Tests use a separate verification editor;
do not run them inside a manual Play session. Reports and screenshots are saved in
`Saved/InventoryVerification`.

Historical verification before this change: the previous developer reported a
successful Unreal 5.8.1 Development Editor build and 12 passing tests, including
Slate-routed rotation checks during actual Play frames. One known
`r.MotionVectorSimulation` warning was reported. That is separate from verification
of this mouse-rotation change.

Historical verification of the rotation change on 2026-09-28: Unreal 5.8.1 Development Editor
compilation succeeded (32.21 seconds). All 13 automated tests passed: 12 clean and
one with the same known rendering warning. `MouseRotation` checks scaled geometry,
no initial snap or idle turning, both directions, release order, smooth resumed
movement, pivot crossings, a full turn, middle-click cancel and invalid drops.
`RotationPlayIntegration` exercises 18 scenarios through Slate during actual Play
frames, including Q/E, mouse/keyboard rebindings, click grab, both release orders,
cancel and focus/capture loss. `InputPreferences` verifies persistence and legacy
migration, including a custom binding conflict. The four captures were inspected
for the English interface, control layout and valid/invalid placement previews.
Build log: `Saved/InventoryVerification/Build.log`.
Test report: `Saved/InventoryVerification/Tests/index.json`.
The user's manual assessment of rotation feel remains pending.

Item expansion verified on 2026-09-29: Development Editor build succeeded, and all
16 tests passed (15 clean, one with the previously recorded rendering warning).
The added `IntentValidation`, `SampleAssets` and `SampleIntegration` checks cover
action namespaces, all eight saved definitions with embedded action data, quantity
limits, and each sample's placement, overlap rejection, rotation and identity.
The existing Play input scenarios passed with the expanded 13-item layout.
All four captures were reviewed: samples are visible, invalid overlap is red,
valid placement is green, and the bandage can still fit inside the frame.
Build log: `Saved/ItemVerification/BuildFinal.log`.
Asset creation log: `Saved/ItemVerification/CreateAssets.log`.
The test report above now contains this 16-test run.

Rotation-speed update verified on 2026-09-29: Development Editor build succeeded;
all 16 inventory/item checks passed (15 clean, one with the same engine rendering
warning). Panel and Play tests cover wheel speed adjustment during a drag, Q/E
turning with the new speed, mouse sensitivity, limits and old wheel-binding
migration. No manual feel test was conducted.

## Remaining scope

Demo items survive closing/reopening inventory during Play and reset when Play ends.
This stage does not implement equipment, world pickups, stack splitting, transfers
between components, saved inventories, networking, carried-weight movement effects
or item action execution. Control preferences do persist. This item expansion adds
eight data assets and action intents; the existing bandage and map assets are unchanged.
