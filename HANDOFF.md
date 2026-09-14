# Project handoff

Updated: 2026-09-14

## Project and current state

- Unreal Engine 5.8 C++ project: `prototype3.uproject`.
- Local project: `C:\Users\tomib\Documents\Unreal Projects\prototype3`.
- Engine location used for the last successful build: `C:\UE_5.8`.
- Configured editor/startup game map: `/Game/FirstPerson/Lvl_FirstPerson`.
- Default game mode: `/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode`.
- Latest commit observed: `9dc1d59` (`organized files. adjusted values, created a new function`).
- At task start, only `AGENTS.md` and `HANDOFF.md` were untracked. Crouch work now
  modifies the shared character/controller source; no commit has been made.
- The user approved implementing crouch on 2026-09-14.
- The user then requested fixing crouched ledge movement, combined tap/hold Ctrl
  input, and 1.5x stamina recovery while crouched. These changes are implemented.
- Milestone requirements and remaining work are saved in `MILESTONE_1.md`.
  Keep its implementation checkboxes and separate validation checklist current
  as each feature is completed; the user requested ongoing tracking.
- Current approved work: unarmed left-click attacks and reserved right-click
  action input. The user clarified that the earlier no-compilation request applied
  only to the drain adjustment; normal compilation is authorized for this work.
- Latest request authorized compilation and checking before the user pushes,
  and saving changes. The build retry succeeded; files are saved on disk.

## Implemented movement and stamina

The user chose walking, running on Shift, and sprinting on Alt. Both running and
sprinting use stamina; sprinting is faster and consumes more. We kept the existing
sprint system and added running, rather than renaming sprint.

Current C++ defaults (Blueprint overrides may differ):

| Variable | Value | Meaning |
| --- | --- | --- |
| `MaxHealth` | 100 | Starting and maximum health |
| `MaxStamina` | 100 | Starting and maximum stamina |
| `WalkSpeed` | 350 | Walking speed, cm/s |
| `RunSpeed` | 500 | Running speed, cm/s |
| `SprintSpeed` | 700 | Sprinting speed, cm/s |
| `RunStaminaDrainPerSecond` | 2 | Running stamina drain per second |
| `StaminaDrainPerSecond` | 4 | Sprinting stamina drain per second |
| `StaminaRecoveryPerSecond` | 50 | Recovery while neither running nor sprinting |

- Left/right Shift run; left/right Alt sprint. Alt takes priority if both are held.
- Releasing Alt returns to running if Shift is still held and stamina permits it.
- Drain requires actual horizontal movement. Standing still permits recovery.
- Empty stamina locks both running and sprinting until full recovery; movement falls back to walking.
- `CurrentHealth` and `CurrentStamina` initialize from their maxima at BeginPlay.
- The character writes Character Movement's `MaxWalkSpeed` every tick. Tune the
  character's speed variables rather than only the component's default speed.
- In `BP_FirstPersonCharacter` Class Defaults, the new run settings are under
  `Run`; existing walking/sprinting and drain/recovery settings are under `Sprint`.

## Relevant code and architecture

- `Source/prototype3/Core/Characters/prototype3Character.h` and `.cpp`: health,
  movement speeds, run/sprint input handlers, stamina update and exhaustion logic.
- `Source/prototype3/Core/PlayerControllers/prototype3PlayerController.h` and `.cpp`:
  runtime Enhanced Input mappings; controller-owned run action exposed through
  `GetRunAction()`. Existing `/Game/Input/Actions/IA_Sprint` maps to Alt. Shift run
  mappings use priority 1 to consume older lower-priority Shift sprint mappings.
- No new run `.uasset` was needed. Run bindings handle Started, Completed, Canceled.
- `Source/prototype3/Legacy/README.md` describes retired prototype modes. Do not
  add new gameplay dependencies to legacy classes. Reusable behavior belongs in
  `Gameplay`, map startup in `Core`, presentation in `UI`.
- Legacy Horror has a separate sprint implementation (`SprintTime`, `SprintMeter`);
  Shooter has separate health (`MaxHP`). The current movement work targets the
  shared first-person character/controller, not a unification of legacy systems.

## Crouch implementation (2026-09-14)

- Either Ctrl supports both tap and hold: press crouches immediately; a release
  before `CrouchHoldThreshold` (default 0.25 seconds) from standing leaves crouch
  toggled on. Tap again to stand on release. Holding for at least the threshold
  stands on release. Holding from an already toggled crouch also stands on release.
  Timing uses elapsed real time. Canceled input requests standing without toggling.
- Unreal Character Movement handles capsule resizing and refuses to stand until headroom is clear,
  then automatically retries while the player moves out from under the obstacle.
- `BP_FirstPersonCharacter` Class Defaults > `Crouch`: `CrouchSpeed` defaults to
  150 cm/s; `CrouchHalfHeight` defaults to 60 cm (120 cm total capsule height).
  Half height is applied at BeginPlay and clamped between capsule radius and
  standing half height. Speed is applied each tick.
