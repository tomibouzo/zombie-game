# Player inventory prototype — backpack and quick pockets

Updated: 2026-10-05. Three-pocket layout on `inventory-quick-pockets`, based on
the pause-menu checkpoint `74e3606` on `pause-menu-controls`.

## Pause and Options

**Escape** closes the current interface. From gameplay it opens Pause; from Pause
it resumes; from Options it returns to Pause; from inventory it closes inventory.
Closing inventory during a drag cancels that placement and keeps the original item.
Holding Escape does not repeatedly open/close menus. Escape is reserved and cannot
be rebound. A previous inventory assignment using Escape migrates to a free key,
preserving other saved keys, grab mode and rotation speed.

Pause contains **Resume**, **Options**, **Exit Game**, and **Exit to Desktop**, in
that order. The world and item-use timers pause while Pause or Options is open.
In Play in Editor, **Exit Game** ends Play and **Exit to Desktop** is disabled.
The editor's Stop toolbar button also remains available. The Escape handler is
scoped to this game's focused viewport/interfaces and does not change editor
preferences. In standalone/packaged play, Exit to Desktop quits the game; Exit Game
is disabled until there is a title-menu/session destination.

**Options** contains a scrollable list of inventory and quick-item key assignments,
plus **Reset controls to default**. Select a key and press its replacement; Escape
closes Options without completing an in-progress assignment. Conflicts within the
same input context are rejected. Gameplay quick-item keys may share inventory-only
keys (for example E), but cannot conflict with opening inventory. Reset restores
keys, hold-grab mode and rotation speed, retaining the assigned quick-item types.

Inventory has no buttons, rebinding column, control instructions or shortcut
summaries. It keeps storage/item names and placement/opening feedback. In-inventory
guidance will be reconsidered with the floor-pickup interface. The old buttons
are replaced by these default inventory-only keys (all rebindable in Options):

| Action | Key |
| --- | --- |
| Cycle visible quick pocket (1 → 2 → 3 → 1) | Tab |
| Open backpack (two-second delay) | R |
| Equip / unequip backpack | U |
| Take selected item in hands | H |
| Transfer selected item to the visible quick pocket | T |
| Transfer selected item to backpack | Y |
| Stow held item | X |
| Assign selected quick item to shortcut 1 / 2 / 3 | 1 / 2 / 3 |
| Switch hold / click grab mode | V |

The Add bandage control remains only in the isolated geometry laboratory. The
player inventory no longer exposes it. Rotation speed remains saved and controlled
by the wheel; its numeric indicator and +/- buttons are removed.

The backpack is shown on the left with one quick pocket beside it. The right side
is available for the future floor list. Floor pickup and drag-out dropping belong
to the third feature branch.

## Run this version

Open this checkout's `prototype3.uproject`, load
`/Game/FirstPerson/Lvl_FirstPerson`, and press Play. The prototype is enabled by
the character's ItemUse component.

- **E once** puts the configured quick item in hands. It never consumes it.
- **Double E** cycles the shortcut's item type among available quick items and
  puts that type in hands. Holding the key does not repeat the action.
- **Left click** starts the held bandage's use. Releasing the button does not cancel.
- **Right click** cancels and stows the held item in its reserved place.
- **I** opens inventory by default; an existing customized open key is preserved.
  Pause > Options shows the current key.
- **F** initially selects canned beans; the third shortcut (**G**) starts unassigned.
  Other sample action effects are not implemented in this first bandage slice.

The bandage takes **3 seconds** and restores **25 health**, capped at maximum health.
You may walk while using it; running, sprinting, jumping and melee are blocked.
Actual damage cancels healing and its animation. The bandage stays in hands,
unspent; another left click is required to begin a fresh 3-second use.
Canceling, opening inventory or death stops use without spending the bandage.
Full health prevents use. The effect and consumption happen only at completion.

A white roll is shown in first person with simple movement while using it.
It is positioned by the camera until a holding pose is authored. Other sample objects currently
share this placeholder. These are not finished models or hand animations.
The held object's place stays reserved (shown dimmed) so stowing it cannot fail
or create a duplicate. A reserved item cannot be moved or removed in the panel.

## Quick storage and backpack

The free-placement menu shows the backpack and one of three quick pockets together:

| Setting | Prototype default |
| --- | --- |
| Quick pockets | 3 independent spaces, each 220 x 220 logical units |
| Maximum mass per quick object | 0.60 kg |
| Quick total-weight limit | None |
| Quick firearm rule | Firearms excluded even if small and light |
| Backpack space | 360 x 360 logical units |
| Backpack opening delay | 2 seconds |

All values are provisional. Quick items must pass both the per-object mass rule
and silhouette placement. A light oversized object still fails. Non-firearm
throwables are not excluded by the firearm rule; no grenade action is supplied.

One sample backpack starts equipped in the prototype equipment slot. Opening
inventory starts its two-second opening delay automatically; the quick pocket is
available immediately. **R** can reopen a closed backpack while inventory is open.
Select an item using the existing grab/place controls, then press **H** to take
it in hands. This closes the menu; use still takes the same 3 seconds.
Closing inventory closes the backpack, so reopening it requires its access delay.

