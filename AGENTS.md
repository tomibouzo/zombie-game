# Project continuity

`HANDOFF.md` is an optional local-only continuity file and must not be committed.
If it exists, read it at the start of a new task, treat it as a dated summary,
and verify it against the current code, Git status, and remote branch state
before changing anything.

## UH shorthand

When the user sends `UH` (case-insensitive) as a command, interpret it as:
"Update the project handoff notes so a new task can continue."

Create or update the ignored root `HANDOFF.md` with the current date,
implemented behavior, important files and settings, accepted decisions,
validation actually performed, known limitations, unfinished work, and proposed
next steps. Check current files, Git status, and relevant remote branches;
distinguish historical test results from fresh verification. Preserve useful
context, replace stale details, and keep the notes concise. Clearly separate
user-approved work from suggestions that have not been approved.

This is a project chat shorthand. It does not by itself request a build, gameplay
changes, a commit, a new task, or archiving. After updating, briefly confirm and
link to the handoff file.
