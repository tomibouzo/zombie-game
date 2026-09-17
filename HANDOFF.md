# Project handoff

Updated: 2026-09-16

## Project and repository state

- Unreal Engine 5.8 C++ project: `prototype3.uproject`.
- Local project: `C:\Users\tomib\Documents\Unreal Projects\prototype3`.
- Engine used for successful builds: `C:\UE_5.8`.
- Startup map: `/Game/FirstPerson/Lvl_FirstPerson`.
- Default game mode: `/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode`.
- Current branch: `feature/tomi-01-locomotion-state` at `6eba6ef`
  (`Refactor locomotion, crouch, and melee state handling`). Local and remote
  feature branches match.
- `main` and `origin/main` both point to `c25495b`
  (`Add crouching and basic unarmed melee combat (#1)`).
- The feature commit was accidentally pushed directly to `main`. It was first
  preserved on the remote feature branch, then local and remote `main` were
  restored to `c25495b` with `--force-with-lease`. The feature branch is now one
  commit ahead of `main`.
- No pull request has been opened or merged for `6eba6ef` yet.
- `MILESTONE_1.md` remains the milestone requirements and validation tracker.

### Intentional uncommitted map work

- The working tree contains World Partition / One File Per Actor changes under
  `Content/__ExternalActors__/FirstPerson/Lvl_FirstPerson`: 12 tracked actor
  packages are deleted and two new actor packages are untracked.
- The user confirmed these edits are intentional: a hittable test character was
  added, some objects were removed, and a low ceiling was added for crouch tests.
- The user restored these edits after the branch correction, opened the level,
  and reported that it worked correctly. The temporary stash was then dropped.
- These actor edits are local-only, uncommitted, and not backed up on GitHub.
  Do not discard them. Decide whether they are permanent test-arena content
  before committing them, and keep them separate from unrelated source changes.

## Finalized locomotion architecture

The character now separates input requests from resolved physical state:

- `FPlayerGaitInputIntent` stores each gait key's held/toggled state, whether it
  was toggled at press time, and press start time.
- `FPlayerLocomotionIntent` stores movement input plus separate run and sprint
  intent.
- `EPlayerLocomotionGait` is the mutually exclusive resolved gait: Walking,
  Running, or Sprinting.
- `EPlayerLocomotionStance` reports the physical stance: Standing or Crouching.
- `ResolveLocomotionState()` is the central policy point. It considers crouch,
  forward input, horizontal velocity, stamina/exhaustion, and request priority.
- `SetActiveGait()` is the central place that applies walking/running/sprinting
  speed to Character Movement. Sprint has priority over run.
- Blueprint-accessible getters expose active gait, stance, running, and sprinting.
- Unreal Character Movement remains authoritative for whether the capsule can
  physically crouch or stand. Crouch callbacks synchronize the reported stance.

This avoids treating a toggle request as proof that an action is being performed,
prevents simultaneous run/sprint states, and gives melee and future systems a
small public state interface instead of duplicating movement rules.

## Current movement, stamina, and input behavior

Current C++ defaults (Blueprint overrides may differ):

| Variable | Default | Meaning |
| --- | ---: | --- |
| `MaxHealth` | 100 | Starting and maximum health |
| `MaxStamina` | 100 | Starting and maximum stamina |
| `WalkSpeed` | 350 cm/s | Forward walking speed |
| `SideAndBackSpeedMultiplier` | 0.666667 | Pure side/back speed is one-third slower |
| `RunSpeed` | 500 cm/s | Forward/forward-diagonal running speed |
| `SprintSpeed` | 700 cm/s | Forward/forward-diagonal sprint speed |
| `CrouchSpeed` | 150 cm/s | Crouched movement speed |
| `RunStaminaDrainPerSecond` | 2 | Actual running drain |
| `StaminaDrainPerSecond` | 4 | Actual sprinting drain |
| `StaminaRecoveryPerSecond` | 50 | Base recovery |
| `CrouchStaminaRecoveryMultiplier` | 1.5 | Actual crouched recovery multiplier |
| `ExhaustionRecoveryFraction` | 0.25 | Unlock gait after recovering 25% stamina |
| `GaitHoldThreshold` | 0.25 s | Tap-versus-hold boundary for Shift/Alt |
| `CrouchHoldThreshold` | 0.25 s | Tap-versus-hold boundary for Ctrl |

- Left/right Shift control running; left/right Alt control sprinting.
- A short Shift/Alt press toggles that gait. Holding activates it until release.
  Pressing and holding an already toggled gait keeps it active, then clears it
  on release rather than canceling immediately on key-down.
- Pure sideways/backward input uses the one-third speed reduction. Forward
  diagonals remain full speed and qualify for run/sprint. Pure side/back input
  currently receives no run/sprint boost or drain.
- Drain requires the resolved gait plus actual horizontal movement. Stationary
  gait requests recover stamina rather than draining it.
- Empty stamina forces Walking. Run/sprint become available again after recovery
  reaches 25% of maximum; the request may remain pending.
- Sprint wins whenever run and sprint are both requested.

### Crouch and gait transitions

- Ctrl cancels tap-toggled run/sprint requests and starts crouching, even if the
  character was moving at run/sprint speed.
- A physically held Shift or Alt blocks crouch until release. If Ctrl remains
  held, crouch begins after the final held gait key releases.
- While physically crouched without forward input, Shift/Alt are ignored and do
  not latch a delayed request.