**U** unequips the backpack, hides its contents and keeps their identities/placements.
Equip it again to start the opening delay. This prototype models one backpack; it does not
yet implement dropping backpacks into the world, multiple bags or nested bags.

**Tab** cycles quick pockets, including during an item drag. Only the displayed
quick pocket can be clicked. Drag between the backpack and quick pocket, or carry
an item while cycling to place it into another quick pocket. Failed placements,
canceled drags and closing the interface preserve the source item and angle.
Changing pockets clears an idle selection from the hidden pocket. Initial sample
quick items remain in pocket one; pockets two and three start empty.

**T / Y** transfers the selected item to the visible quick pocket / backpack at an available position.
These actions sample candidate positions; ordinary manual placement remains
continuous. Failed transfers preserve the original. The backpack opens first
when transferring into it. **X** stows the held item and frees its reserved place.

Quick shortcuts search all three pockets regardless of the last visible pocket.
They are assigned by item type, not to a particular copy. Select an
item in any quick pocket and press **1 / 2 / 3** to assign the corresponding shortcut.
Change its key in Pause > Options. Duplicate shortcut keys, movement keys, mouse
buttons and the current inventory-open key are rejected. Missing assigned items
do nothing and never silently use an item from the backpack. Settings persist in
GameUserSettings; inventory contents reset at the end of Play.

## Dropping items

In the inventory, select an item and press **Drop** (default **Delete**). You can
drop the selected item without holding it, including in click-to-grab mode. The
former Remove binding becomes Drop, preserving a custom key; there is no item
deletion control. A held item reserved for use must be stowed first. The equipped
backpack is outside the inventory's item entries and cannot be dropped here.

The nine saved sample types have temporary named, colored world shapes. The L,
bar and frame geometry fixtures remain in the isolated laboratory. A dropped
item appears at the player's feet, falls onto static ground, and ignores the
player and other dropped items. It keeps its item identity and profile; the
inventory entry is removed only after the world actor is ready. World items
survive closing the menu but reset at the end of Play. Items falling below
world Z -10,000 cm are removed. Pickup and drag-out dropping are not yet present.

## Safe damage test

A patch of cones labeled **TEST SPIKES** spawns in front/right of the local player
on the floor. Touch it to take 10 damage per second. It stops at 1 health and
cannot kill. Leave the patch, equip a bandage and click to heal. Reenter while
using a bandage to test interruption and manual retry. The ItemUse component can disable the
prototype hazard with `bSpawnTestSpikes`.

## Preserved manipulation controls

Hold left mouse to drag (or use your saved click-toggle mode). Hold right mouse
while moving the cursor around the item to rotate, then release right mouse to
resume dragging smoothly. Q/E also turn. The wheel changes the
shared rotation speed in steps of 15, from 15 to 360 degrees/s. At the default
120, mouse rotation is 1:1. Pressing the wheel cancels placement. Controls are
rebindable in Pause > Options. Gameplay shortcuts only apply with this
menu closed, so E can also remain a panel rotation binding.

Green placements commit; red placements restore the original angle and position.
Rendering, hit testing and collision share the same polygon silhouettes. Holes
remain usable, edges may touch, and every part must fit inside its destination.

## Implementation and verification

- `Gameplay/Player/Inventory/InventoryComponent`: placement, pocket admission,
  reservation, consumption and identity-preserving movement.
- `Gameplay/Player/Inventory/PlayerItemUseComponent`: prototype population,
  hands, quick shortcuts, backpack access, use timer and damage interruption.
- `Gameplay/Player/Inventory/ItemUseSettings`: saved shortcut types and keys.
- `Gameplay/Player/Inventory/InventoryTestSpikes`: nonlethal test hazard.
- `UHealingItemActionData`: existing healing amount plus configurable UseSeconds.
- `Core/Characters/prototype3Character`: assembly, held-item input routing and
  movement restriction. Primary input uses healing data from either action slot;
  the saved bandage definition retains the secondary-slot data supplied earlier.
- `UI/Inventory`: existing menu adapted to player-owned storage. The old 560x560
  laboratory is retained for independent geometry/rotation regression tests.
- `UI/Pause/SPauseMenu`: pause actions and saved key assignment screen.
- `Core/PlayerControllers/prototype3PlayerController`: interface focus, pausing,
  scoped Escape handling and editor-safe exit actions.
- `Gameplay/Items/World/DroppedItem`: temporary world shapes, transfer and cleanup.

Run `Scripts/VerifyInventory.ps1 -Capture -PlayTest` in this checkout. It builds
Unreal 5.8.1 and runs the item/inventory suite, including real Play input and UI
routing. The new checks cover quick admission, reservations, timing, damage,
consumption, bag access, equipment identity, shortcuts and nonlethal damage.
Reports and images are under `Saved/InventoryVerification`. Automated Play checks
are separate from the user's manual assessment of appearance and feel.
The script expects at least 23 checks, including pause-menu defaults and the
three-pocket lifecycle and panel interaction checks. The pocket checks use an
isolated component world and synthetic Slate input; they do not assess gameplay
appearance or feel.

Food, water, radiation treatment, firearms, grenade throwing, durable equipment,
world pickups, saved games and networking remain later slices. Their sample tags
are configuration only. Existing sample masses and polygon sizes are provisional.
