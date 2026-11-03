# Zombie polish — 2026-11-02

Authorized: finish chase recovery, polish current senses and optionally reuse existing mannequin/attacks. Preserve basic walking zombie, sight occlusion, hearing location clues, strike/strike/bite, no radiation or new abilities. No commit/push.

Approach: repair navigation at its source, retain StateTree and editable tuning. Reuse mannequin and existing sequences; keep geometry as fallback. Avoid replacing the AI or introducing external assets.

- [x] Reproduce cold-start/repeated attack retreat with ChaseRecovery; inspect lab navigation coverage, repair only evidenced causes, retest.
- [x] Add perception edge tests (close versus far, field of view, crouched visibility, partial cover, remembered clues); implement focused refinements.
- [x] Add mannequin idle/walk/strike presentation synchronized to existing damage timing, cancellation and death; verify assets and captures.
- [x] Build, zombie integration, general regression, review changes, update guide and Obsidian with actual results.

Evidence: historical ChaseCoverage fails phase 1; navigation available only central +/-900 despite bounds +/-1975. Need inspect collision/navigation export and map settings before choosing correction.

Checkpoint 2026-11-02: complete editor navigation bake passed and saved; fresh rendered 5/5 zombie tests passed at 23:28:07 UTC after reload. Four chase recoveries completed in 2.2–2.5 s including intentional windup/recovery and 80 cm travel. Head cover, cold acquisition, smooth proximity, hearing and existing combat checks pass. Added dedicated presentation scenario to validate root-motion isolation, interruption/fallback/death and capture attacks; compile/run still pending. General 41-test regression is running.

Rulings: preserve current branch with its uncommitted prototype; no worktree relocation or commit. Failed force-on-load and voxel-filter experiments removed. Initial synchronous editor rebuild hit AsyncLoadLock; latent repair now waits for genuine load completion. Smart App Control blocked one compiled DLL; the subsequent required build for the reviewer's timeout correction loaded normally, with no security configuration changes. Earlier security-block warning is no longer the current blocker.

Final verification: Development Editor build succeeds. Rendered six-test zombie suite 6/6 at 2026.11.02-23.30.25 UTC, no skipped tests; mannequin windup/death screenshots inspected. General regression37/41 at23:28:49 UTC, exact same16 errors across DropPlayIntegration, ItemUsePlayIntegration, PlayIntegration, RotationPlayIntegration as historical main Baseline. No new main rebuild claimed. Review found only missing test startup timeout/guards and duplicate screenshot; corrected. Final guard-only presentation test recompilation succeeds and narrow rendered rerun passed 1/1 at 2026.11.02-23.31.05 UTC (known renderer warning). Existing gameplay suite and regression need no repeat for test-only guard edits.

Remaining product limits: generic mannequin animations, approximate 45% contact phase, third strike as bite placeholder, hearing radius without acoustics, local search and single-player target. No packaged build or prolonged manual play. No commit/push/merge.



User acceptance: user tested the laboratory and confirmed it works. User subsequently requested feature/ naming and a local commit; target branch feature/zombie-prototype. Push/merge remain unauthorized.

