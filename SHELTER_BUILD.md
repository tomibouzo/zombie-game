# Shelter building

Placement half of the shelter construction prototype: a build grid, build mode, a preview,
placement rules and removal. Interactive pieces (doors that open, usable objects), the shelter
level and final piece content are a separate contribution and are not part of this code.

## Try it

Open `Content/Shelter/Lvl_BuildLab` and press Play. You start just outside a 6 x 6 grid of
200 cm modules. Build mode only works inside a build area (and up to 200 cm outside its edge,
so outer walls can be built from outside); elsewhere the build key does nothing.

## Controls (defaults; rebind in Options > Controls > Building)

| Key | In the category menu | While placing |
| --- | --- | --- |
| `B` | opens build mode from gameplay | back to the category menu |
| `Q` / `E` | previous / next category | previous / next piece |
| `1`-`9` | pick that category | — |
| Left mouse (Primary) | choose the category | place the piece |
| Right mouse (Secondary) | — | remove the piece you look at |
| `R` | — | rotate (walls flip; cell objects quarter-turn) |
| `V`, `Esc` | leave build mode | leave build mode |

Build mode also closes when you open the backpack or pockets, pause, take damage, die, reach the
house exit prompt, or walk out of the build area. It is an overlay: there is no mouse cursor and
you keep walking and looking. A held item stays in your hands; its actions, pickup, drop, stow and
the bandage shortcut are suspended until you leave. Because `E` cycles pieces while building,
Interact / pickup is unavailable until you leave build mode.

## Rules

- A slot is one edge between two grid cells. It holds exactly one piece. Placing on an occupied
  slot is refused; remove the piece first.
- A placement is valid when the slot is inside the grid, empty, within reach (450 cm on the floor)
  and no character stands in it. The preview is green when valid and red otherwise, and the panel
  says why.
- Nothing is saved, nothing costs resources and there is no structural support. The grid starts
  empty every session.

## Adding a piece

Create a **Shelter Piece Definition** data asset under `Content/Shelter/Pieces`. No C++ change is
needed: pieces are discovered by asset type (`ShelterPiece` in `Config/DefaultGame.ini`) and are
always cooked.

| Field | Meaning |
| --- | --- |
| Piece Id | Stable identity stored in the grid. Do not change it when renaming the asset. |
| Display Name, Category | Menu text. Pieces sharing a Category are cycled together. |
| Sort Order | Lower first, for categories (by their lowest piece) and within a category. |
| Slot Kind | `Edge` for wall-sized pieces. `Cell` for one object on a floor cell. |
| Actor Class | Actor spawned in the slot. Empty uses the grey placeholder. |
| Opening Width / Height / Sill | Placeholder only: a centred opening in the wall panel. Zero is solid. |
| Cell Object Size | Placeholder only: box size for a `Cell` piece. |

For a piece with behaviour, derive an actor from `AShelterPiece`, set it as Actor Class, and
override `InitializePiece` (called once before spawning finishes, with the definition and module
size) and, if it has its own look, `SetPreview`. Local frame: origin at the slot centre on the
floor, X along the edge, Z up. Placement code never asks what a piece does.

## Adding a build area to a level

Place a **Shelter Build Area** actor. Its origin is the grid's minimum corner on the floor and its
X/Y axes are the grid's. Set Size Cells and, if needed, the module (width 200, height 300, wall
thickness 20 cm). The yellow box in the editor is only an outline.

## Code map (`Source/prototype3/Gameplay/Shelter/Build`)

| File | Role |
| --- | --- |
| `ShelterBuildModel.*` | Data-only grid: slots, place, remove, validate, nearest slot. No world or actors. |
| `ShelterPiece.*` | Piece definition asset, `AShelterPiece` base and the placeholder actor. |
| `ShelterBuildArea.*` | The grid in a level: world transforms, the blocked check, piece actors. |
| `ShelterBuildComponent.*` | Build mode on the player controller: states, keys, preview, exits. |

The player controller has one hook, at the top of `InputKey`, that offers every key to build mode
before its own routing. The on-screen panel is drawn by `AHorrorHUD::DrawBuildOverlay`.
New controls were appended to the end of `EInventoryControl`; saved bindings are stored by
position, so never insert above them.

## Checks

```
Scripts\VerifyShelter.ps1 -Regression
```

Runs `Prototype.Shelter.*` (slot model, controls, area placement, build-mode input, piece catalog)
and the existing input, pickup, pause-menu and movement checks that share files with build mode.
`-CreateFixtures` creates the three placeholder piece assets and `Lvl_BuildLab` once; it never
overwrites existing files.

Not covered by automated checks: how the preview and panel look, placement feel, walking through a
placed doorway, and build mode combined with a real pause menu or inventory window.
