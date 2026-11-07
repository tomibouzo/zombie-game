# Project continuity

Shared project records live in the Obsidian vault and are maintained by
whichever assistant is doing authorized work. Each assistant keeps its own
native memory; native memory points to these records and never overrides them.
Do not create or maintain `HANDOFF.md`.

## Shared records (read selectively)

Folder: `C:\Users\tomib\Documents\Obsidian Vault\Projects\prototype3\`

| Record | Read when |
| --- | --- |
| `Overview.md` → "Current checkpoint" | Resuming or handing off work. Routine pickup reads only this section. |
| `Overview.md` → branch map / history | A specific branch or past decision matters. |
| `Design rules.md` | Before changing gameplay code or project workflow. |
| `Verification.md` | Builds, tests or verification questions. |
| `Later work.md` | Planning; it records designs, not permission to start them. |

Verify checkpoints against current code and Git before changing anything.

## Editing shared records

- Reread the affected section first and preserve intervening changes.
- Keep one authoritative home per fact; link instead of copying.
- Date checkpoints and name the branch, commit or working state.
- Distinguish completed work, accepted decisions, proposals and explicit holds.
- Record verification accurately, including checks not performed.
- Keep "Current checkpoint" near 300–400 words: objective and authorized scope,
  checkout and branch, dated commit or working state, user work to preserve,
  completed changes, verification and remaining user testing, blockers/holds,
  next authorized action. Label proposals as unapproved. Move superseded
  checkpoints into the history below it. Never save secrets.

## UH shorthand

When the user sends `UH` (case-insensitive) as a command, interpret it as:
"Update the shared project records so a new task can continue."

Refresh "Current checkpoint" (and Verification or Later work if they changed)
with the current date, implemented behavior, important files and settings,
accepted decisions, validation actually performed, known limitations,
unfinished work and proposed next steps. Check current files, Git status and
relevant remote branches; distinguish historical results from fresh
verification; separate approved work from unapproved suggestions.

`UH` does not by itself request a build, gameplay changes, a commit, a push, a
merge, a new task or archiving. After updating, briefly confirm and link to the
updated record.
