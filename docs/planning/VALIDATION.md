# Planning Mode beta validation — 2026-10-22

Base: Augustus 4.0.0, `7af2ee66fb816145795192c6648a6720e83d284c`.
Delivery target: macOS Apple Silicon (ARM64). This is an independent beta, not an official Augustus release.

## Passed locally

- Full application and optional engine harness compiled natively on ARM64.
- 1,463 checks/corruption cases in model/storage tests, with AddressSanitizer and UndefinedBehaviorSanitizer.
- Standard SHA-256 vectors; companion roundtrip, copy, rename, absent/corrupt/mismatched files, truncated files, invalid geometry and failed atomic replacement.
- A real game save (version 192) loaded from a disposable private copy.
- Planning, preview, hiding, mode switching and defensive construction guards preserved simulation state. The comparison includes all serialized state plus live buildings/figures/scenario data for serializers with unwritten padding. Production serialization was not altered for this check.
- Real one-cell construction overlap removed an entire planned warehouse; real roads removed only intersected planned road/wall/aqueduct cells. Failed real construction retained plans.
- Move, erase and Undo; warehouse, fort/grounds and hippodrome orientations; building variants.
- Both low and ship bridges were planned without changing the actual bridge construction cache.
- Save/Save As through the common writer, timestamped Quick Save, yearly Auto Save and rotating backup companions.
- Real city reload with valid, missing and corrupt companions; an original career save (version 102) and a classic custom `.map` scenario.
- Actual city-widget input handlers and placement preview exercised in the engine harness; no operating-system input automation was required.
- Rendered four map orientations at 75% and 150% scale, plus composite buildings in all orientations and a separate preview capture. Selected images were visually inspected.
- Updater tested against disposable local Git remotes: clean merge, repeat, no changes and conflicting merge. Every case preserved the working branch; conflicts produced a report and aborted safely.
- Packaged application checked for ARM64 architecture, bundled dependencies and ad-hoc signature, then launched headlessly with no external library-path override. Packaging preserves upstream's library-validation entitlement, needed for the bundled ad-hoc-signed SDL frameworks. The original installed application's executable checksum was unchanged. The private input save copy also retained its original checksum; test outputs went to separate scratch directories.

## GitHub checks

Model/storage and updater regression passed on GitHub Linux. The first upstream check prepared a clean candidate against official `master`; it did not advance `feature/planning-mode`. Native macOS 14/15 jobs remained queued. The workflow therefore uses a supported Intel Mac runner with universal SDL frameworks to produce an explicitly verified ARM64 binary; native ARM64 runtime tests run locally. Check the latest **Planning Mode builds** and **Check official Augustus updates** runs for final cloud build results.

## Known limitations and inherited diagnostics

- Interactive desktop automation did not respond. Extended manual play and every decorative sprite combination are not claimed as verified. The beta should be played on copies before adopting a new upstream candidate.
- Additional planning controls/status messages are in English. Touch input and Windows companion behavior are outside this Mac release's tested scope.
- Network plans use straight/Manhattan geometry, not terrain-avoiding routing. A plan is a visual intention and can mark currently unsuitable terrain. A finished plan's type/rotation is changed by replacing it; Move changes its position.
- Plans rely on exact-save SHA-256. If another Augustus version rewrites a save, the old companion is deliberately ignored. Copy both files to retain plans.
- The base serializer writes some uninitialized padding. Two regular `.svx` writes can differ even with no gameplay; the state invariance test accounts for these unused bytes without changing the save format.
- The version-192 private fixture produced upstream `Formula ID mismatch` / `Text ID mismatch` diagnostics during loading. A rotated view of that same fixture also showed black terrain gaps **before any plan existed**, and with plans hidden. These baseline issues were not repaired as part of Planning Mode, and the beta is not a claim that all existing Augustus scenario/graphics issues have been fixed.
- Automatic merge/build success is not a substitute for runtime review after upstream type-id, save, geometry or rendering changes. Candidates are never automatically promoted.

Private saves, game installation assets, test output files and screenshots are excluded from Git and release artifacts. The application includes only Augustus's distributable project assets; original Caesar III data must be supplied by the player.
