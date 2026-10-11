# Inventory and controls

Updated: 2026-10-09. Includes movement intent replacement and the crosshair dot.
Read `Source/prototype3/Gameplay/Items/README.md` before changing item actions.

## Crosshair

A small cyan dot with a dark outline marks the center of the gameplay view.
It scales with viewport height and hides while inventory or pause menus are open.

## Options

Pause > Options has **Controls**, **Sound**, and **Graphics**. Sound and Graphics
are placeholders. Controls uses a horizontal scrollable bar: **Movement**, **Item actions**,
**Gameplay**, and **Inventory**. Gameplay contains opening keys; Inventory groups
backpack, item handling, item rotation, and floor-list controls. Rotation speed
appears at the top of the rotation group. Click mode sits directly below item
selection; Allow item replace sits directly below Pick up item.

Hover over an action label or its binding buttons for its usage description.
Obvious single-use controls omit descriptions; controls with multiple usages
explain them. Pockets currently has no description pending its rework.

Every editable action has **two binding buttons**; the second starts **Unbound**. Buttons
show compact key names (LMC, RMC, Esc, L Ctrl), with full names on hover. Either
binding activates the action; releasing one while the other is held does not end
a held action. Item-use and inventory manipulation bindings are independent.
Movement/run/sprint bindings also operate in backpack view and participate in its
binding conflict checks. Existing conflicting saved bindings follow the same
custom-key-first normalization as other controls.

Choose a slot, then press a keyboard key or mouse button. **Delete** clears that
slot; **Escape** cancels capture. Same-context conflicts offer **Replace / Cancel**.
Replace clears only the colliding slots. Keys may be reused in different contexts.
Edits remain a draft until **Apply changes**. Exiting dirty Options offers **Cancel**
(return to editing), **Cancel and exit** (discard), or **Save and exit**.
Explicitly empty slots survive reloading. Esc is fixed and cannot be reassigned.
Hints show current keys, or **Unbound**, including actions with neither slot bound.

**Reset this section** restores that section's keys/preferences. A default used
by another section stays unbound, with an explanation; other sections are preserved.
**Reset all controls** requires confirmation. Movement also has look sensitivity
only. Vertical mouse inversion is removed. Inventory has hold/click dragging and numeric rotation speed.

## Default controls

All listed actions except Esc and floor-list scrolling are rebindable. Floor
scrolling is fixed to wheel up/down, with no rebinding buttons. The input model for run/sprint/crouch remains
the existing combined tap-toggle / hold-until-release behavior. No mode setting.
There is no jumping; Space remains reserved. Delete is reserved for clearing a
binding during assignment.

Run, sprint and crouch use the newest conflicting request. Run replaces sprint,
sprint replaces run, and crouch consumes either gait even if its key is still
held. A gait request consumes crouch and requests standing. Releasing or canceling
a superseded key cannot restore or toggle its old request; use a fresh press.
Ending the newest gait returns to walking, without remembering an earlier gait
or crouch. Tap/hold behavior still uses the existing 0.25-second threshold.

A blocked gait request replaces the previous intent immediately and waits for
actual movement, a permitted direction, stamina and physical standing clearance.
Releasing a hold before it can act ends that request; a short tap remains latched.
Character Movement owns capsule clearance, including when a gait ends under a
low ceiling. Run still permits sideways/backward movement at its existing speed;
sprint still requires forward input.

| Context | Action | Default key |
| --- | --- | --- |
| Gameplay | Move forward / backward / left / right | W / S / A / D |
| Gameplay | Run / sprint / crouch | Left Shift / Left Alt / Left Ctrl |
| Gameplay | Primary / secondary item action | Left / right mouse |
| Gameplay | Take / use bandage | G |
| Gameplay or inventory | Stow held item | X |
| Gameplay | Pick up looked-at floor item | E |
| Gameplay or inventory | Drop held item | P |
| Gameplay or pockets | Open / close pockets | Tab |
| Gameplay or inventory | Open / close backpack | I |
| Backpack open | Next pocket | Tab |
| Inventory | Select / drag / place item | Left mouse |
| Inventory | Rotate item with mouse | Hold right mouse |
| Inventory | Rotate item left / right | Q / E |
| Inventory | Cancel item placement | C |
| Inventory | Drop selected item | F |
| Manipulating an item | Increase / decrease rotation speed | Wheel up / down |
| Over floor list, idle | Scroll floor items up / down | Wheel up / down |
| Gameplay / interface | Pause / close interface / Back | Escape |

The separate pockets and pocket-cycle actions may both use Tab because their
contexts differ. T/Y automatic transfers, H take-in-hands, player-assigned object
slots, V drag-mode toggle, and R backpack-open alias are retired.
The isolated geometry laboratory retains its B add-bandage test control.

## Hands and item use

