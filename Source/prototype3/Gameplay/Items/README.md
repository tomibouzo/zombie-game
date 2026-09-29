# Item contract

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
mouse-button behavior. `PrimaryAction` and `SecondaryAction` describe left and
right click respectively. The bandage has only a secondary healing action with
`HealAmount = 25`. Targeting and action execution are not implemented here.

The bandage has no final icon, 2D storage shape, or 3D world representation.
The inventory demo supplies a separate provisional polygon profile for it.
Storage fit is contextual: future inventory code will check item shape against a
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

| ItemId / display name | Category suffix under `Item.Category` | Mass kg | Trait | Primary / left intent | Secondary / right intent |
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
for 25 health and no primary action. Neither that healing action nor any new intent
executes yet. All icons and 3D representations remain unset.

### Action data implemented now

`UIntentItemActionData` holds an `ActionTag` under `Item.Action`, using the intent
names in the table. Missing tags, tags outside that hierarchy, and the root tag
itself are invalid. Each action is an inline subobject owned by its definition;
two items of the same type share that configuration. Categories and traits do not
dispatch actions. The left/right assignments for these new samples are provisional.

This class records intent only: it has no execution method, damage, nutrition,
ammo, battery charge, equipment slot, storage capacity, targeting or consumption
rules. Later systems can add appropriate action-specific configuration types under
`UItemActionData`, as the bandage already does with `UHealingItemActionData`.
Changing state such as remaining water or charge belongs to future per-instance
state, not these shared definitions. Runtime execution requires a separate design.

### Inventory integration implemented now

`UI/Inventory/InventoryDemoData.cpp` loads the saved assets and registers their
`FInventoryItemProfile` shapes with `bProvisional = true`. Each sample gets its
own `FItemInstance`; a pistol, jacket and backpack use the same item contract.
Profiles use arbitrary logical test units, not metres or final physical sizes.
Drawing, hit testing and overlap checks use the same existing polygon geometry.
The demo retains two bandages and three geometry fixtures, for 13 entries total.
Selecting a silhouette shows its item name in the existing panel.

The backpack sample is an item placed in the test pocket. It does not create or
own that pocket, and its OpenStorage intent is inert. Equipment, quick-access
pockets, destination compatibility, functional backpacks/holsters/belts, item
execution, crafting, persistence and networking are future work. A handheld trait
does not create a hand model, and an equippable trait does not implement equipping.

### Authoring and verification

Edit the saved assets in Unreal for normal tuning. `Scripts/CreateSampleItems.py`
can bootstrap missing sample assets in a compiled editor using the bundled Python
plugin. It skips existing assets, preserving manual edits, and is never run during
gameplay. The existing bandage is outside that script's creation list.

Run `Scripts/VerifyInventory.ps1 -EngineRoot <engine path> -Capture -PlayTest`
to build and run `Prototype.Items` plus `Prototype.Inventory`. Item tests reload
the saved assets and check IDs, categories, masses, traits, owned action subobjects
and quantity limits. Inventory tests exercise each sample's fit, overlap rejection,
rotation and identity preservation, plus the existing real-frame input scenarios.
The tests document today's provisional values; update expectations deliberately
when those values change.
