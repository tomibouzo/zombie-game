# Item contract

## Item creation and input guide — 2026-10-06

Source: agreed controls plan sections 1-6, including the section 5 scope correction.
Controls now follow these rules. New action effects still require their own runtime handlers.

- Author stable type data in `UItemDefinition` and mutable owned state in
  `FItemInstance`. Preserve instance identity and inventory reservations when moving
  items. Categories/traits describe items; action data selects their behavior.
- `PrimaryAction` and `SecondaryAction` are semantic slots. Route each input only
  to its matching slot; missing actions do nothing. Never search the other slot
  for a convenient action. Empty hands use the separate unarmed behavior.
- Physical keys belong to player controls settings. Default primary/secondary to
  LMB/RMB, but allow keyboard or mouse rebinding for item actions. Esc is a fixed interface control. Labels and hints use UInventoryInputSettings::KeyLabel, including both keys or Unbound.
- Players assign keys to developer-defined actions. Named quick-item identities
  remain fixed; do not restore player-assigned object/shortcut slots. Actions with
  no assigned key remain in gameplay hints as Unbound (e.g. Hold Unbound to use).
- Expose bindings only when their feature exists. Bandage is currently the only
  quick-item binding (default G); food/water and backpack equipment bindings are
  deferred. Options edits are drafts until Apply or Save and exit.
- Name each control's action and target clearly: Rotate item left/right, Cancel
  item placement, Drop selected item. Avoid labels such as Turn left/right that
  could describe camera or character movement. Follow this for future bindings.
- Every editable control has two bindings; Binding 1 starts with its default and
  Binding 2 starts empty. This includes inventory grab/rotate. Binding slots are
  independent of the item's PrimaryAction/SecondaryAction slots. Double activation
  of inventory grab/select takes the item into hands and leaves inventory open;
  taking an item never executes its use action.
- Double activation of the item already in hands stows it. Inventory movement or
  dropping may target held items directly: success removes the item from hands;
  cancellation/failure preserves the original hands/storage state.
- Storage placement is manual, including floor-to-storage pickup. T/Y automatic transfers are removed. Taking a stored item into hands
  reserves its original place; stowing restores that place. Direct floor-to-hands
  pickup and carrying without a reserved storage home are deferred.
- Controls supports clearing a selected binding slot with Delete. Preserve
  intentionally unbound slots when saving/loading; do not treat them as corrupt
  settings or silently repopulate them with defaults.
- Define activation semantics per action: press, hold and release. Most consumables,
  including the bandage, require holding through their use duration; release stops
  unfinished use. Aim is held; a flashlight toggle is a press. Explicit exceptions
  belong in the action contract, never in item-name or category special cases.
- Timed consumables apply their completed effect/consumption once. Releasing early
  leaves unfinished use uncommitted. Fixed quick-item tap equips; holding invokes
  that item's same use action and releasing stops unfinished use. Do not duplicate
  effect logic in shortcut handlers or restart completed use automatically.
- Put away (default X) is a separately rebindable stow action.
- Define action compatibility and interruption. A new applicable action stops
  conflicting actions and starts: run/sprint or switching items can interrupt
  healing. Compatible actions can coexist. Release also stops unfinished held use.
  Physical/stamina requirements still apply; interruption does not bypass them.
  Validate the new request before canceling the old action; a missing quick item
  must not interrupt healing. Interrupted use needs a fresh press to restart; a
  still-held input cannot restart it. Unfinished consumption/effects stay uncommitted.
- Follow the locomotion architecture's separation of input intent, validated
  transitions and resolved state. Keep compatibility/interruption decisions
  explicit instead of spreading input-specific priority checks across handlers.
  The separate future run/sprint intent-replacement change is outside this
  inventory pass; preserve locomotion's current combined tap/hold behavior.
- Cursor interfaces suspend gameplay item actions. Interface mouse interactions
  are independent of gameplay bindings; clicking UI must never also fire/use.
