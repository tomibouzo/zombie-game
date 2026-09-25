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
Storage fit is contextual: future inventory code will check item shape against a
particular pocket and its compatibility rules. Do not infer the final footprint
from the missing icon or assume every item fits every container.

Run the editor automation group `Prototype.Items` to check the item contract
and reload the bandage asset. This folder does not implement inventory,
equipment, input, UI, healing effects, or world pickups.