- While crouched with forward input, Shift/Alt request standing. Run/sprint does
  not become active until Unreal confirms that standing physically succeeded.
- Under a low ceiling the character remains crouched at crouch speed. A tapped
  gait request stays pending and resolves after headroom becomes available.
- Existing Ctrl behavior remains: tap toggles crouch; hold crouches only until
  release; Unreal prevents standing without headroom and retries when clear.
- `bCanWalkOffLedgesWhenCrouching` remains enabled. Camera height changes are
  immediate; no smoothing or dedicated crouch animation has been added.

## Melee integration

- The existing unarmed system remains on `UPlayerMeleeComponent`: LMB attacks,
  holding repeats, RMB is reserved, accepted swings cost stamina, and point
  damage uses a camera-directed blocking sweep.
- Primary attack requests standing before starting.
- Every repeated `TryAttack()` requests and verifies standing again. Punches do
  not execute while crouched or while a low ceiling prevents standing; held LMB
  keeps retrying through the existing attack loop.
- An action-forced stand does not re-arm crouch. The player must press Ctrl again
  after the action stands them.
- Melee defaults remain damage 10, stamina cost 10, interval 0.525 s, hit delay
  0.175 s, reach 150 cm, and radius 12 cm.

## Relevant files

- `Source/prototype3/Core/Characters/prototype3Character.h/.cpp`: locomotion
  intent/state types, gait and crouch input, resolver, stamina, speeds, posture,
  health, and action entry points.
- `Source/prototype3/Core/PlayerControllers/prototype3PlayerController.h/.cpp`:
  runtime Enhanced Input mappings. Shift uses the controller-owned run action;
  Alt uses `/Game/Input/Actions/IA_Sprint`; Ctrl uses the runtime crouch action.
- `Source/prototype3/Gameplay/Combat/Melee/PlayerMeleeComponent.h/.cpp`: repeated
  unarmed attack flow, standing validation, stamina spending, delayed hit sweep,
  damage, and animation.
- `MILESTONE_1.md`: accepted milestone scope and outstanding validation.
- `Source/prototype3/Legacy/README.md`: retired modes. Do not add new gameplay
  dependencies to legacy classes.

## Validation actually performed

- `prototype3Editor Win64 Development` compiled successfully after the initial
  locomotion-state refactor, producing `UnrealEditor-prototype3-0015.dll`.
- A second full build after hybrid Shift/Alt handling and the 25% exhaustion
  threshold succeeded on 2026-09-15, including Unreal Header Tool and C++
  compilation, producing `UnrealEditor-prototype3-0016.dll`.
- `git diff --check` passed after both implementations. A fresh comparison of
  `main...feature/tomi-01-locomotion-state` also passed on 2026-09-16.
- Editor logs show `-0016.dll` loaded, `-0017.dll` subsequently hot-loaded, and
  PIE sessions started and stopped successfully. This is startup/smoke evidence,
  not proof of every gameplay transition.
- The user reported that the locomotion/crouch behavior worked, and later
  reopened and played the intentionally edited test level successfully after the
  stash/branch repair.
- No itemized final Play-mode record confirms every hybrid tap/hold timing case,
  simultaneous Shift/Alt release order, exact 25% exhaustion boundary, or every
  melee repetition/damage edge case. Keep those checks open in `MILESTONE_1.md`.

Build command:

```powershell
& 'C:/UE_5.8/Engine/Build/BatchFiles/Build.bat' prototype3Editor Win64 Development '-Project=C:/Users/tomib/Documents/Unreal Projects/prototype3/prototype3.uproject' -WaitMutex
```

## Remaining and planned work

### User-approved final addition for this branch (not implemented)

- Before opening the pull request, allow running and sprinting during pure
  sideways and backward movement, but keep those directions slower than forward
  running/sprinting. Preserve full-speed forward diagonals unless the user changes
  that decision.
- The current implementation only permits run/sprint when movement has a forward
  component; pure side/back remains reduced walking/crouch speed and has no gait
  stamina drain. This is the behavior the final addition must replace.
- Implement this on `feature/tomi-01-locomotion-state`, then test directional
  speed, drain, gait priority, crouch transitions, and exhaustion before opening
  the pull request into `main`.

### Existing milestone work

- Implement Fitness, Strength, and Melee Skill; define derived-stat formulas and
  an extensible modifier system; route movement, stamina, and melee through them.
- Complete the gameplay validation checklist in `MILESTONE_1.md`, especially
  melee tap/hold/damage/stamina cases and crouch/low-ceiling combinations.
- Weapons, shooting, enemies, equipment, inventory, and status effects remain
  later scope. Zombie hearing was only an earlier suggestion and is not approved
  milestone work.

## Continuing in a new task

1. Read this file and inspect Git status before changing anything.
2. Preserve the intentional uncommitted External Actor map files.
3. Work on the current feature branch, not `main`.
4. Implement the user-approved reduced-speed side/back run and sprint as the
   final addition to this branch. Keep the intent/gait/stance separation intact.
5. Validate tap and hold for both Shift and Alt; both keys together and each
   release order; Ctrl against toggled versus physically held gait; stationary
   and moving crouch; obstructed standing; exhaustion to exactly 25%; pure side,
   backward, forward, and diagonal movement; and stamina drain/recovery.
6. Keep implemented work separate from suggestions and update `MILESTONE_1.md`
   only when checks have actually been performed.

`UH` is the shorthand for updating these notes; see `AGENTS.md`.