Primary and secondary execute only the matching item contract slot. A missing
slot does nothing. With empty hands, primary retains the existing punch.
Bandage healing is **secondary**: hold right mouse for **3 seconds** to restore
**25 health**. Release early to stop without consuming or healing.
Most future consumables should follow the same hold/release rule.

Tap a quick-item key to take its fixed item type into hands from any pocket.
Hold it for 0.3 seconds to begin the same secondary use action; keep holding until
completion. Release cancels unfinished use and leaves the item in hands.
Continued holding never consumes another item automatically. Only Bandage has a
quick-item binding. Food/water effects and their bindings are deferred. The former
default 1 migrates to G, or a free letter when G is already assigned.

A valid new conflicting action interrupts healing: switching items or requesting
applicable run/sprint. Direction, movement, stamina and standing clearance still
apply. A blocked gait waits and interrupts healing when it can actually begin.
A failed request, such as a missing quick item, preserves the current use.
Interrupted use needs a fresh press. Walking can coexist with healing.
Full health prevents wasting a bandage.

X stows to a still-valid reserved storage position. Quick-access reservations
survive closing interfaces. A backpack item's reservation lasts only while the
backpack remains open; closing it leaves the item in hands and frees its old
position. A floor item taken directly into hands has no reserved position.
X does nothing when there is no valid reservation. Taking another stored item
stows the previous held item if possible, otherwise drops it before the new
item reaches hands. Opening a cursor interface
or losing viewport focus stops item use and clears held input; it preserves the
item in hands. Interface clicks never also perform gameplay item actions.
Taking a stored item reserves its existing entry; it never creates a second item.

## Inventory interaction

Tab opens all current pockets plus the floor list immediately; that view blocks
movement. The rebindable backpack action (default I) selects two opening modes
using the character's existing run tap/hold threshold (currently 0.25 seconds):

- Tap: quick mode opens 0.5 seconds after the initial press. The character
  crouches and stays still. Movement/run/sprint closes or cancels the backpack,
  restores the previous crouch/standing request, and proceeds through normal
  movement, headroom and stamina rules. Item changes are instant.
- Hold: walking mode opens after 2.5 seconds of stationary charging, including
  recognition time. Actual walking accrues progress at 60% speed (about 4.17
  seconds if continuously walking). Release during charging cancels; release
  after opening keeps the interface open. Run/sprint closes it and proceeds.

Tap/hold recognition has no loading screen: the first visible panel belongs to
the selected quick or walking mode. This adds no extra opening delay.
Walking continues during the initial tap/hold selection window. A short tap
while movement is still held cancels quick opening. Both binding slots form one
held action; release the last held binding to finish that press. Slow mode keeps
the previous stance. The mouse controls the UI; camera look stays blocked.

In walking mode, a valid changed arrangement takes 0.3 seconds: moving between
or within storage, floor pickup/drop, and taking/stowing. Ownership stays at the
source until the delay ends. Source and destination are shaded previews; they
are never extra owned items. Invalid/unchanged placement starts no delay.
Walking stops for the whole transfer. Held walking resumes afterward. Other
inventory input is blocked during the delay; run/sprint and closing can cancel.
Closing, focus loss, interrupting damage, changed ownership, lost floor range,
or a destination that becomes blocked cancels without a partial change. The
source and destination are validated again at commit. Pockets-only operations
remain instant.

The full view shows backpack + one pocket + floor. Tab cycles pockets, including
during a drag. I or Back closes/cancels. Reopening requires the selected delay.

Single grab/select activation selects an item. In default hold mode, moving more
than five logical pixels starts dragging; release places it. Double activation on
the same unmoved, unrotated stored item takes it into hands, leaving inventory open.
Double activation on the held item stows it. This also works with rebound keyboard
keys and with optional click-to-grab/click-to-place mode.

Hold the rotation control and move around the fixed item center; release it to
resume dragging smoothly. Q/E turn continuously. The rotation speed is saved,
15–360 degrees/second, default 120; the default mouse response is 1:1.
Wheel changes speed by 15 while manipulating an item, otherwise scrolls the
hovered floor list. It never does both. Unbind the speed actions and set a numeric
speed in Options to keep a fixed value; floor scrolling remains independent.

Green placement commits immediately or starts the walking-mode arrangement delay;
red placement restores original position/angle.
Rendering, hit testing and collision use the same polygons. Holes remain usable;
edges may touch and every shape part must fit.
C cancels placement while keeping inventory open. Back cancels and closes it.
A held item may be dragged directly: success clears hands/reservation; canceled
or failed placement preserves the original hands/storage state.

## Storage and dropping

Storage placement is manual, including floor pickup. Quick-item bindings only
search pockets; they never pull from a backpack or the floor.
Prototype storage: three 220×220 pockets, each with a 0.60 kg per-item limit and
no firearms; one 420×600 backpack. Values and footprints are provisional.
Future clothing/wearables will determine pocket count/size.

