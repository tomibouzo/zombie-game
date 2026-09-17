# Player component architecture

## Agreed direction

`Aprototype3Character` remains the player assembler and coordinator. Unreal's
`CharacterMovementComponent` and the existing walk, run, sprint, crouch, camera,
and input behavior remain in place. Gameplay state and item behavior move into
small actor components without introducing a second movement system.

The migration is incremental. Each phase must compile and preserve the previous
playable behavior before the next phase begins.

1. `PlayerVitalsComponent`: owns health, stamina, recovery delay, and exhaustion.
2. Item data and capabilities: defines what an inventory item is and can do.
3. `InventoryComponent`: owns item stacks and quantities.
4. `EquipmentComponent`: selects an inventory item for an equipment slot.
5. `PlayerActionRouterComponent`: routes input to equipped capabilities, with
   `PlayerMeleeComponent` as the unarmed fallback.
6. Bandage vertical slice: obtain, store, equip, use, heal, and consume one unit.

## Item model

Inventory objects use a hybrid data-and-capability model instead of a deep class
hierarchy.

- `ItemDefinition` contains identity, presentation, stack limits, category, and
  capability definitions.
- Categories distinguish families such as firearm, melee weapon, health
  consumable, food consumable, and quest item.
- Capabilities express behavior such as storable, stackable, equippable, heal,
  feed, melee attack, or fire.
- A world pickup is an actor that references an item definition. Picking it up
  converts it to inventory data; dropping it creates a world representation.
- World actors without an inventory item definition or storable capability are
  not accepted by the inventory.

The first bandage is a health-consumable definition with storable, stackable,
usable, and healing capabilities. Item-specific type switches do not belong in
the character, inventory, equipment, or action router.

## Parallel work and file ownership

Three people can work in parallel after agreeing on the small public contracts:

- Vitals owner: `Gameplay/Player/Vitals/**`.
- Item owner: `Gameplay/Items/Data/**`, `Gameplay/Items/Capabilities/**`, and the
  isolated bandage asset folder.
- Inventory/action owner: `Gameplay/Player/Inventory/**`,
  `Gameplay/Player/Equipment/**`, and `Gameplay/Player/Actions/**`.

Only the designated integrator edits shared assembly files such as
`prototype3Character.*`, `prototype3PlayerController.*`, the HUD, module build
configuration, or shared Blueprint classes. Contributors should expose a narrow
API in their owned files and let the integrator connect it.

Avoid moving or renaming shared files during this migration. Each pull request
should represent one phase, include only its owned files plus integration edits,
and record the build and gameplay checks actually performed.

## Git workflow

- `main` remains protected from direct feature work.
- Local feature branches contain one architectural phase each.
- No generated `Binaries`, `Intermediate`, `Saved`, or local IDE files are
  committed.
- Changes are reviewed in source control and tested in Unreal before push.
- Push, pull request creation, and merge require explicit approval.