- Future pocket count/size comes from clothing/wearables, and backpack storage
  from the backpack. Three equal quick pockets are prototype data. The later
  animated backpack/pocket-selection flow is deferred; preserve today's access
  flow. Backpack access now supports quick stationary crouching and slow walking;
  pockets-only view still blocks movement. See the root inventory guide.

- Walking backpack mode queues valid arrangements for 0.3 seconds. Keep source
  ownership and held reservations intact until commit; validate source and
  destination again. Closing, damage or a conflicting gait cancels the pending
  operation without a partial move. The panel's shades are presentation only.

When adding an item, specify its slots, activation/completion rules and required
runtime handler. An intent tag alone does not implement the behavior. Focused
verification should cover correct/missing slots, remapped input, early release,
single completion, conflicting-action interruption, quick-use parity and UI
isolation where applicable. Current code tests cover bandage/item slots, early release, interrupted use, ownership transactions and key settings. Gameplay/visual validation is separate.

## Current data and prototype behavior

`UItemDefinition` is shared data for an item type. `DA_Item_Bandage` lives at
`/Game/Items/DA_Item_Bandage`. Runtime inventory code should keep a reference to
the definition, not copy its name, mass, category, or action data into each entry.
`ItemId` is the stable gameplay identifier; the asset filename can change.

`FItemInstance` is one owned item. Create it with `FItemInstance::Create`, which
generates its `InstanceId` and rejects invalid definitions or quantities. A full
move can preserve that ID by moving the struct. Its `Quantity` must remain between
one and the definition's `MaxStackSize`; all current definitions use a maximum of
one. Pocket location and rotation belong to the inventory, not this struct.

`MassKg` is the mass of one unit. The bandage currently uses 0.05 kg (50 g), a
provisional balance value. `Category` identifies the item family, while `Traits`
hold independent facts such as `Item.Trait.Handheld`. Neither field determines
action-slot behavior. `PrimaryAction` and `SecondaryAction` hold action data.
The bandage retains secondary healing data with
`HealAmount = 25` and `UseSeconds = 3`. The player routes primary and secondary independently; holding secondary executes this bandage contract. Release cancels unfinished use. Quick use calls the same action. X stows. Action execution lives in PlayerItemUseComponent; this data layer does not execute actions.

The bandage has no final icon, 2D storage shape, or 3D world representation.
The inventory demo supplies a separate provisional polygon profile for it.
Storage fit is contextual: inventory code checks item shape against a
particular pocket and its compatibility rules. Do not infer the final footprint
from the missing icon or assume every item fits every container.

Run the editor automation group `Prototype.Items` to check the item contract
and reload the bandage asset. This folder does not implement inventory,
equipment, input, UI, healing effects, or world pickups.

## Sample catalog (2026-09-29)

Eight additional saved `UItemDefinition` assets live in `/Game/Items`, named
`DA_Item_<ItemId>`. Every sample has `MaxStackSize = 1`. These masses are provisional
test values per complete item, including contents where relevant; they do not
establish a contents simulation or authoritative real-world specifications.

| ItemId / display name | Category suffix under `Item.Category` | Mass kg | Trait | Primary intent | Secondary intent |
| --- | --- | --- | --- | --- | --- |
| CannedBeans / Canned beans | Consumable.Food | 0.45 | Handheld | — | Eat |
| WaterBottle / Water bottle | Consumable.Drink | 0.55 | Handheld | — | Drink |
| Knife / Knife | Weapon.Melee | 0.20 | Handheld | MeleeAttack | — |
| Pistol / Pistol | Weapon.Firearm | 0.90 | Handheld | Fire | Aim |
| Flashlight / Flashlight | Tool.Lighting | 0.15 | Handheld | — | ToggleLight |
| Jacket / Jacket | Equipment.Clothing | 0.80 | Equippable | — | Equip |
| SmallBackpack / Small backpack | Equipment.Backpack | 0.70 | Equippable | OpenStorage | Equip |
| ScrapMetal / Scrap metal | Material.Salvage | 0.25 | — | — | — |