- Crouch overrides both run and sprint and allows stamina recovery,
  including while moving or blocked from standing. Held Shift/Alt resumes its
  normal behavior after standing, subject to the existing exhaustion lock.
- `CrouchStaminaRecoveryMultiplier` defaults to 1.5 (75 stamina/second with the
  default base recovery of 50), applied to actual crouched state. Both the
  multiplier and tap/hold threshold are editable in Class Defaults > `Crouch`.
  Stamina updates now broadcast the final recovery step to full as well.
- `bCanWalkOffLedgesWhenCrouching` is enabled in the constructor and BeginPlay
  to fix the reported sticking at ledges and override old Blueprint defaults.
  This allows walking/falling off edges; `JumpMaxCount = 0` remains unchanged.
- Controller owns `RuntimeCrouchAction`, exposes `GetCrouchAction()`, and maps
  both Ctrl keys in the existing priority-1 runtime context. No new asset needed.
- Character exposes `DoStartCrouch`/`DoEndCrouch` as explicit Blueprint requests.
  Keyboard Started/Completed/Canceled events use separate `CrouchInput*`
  handlers to interpret taps and holds.
- The head-mounted camera follows the first-person mesh. Crouch callbacks offset
  that mesh to cancel the parent body's height compensation and lower the view
  with the capsule top; standing restores the position captured at BeginPlay.
- View height changes immediately; smooth transitions and crouch animation
  assets have not been added. The user tested the first crouch version and
  reported the ledge issue; the revised behavior needs Play-mode validation.

## Build and validation

- Pre-push review (2026-09-14): user reports attacking works after restarting
  the computer. This confirms recovery of the reported input symptom, not every
  gameplay checklist item or the exact cause. Reviewed all pending character,
  controller, and new melee source; no push-blocking issue found. Fresh
  `git diff --check` passed. No source edits or new build during this review;
  the previously successful `-0009.dll` build postdates all six gameplay files.
  Include both untracked `PlayerMeleeComponent` source files in the commit.
  Git's system `core.autocrlf=true` explains the LF-to-CRLF warning; no Git
  settings or line endings were changed.

- Attack regression follow-up (2026-09-14): user reports LMB does nothing,
  including no stamina consumption, after compilation. User will perform gameplay
  validation. Changed controller primary/secondary actions from constructor default
  subobjects to lazy runtime transient objects shared by mappings and character
  bindings. Input setup now clears/rebuilds the runtime context instead of retaining
  its old mappings. This addresses suspected stale action/default state after hot
  reload; the exact runtime cause and gameplay recovery are not yet confirmed.
  Run/crouch actions and melee timing/damage/stamina logic are unchanged by this fix.
  Fresh validation: `prototype3Editor Win64 Development` succeeded in 164 seconds,
  producing `UnrealEditor-prototype3-0009.dll`; restricted attempt failed without
  diagnostics, approved retry succeeded. `git diff --check` passed. No gameplay
  test was performed; user should verify tap, hold, release, and stamina spending.

- The drain adjustment on 2026-09-14 doubled running drain from 1 to 2
  and sprinting drain from 2 to 4 stamina/second. The user requested
  no compilation for that adjustment alone, not as an ongoing preference.
  Recovery rates are unchanged. The user subsequently supplied milestone 1 and
  chose unarmed melee as the next implementation.

- Melee work: added `Gameplay/Combat/Melee/PlayerMeleeComponent.h`
  and `.cpp`, attached as the character's `Melee` component. Defaults are damage
  10, stamina cost 10, interval 0.525 s, hit delay 0.175 s, reach 150 cm, radius 12 cm.
  Interval/delay were shortened by 12.5% from 0.6/0.2 s. Animation playback scales
  with the interval. LMB starts repeating punches; RMB is mapped but does nothing. Both actions
  have overridable start/end handlers for future held-item behavior. No item
  types, equipment, interaction, aiming, or blocking implementation was added.
- `StartAttacking` enables melee component ticking and attempts an immediate
  swing. Held ticking retries through `TryAttack` and its cooldown/stamina guards.
  `StopAttacking` disables ticking on release/canceled input; pending hits finish.
  Insufficient stamina permits recovery and retry while held. Death/loss of
  possession stops repetition; EndPlay disables ticking and clears the hit timer.
- Melee spends stamina on accepted swings (including misses), pauses recovery
  for the interval, and blocks attacks when dead, too soon, or short of stamina.
  Hit checks use the current camera at impact time, ignore self, stop at the
  first blocker, and apply Unreal point damage. The Camera collision channel is
  used because stock Pawn collision ignores Visibility. Targets must block that
  channel and implement damage/health handling. Authority-only; no client RPCs.
