# Player inventory prototype — hands and bandage use

Updated: 2026-10-05. Combines player item use with world dropping.

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
  The controls column shows the current key.
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

The same free-placement menu now has two storage views:

| Setting | Prototype default |
| --- | --- |
| Quick space | 220 x 220 logical units |
| Maximum mass per quick object | 0.60 kg |
| Quick total-weight limit | None |
| Quick firearm rule | Firearms excluded even if small and light |
| Backpack space | 360 x 360 logical units |
| Backpack opening delay | 2 seconds |

All values are provisional. Quick items must pass both the per-object mass rule
and silhouette placement. A light oversized object still fails. Non-firearm
throwables are not excluded by the firearm rule; no grenade action is supplied.

One sample backpack starts equipped in the prototype equipment slot. Click
**Open backpack** and wait for opening to finish before accessing its contents.
Select an item using the existing grab/place controls, finish placing it, then
click **To hands**. This closes the menu; use still takes the same 3 seconds.
Closing inventory closes the backpack, so reopening it requires its access delay.

**Unequip backpack** hides its contents and keeps their identities/placements.
Equip it again to regain access. This prototype models one backpack; it does not
yet implement dropping backpacks into the world, multiple bags or nested bags.

**To quick / To backpack** transfers the selected item to an available position.
These buttons sample candidate positions; ordinary manual placement remains
continuous. Failed transfers preserve the original. The backpack opens first
when transferring into it. **Stow** frees the reserved held item.

Quick shortcuts are assigned by item type, not to a particular copy. Select an
item in quick storage and choose **Assign selected** on a shortcut row. Click its
key to rebind; Escape cancels. Duplicate shortcut keys, movement keys, mouse
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
resume dragging smoothly. Q/E also turn. The wheel and +/- controls change the
shared rotation speed in steps of 15, from 15 to 360 degrees/s. At the default
120, mouse rotation is 1:1. Pressing the wheel cancels placement. Controls are
rebindable in the right-hand column. Gameplay shortcuts only apply with this
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
- `Gameplay/Items/World/DroppedItem`: temporary world shapes, transfer and cleanup.

Run `Scripts/VerifyInventory.ps1 -Capture -PlayTest` in this checkout. It builds
Unreal 5.8.1 and runs the item/inventory suite, including real Play input and UI
routing. The new checks cover quick admission, reservations, timing, damage,
consumption, bag access, equipment identity, shortcuts and nonlethal damage.
Reports and images are under `Saved/InventoryVerification`. Automated Play checks
are separate from the user's manual assessment of appearance and feel.
The script expects 19 checks from the combined branches.

Food, water, radiation treatment, firearms, grenade throwing, durable equipment,
world pickups, saved games and networking remain later slices. Their sample tags
are configuration only. Existing sample masses and polygon sizes are provisional.