Backpack equipment has no player binding until a full feature exists. The internal
prototype equipment toggle is retained for fixtures. This is one bag, without
world backpack drops, multiple bags or nested containers.
Direct damage interrupts backpack access and closes its interface.

F drops a selected item at the player's feet, including one in hands. P drops
the held item directly during gameplay or inventory, when no placement or
transfer is underway.
The world actor must be ready before ownership transfers. Failed drops preserve
inventory and hands. There is no deletion control. The equipped backpack is not
an inventory entry and cannot be dropped here.

Dragging an item entirely clear of **all displayed storage grids** also drops it.
The floor list and surrounding panel do not block a drop. Its complete rotated
silhouette must clear the grids; the cursor alone is insufficient. Partial overlap
or edge contact cancels the world drop. Hold mode commits on release; click mode
commits on the next grab activation.

Dropped sample items have provisional named colored physics shapes. They fall
onto static ground and ignore pawns/other drops. Instance identity, quantity and
profile survive transfer. Items persist for the Play session; below-world cleanup
defaults to Z=-10,000 cm, configured by `DroppedItem.CleanupZ`.

## Nearby floor items and damage

The floor list refreshes every 0.2 seconds. Default radius is 250 cm from the
player's feet in 3D, configured by `DroppedItem.PickupRadius`; no sight trace yet.
Drag a floor name to reveal its silhouette and place it in a pocket/open backpack.
In the full backpack view, double-click a floor name to take that item directly
into hands. The Tab-only pockets view keeps its existing interactions. A small
Held Item row above the visible pocket shows the name, or Empty. Drag its name
to manually place the item in any accessible storage; double-click it to attempt
the same reserved-position stow as X.
Until placement succeeds, the world actor remains intact. Cancel, invalid fit,
range loss, actor destruction, focus loss or closing the menu never duplicate it.
Dragging the silhouette clear of every storage grid may instead move the same
actor to the player's feet and clear its velocity. During gameplay, look at an
unobstructed floor item within reach and press E to take one item into hands;
holding E has the same single-pickup behavior. The prompt reads "pick up (item name)".
With occupied hands, a current item with a valid stow home returns there. If it
cannot be stowed, replacement drops it only when **Allow item replace when picking
up objects** is enabled in Options > Controls > Item actions (default off).
When blocked, a red notice at the top center points to the setting. Failed or
canceled pickup leaves the held and floor items unchanged. An item placed
manually into quick storage gains that location as a stow home when taken again.

TEST SPIKES are an existing optional prototype hazard: 10 damage/second, stopping
at 1 health. `bSpawnTestSpikes` disables them. Damage normally interrupts healing
and backpack access; `ApplyDamage(Amount, false)` permits future damage-over-time
without interruption. Death always interrupts. No bleeding/poison system is added.

## Code and verification

- `InventoryInputSettings`: canonical controls, contexts, two slots, persistence,
  migration and preferences. `ItemUseSettings` only supplies the fixed quick-item
  catalog and reads legacy preferences for migration.
- `prototype3PlayerController`: runtime Enhanced Input mappings, quick-item
  press/release, focus, pause and menu changes. Saved keyboard/mouse mappings
  replace legacy digital asset mappings; analog/platform mappings are retained.
- `prototype3Character`: matching primary/secondary routing and locomotion
  interruption. Latest conflicting movement requests consume earlier intents;
  requested intent, resolved gait and physical stance remain separate.
- `PlayerItemUseComponent`: hands, reservation, timed use, quick-item delay,
  backpack access and damage interruption.
- `SInventoryPanel`: contextual gestures and manual transactions.
  `SPauseMenu`: sections, key capture, conflicts and reset.
- `InventoryComponent` and `DroppedItem`: placement and ownership transactions.

Run `Scripts/VerifyInventory.ps1 -EngineRoot C:\UE_5.8` for a build and headless
item/inventory code tests. Reports are under `Saved/InventoryVerification`.
Optional `-PlayTest` / `-Capture` enable separate gameplay/visual checks; without
those flags their gated tests report an informational skip, not gameplay evidence.

The suite covers binding persistence/migration/unbinding, menu conflict decisions,
strict item slots, hold/release and damage cancellation, single consumption,
double activation, dual-key release, held-item move/drop rollback, storage admission,
floor scrolling, rotation and geometry. Appearance and gameplay feel require the
user's own playthrough.

The focused `Prototype.Movement.IntentReplacement` check covers replacement in
both directions, tap/hold combinations, consumed releases/cancellations, crouch,
pending direction/movement/stamina requests and low-ceiling clearance. Reports
for this change are under `Saved/MovementVerification`.

Future work stays separate: wearable storage;
animated backpack/pocket selection; food/water and other
item effects; saves/networking. The mixed floor-item test pile is implemented
independently on `floor-item-test-pile` for scrolling and pickup checks.