- Existing `MM_Attack_01` AnimSequence is loaded and played in the body's
  `DefaultSlot`; the first-person mesh copies the body pose. Montage-instance
  root motion is disabled to retain normal movement. Component events expose
  attack start and blocking impact for future feedback.
- Initial melee validation on 2026-09-14: `prototype3Editor Win64 Development`
  succeeded in about 114 seconds, including header generation, melee component,
  character/controller compilation, and the doubled movement drain defaults.
  The restricted attempt exited without diagnostics; the approved unrestricted
  retry succeeded. `git diff --check` passed.
- The editor loaded `UnrealEditor-prototype3-0005.dll` and `MM_Attack_01`,
  recompiled the character/controller Blueprints, and completed re-instancing.
  No gameplay test was performed. Animation appearance, hit timing, damage,
  and stamina behavior still need verification; see the milestone checklist.
- Latest faster/repeating melee verification (2026-09-14): build retry succeeded
  with the target up to date; `-0006.dll` is newer than the changed source and
  the editor log confirms successful reload/re-instancing of the new methods.
  `git diff --check` passed. Assistant started Play mode successfully, but could
  not conclusively verify attacks before the user stopped Computer Use with
  Escape. Do not mark gameplay checks complete based on that smoke test.
- Resolved the VS Code save conflict for `prototype3Character.h` on 2026-09-14.
  The unsaved buffer was an older version missing run/crouch/melee declarations
  and the stamina-drain declaration. Saved it to the Git-ignored backup
  `Saved/ConflictBackups/prototype3Character.unsaved-20260914.h.txt`, then
  reopened the current saved header. No gameplay source was changed by this
  resolution; the previous successful build still applies.

Historical build: 2026-09-11, succeeded for
`prototype3Editor Win64 Development`. The running editor detected
`UnrealEditor-prototype3-0002.dll`, hot-reloaded the module, and completed
re-instancing. No editor restart was needed for that build.

PowerShell build command used:

```powershell
& 'C:/UE_5.8/Engine/Build/BatchFiles/Build.bat' prototype3Editor Win64 Development '-Project=C:/Users/tomib/Documents/Unreal Projects/prototype3/prototype3.uproject' -WaitMutex
```

- Live Coding was disabled in editor settings when checked on 2026-09-11.
- The restricted build exited without diagnostics; the approved unrestricted
  retry succeeded. Use the normal permission flow if another build needs it.
- That build took about 287 seconds. Low available memory limited compilation
  to one process; the shared header change rebuilt dependent classes.
- No direct gameplay test was performed by the assistant. User replied "great",
  but did not explicitly report gameplay test results.
- On 2026-09-14, source values, movement logic, map config, and Git state were
  inspected initially for documentation, before the crouch implementation.
- Initial crouch validation on 2026-09-14: `prototype3Editor Win64 Development`
  succeeded in about 120 seconds, including Unreal Header Tool and compilation
  of the modified character/controller. The restricted attempt exited without
  diagnostics; the approved unrestricted retry succeeded.
- The running editor loaded `UnrealEditor-prototype3-0003.dll`, recompiled the
  first-person character/controller Blueprints, and reported successful reload
  and re-instancing. Live Coding was still disabled. No restart was performed.
- `git diff --check` passed. Engine source inspection confirmed built-in
  crouched speed selection and obstructed-standing retry behavior. No fresh
  Play-mode test was performed; camera appearance and collision interaction
  remain to be checked in-game.
- Latest validation after the ledge/tap-hold/recovery changes: editor build
  succeeded in about 104 seconds on 2026-09-14. Restricted build exited without
  diagnostics; approved unrestricted retry succeeded. The editor hot-reloaded
  `UnrealEditor-prototype3-0004.dll`, recompiled `BP_FirstPersonCharacter`, and
  completed re-instancing. `git diff --check` passed. Source inspection confirmed
  the crouched ledge restriction defaults to false in Unreal and is now enabled
  by the character. No assistant Play-mode test was performed for these changes.

## Remaining milestone work

See `MILESTONE_1.md`: attributes (Fitness, Strength, Melee Skill), derived values,
extensible modifiers, integration with movement/stamina/melee, and validation.
Weapons, shooting, enemies, equipment, inventory, and status effects are later
scope. Zombie hearing was only an assistant suggestion and is not in milestone 1.

## Continuing in a new task

Read these notes, inspect current project state, and follow the user's next
request. If validating movement, check each key separately, both together,
release order, stationary stamina, depletion, and full recovery in Play mode.
For crouch, also check each Ctrl key, tap/tap, hold/release, hold while toggled,
Ctrl with Shift/Alt, standing requests under a low ceiling, automatic standing
after clearing it, falling off ledges while crouched, repeated crouch camera
height, and 1.5x stamina recovery (including the last step to full).

`UH` is the user's project shorthand for updating these notes; see `AGENTS.md`.
