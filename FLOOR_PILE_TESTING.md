# Mixed floor item test pile

Start Play in `Lvl_FirstPerson`. A loose pile of 36 existing sample items appears
about 1.2 m ahead and 0.7 m left of the player's starting position, within pickup
range. It is created once per Play session; restarting Play restores the pile.

Open pockets with Tab or the backpack with I. Hover the floor list and use the
mouse wheel to reach the remaining rows, then drag an item into available storage.
Duplicate names are separate items. The wheel changes rotation speed while an
item is being dragged, as before. Direct pickup into hands is a separate future
feature.

## Setup decisions

- Use all nine sample item types at least once, then randomize the remaining
  types, positions and facing. Reuse the existing colored physical placeholders.
- Use seed 7307 for repeatable setup. Runtime physics can vary the final pose.
- Keep the player's starting items intact. Every pile object has its own identity
  and uses the normal world-item pickup and drop path.
- Only the first-person test map creates this fixture automatically. Disable
  **Spawn Inventory Test Pile** in its game mode defaults to turn it off.
- `AInventoryTestPile` exposes `ItemCount` (17–128) and `RandomSeed`. They can also
  be configured in `DefaultGame.ini` under `[/Script/prototype3.InventoryTestPile]`.
- Require nearby static ground. Missing ground/assets produce a log warning
  instead of spawning an endless replacement stream.

The pile is a temporary test fixture, with no saved-world persistence or new
loot/pickup controls.
