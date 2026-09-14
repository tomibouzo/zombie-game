# Milestone 1 — Player Core Systems (weeks 1–2)

Updated: 2026-09-14

## Tracking rules

Check implementation items off when their code is implemented, and update this
file as work progresses. Validation is tracked separately: a checked source item
does not imply a successful build or gameplay test. The earlier no-compilation
request applied only to the drain adjustment; the user clarified that normal
compilation is authorized for subsequent feature work.
The user later authorized compiling and checking the faster/repeating melee
change before pushing. The build check succeeded; gameplay validation remains open.

## Implemented foundation

- [x] Walking, Shift running, and Alt sprinting with separate speeds.
- [x] Sneaking/crouching with separate speed and lower capsule/view.
- [x] Combined Ctrl tap-to-toggle and hold-to-crouch controls.
- [x] Crouched ledge movement and safe standing under low ceilings.
- [x] Health capacity, current health, incoming damage, and health display.
- [x] Stamina capacity, consumption, regeneration, exhaustion, and display.
- [x] Crouched stamina regeneration at 1.5x normal rate.
- [x] Double movement drain defaults: running 2, sprinting 4 stamina/second.

## Missing requirements and current work

- [x] Basic unarmed melee attack input on left mouse button (source implemented).
- [x] Melee hit detection and damage with configurable base damage (source implemented).
- [x] Melee stamina cost and attack interval (source implemented).
- [x] Shorten attack interval and hit delay by one eighth; hold left-click to repeat.
- [x] Generic primary/secondary action inputs; reserve right mouse for future behavior.
- [ ] Player attributes: Fitness, Strength, and Melee Skill.
- [ ] Define and implement attribute-to-stat formulas.
- [ ] Derive movement speeds from player attributes.
- [ ] Derive stamina capacity and consumption from player attributes.
- [ ] Derive melee damage from player attributes.
- [ ] Add a shared stat/modifier system that future systems can extend.
- [ ] Route movement, stamina, and melee through those calculated values.
- [ ] Demonstrate that changing attributes changes the derived values.

## Accepted input design and current melee defaults

Left mouse is **Primary Action**, right mouse is **Secondary Action**. Future held
items should supply their action behavior: primary can punch/shoot, secondary
can use/aim/block/interact. Avoid putting item-type switch statements in input
bindings. The current character exposes overridable start/end action methods;
there is no equipped-item system yet. Secondary action is mapped but does nothing.

Current unarmed defaults, editable on the character's **Melee** component:

| Setting | Default |
| --- | --- |
| Damage | 10 |
| Stamina cost | 10 per accepted punch, including misses |
| Attack interval | 0.525 seconds (12.5% shorter than 0.6) |
| Hit delay | 0.175 seconds after each swing starts (12.5% shorter than 0.2) |
| Reach | 150 cm from the camera, including the sweep radius |
| Hit radius | 12 cm |

Press attempts a punch immediately; holding repeats at the attack interval.
Release/cancellation stops new swings; an already-started punch finishes normally.
Cooldown or insufficient stamina rejects the attempt without spending stamina.
Holding retries when stamina recovers enough. Repetition stops on death or loss
of possession, and slow frames do not cause bursts of catch-up attacks.
Dead players cannot attack. Damage
is checked at impact time using the current view direction, stops at the first
blocking hit, and ignores the player. The default Camera collision channel
blocks stock Pawn capsules and walls. Target collision must block that channel
and the target must handle Unreal damage to lose health.

The existing `MM_Attack_01` sequence plays through the body's `DefaultSlot`, with
the first-person mesh copying its pose. Timing and first-person appearance need
Play-mode verification. Regeneration pauses for the attack interval, then resumes
at the normal or crouched rate. Movement stamina costs still apply while moving.

## Validation still required

User follow-up (2026-09-14): attacking works after a computer restart. Specific
tap/hold, damage, stamina, and movement combination checks below remain unconfirmed.
Pre-push source review and fresh `git diff --check` found no push-blocking issue;
no gameplay source changes or new build were made during that review.

Attack regression follow-up (2026-09-14): user reports LMB produces no action or
stamina consumption. Controller mouse actions now initialize lazily at runtime,
and input setup rebuilds the runtime mappings. Suspected hot-reload input state
is not a confirmed root cause; user gameplay verification remains required.
Fresh editor build succeeded in 164 seconds (`-0009.dll`); `git diff --check`
passed. No gameplay validation was performed for this repair.

- [x] Compile the initial melee component (2026-09-14).
- [x] Compile the faster timing and hold-to-repeat changes (build retry succeeded; target up to date).
- [ ] Verify taps start one punch and holding repeats at the shortened interval.
- [ ] Verify release/cancellation stops repetition without canceling the current punch.
- [ ] Verify repetition pauses for insufficient stamina and resumes after recovery.
- [ ] Verify close targets receive one damage application; misses cost stamina.
- [ ] Verify out-of-range targets, walls, and self cannot be hit incorrectly.
- [ ] Verify attack interval, insufficient stamina, zero stamina, and recovery.
- [ ] Verify a player who dies during windup deals no pending hit.
- [ ] Verify right-click has no gameplay effect or stamina cost.
- [ ] Verify animation and hit timing while walking, sprinting, and crouching.
- [ ] Recheck crouched ledges, tap/hold, low ceilings, and stamina recovery.
- [ ] Verify attribute changes affect movement, stamina, and melee after stat work.

A simple damage-receiving test target is sufficient for melee validation; enemy
behavior is outside this milestone. Static code/asset inspection and
`git diff --check` passed. `prototype3Editor Win64 Development` built successfully
on 2026-09-14 (about 114 seconds), including the doubled drain defaults. The
open editor loaded `UnrealEditor-prototype3-0005.dll`, loaded the attack animation,
recompiled the character/controller Blueprints, and completed re-instancing.
That build predates hold-to-repeat and the 12.5% timing reduction. A subsequent
build retry on 2026-09-14 succeeded and reported the latest target up to date.
`UnrealEditor-prototype3-0006.dll` is newer than the changed melee source, and
the editor log confirms successful reload of StartAttacking/StopAttacking and
re-instancing. `git diff --check` passed. An assistant Play-mode smoke test
started successfully, but attack behavior was not conclusively verified before
the user stopped Computer Use with Escape. Gameplay validation remains open.

## Deferred scope

Weapons, shooting, enemies, equipment, inventory, and status effects belong to
later milestones. Their future modifiers must be supported by the stat design,
but those systems themselves are not part of milestone 1. Zombie hearing is not
a milestone 1 requirement. Multiplayer attack routing/prediction is not implemented.
