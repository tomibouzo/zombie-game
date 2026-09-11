# Legacy Systems

This folder isolates systems that were tied to the retired Horror and Shooter
prototype modes. They remain buildable during the structural migration so that
existing Blueprint references can be identified and redirected safely.

Do not add new gameplay dependencies to these classes. Reusable behavior
belongs in `Gameplay`; current map startup code belongs in `Core`; presentation
belongs in `UI`.

When a legacy class has no remaining asset reference, archive it from the
runtime module in a dedicated retirement pass rather than leaving commented-out
blocks in production files. Git retains the original implementation.