The existing bandage remains 0.05 kg, medical/handheld, with secondary healing data
for 25 health and no primary action. The player item-use prototype executes healing
after its use timer. New intent tags still have no gameplay effect. Shared
definitions have no assigned icons or world meshes; temporary world-drop shapes
are generated separately.

### Action data implemented now

`UIntentItemActionData` holds an `ActionTag` under `Item.Action`, using the intent
names in the table. Missing tags, tags outside that hierarchy, and the root tag
itself are invalid. Each action is an inline subobject owned by its definition;
two items of the same type share that configuration. Categories and traits do not
dispatch actions. The primary/secondary assignments for these new samples are provisional.

This class records intent only: it has no execution method, damage, nutrition,
ammo, battery charge, equipment slot, storage capacity, targeting or consumption
rules. Later systems can add appropriate action-specific configuration types under
`UItemActionData`, as the bandage already does with `UHealingItemActionData`.
Changing state such as remaining water or charge belongs to future per-instance
state, not these shared definitions. Runtime execution requires a separate design.

### Isolated inventory laboratory

`UI/Inventory/InventoryDemoData.cpp` loads the saved assets and registers their
`FInventoryItemProfile` shapes with `bProvisional = true`. Each sample gets its
own `FItemInstance`; a pistol, jacket and backpack use the same item contract.
Profiles use arbitrary logical test units, not metres or final physical sizes.
Drawing, hit testing and overlap checks use the same existing polygon geometry.
The demo retains two bandages and three geometry fixtures, for 13 entries total.
Selecting a silhouette shows its item name in the existing panel.

In this isolated laboratory the backpack sample is an item in the test pocket.
Its OpenStorage tag is inert; a handheld or equippable trait alone adds no behavior.

### Player prototype (2026-09-30)

PlayerItemUseComponent reuses this catalog with separate quick/backpack pockets,
one equipped backpack instance, reserved hands, fixed quick items with configurable keys and bandage
consumption. Quick storage checks silhouette, per-object mass and firearm exclusion.
The backpack requires an opening delay before access; its contents survive
unequipping/re-equipping. This is one prototype bag, not a generic nested-container
system. See the root INVENTORY_GUIDE.md for current controls and limitations.
Crafting, persistent inventories, other item effects and networking remain future work.

### Authoring and verification

Edit the saved assets in Unreal for normal tuning. `Scripts/CreateSampleItems.py`
can bootstrap missing sample assets in a compiled editor using the bundled Python
plugin. It skips existing assets, preserving manual edits, and is never run during
gameplay. The existing bandage is outside that script's creation list.

Run `Scripts/VerifyInventory.ps1 -EngineRoot <engine path>`
to build and run `Prototype.Items` plus `Prototype.Inventory`. Item tests reload
the saved assets and check IDs, categories, masses, traits, owned action subobjects
and quantity limits. Inventory tests exercise each sample's fit, overlap rejection,
rotation and identity preservation, with optional real-frame input scenarios enabled by -PlayTest (and visual capture by -Capture).
The tests document today's provisional values; update expectations deliberately
when those values change.

## Temporary world drops (2026-10-01)

`World/DroppedItem` provides named, primitive 3D stand-ins for the nine saved item
types. Pistol and flashlight parts are rotated to lie on their sides. These are provisional centimetre dimensions, separate from inventory shape
profiles and final art. The inventory's three geometry fixtures have no world form.
The mapped Drop control (default F) transfers the selected inventory item (also while held) to a
physics actor at the player's feet, preserving its instance, quantity and profile
ID. The world actor must be ready before the inventory removes the source entry.

Drops fall under gravity, block static level geometry, and ignore pawns, other
dropped items and movable props. Below-world cleanup defaults to Z=-10,000 cm;
`CleanupZ` is configurable in the `prototype3.DroppedItem` Game config section.
There is no expiration timer on landed items. World actors last for the Play
session. Manual floor-to-storage pickup and drag-out dropping are implemented; direct floor-to-hands remains deferred.
`Prototype.Inventory.DropPlayIntegration` verifies actual input, transfer, falling,
landing, overlap and cleanup in Play. See `INVENTORY_GUIDE.md` for running it.
