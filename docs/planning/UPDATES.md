# Updating from official Augustus

Development and the usable version live on `feature/planning-mode`. `origin` is the user's fork; `upstream` is `https://github.com/Keriew/augustus.git`.

The fork's **Check official Augustus updates** workflow runs daily at 03:15 UTC (08:15 Almaty) and can be started manually from Actions. Scheduled workflows must be enabled on the fork and live on its default branch, which is set to `feature/planning-mode`. GitHub may disable schedules after repository inactivity; the Actions page permits re-enabling them.

1. Fetch official `master` and the fork's working branch.
2. If upstream is already integrated, report no changes.
3. Otherwise prepare `updates/upstream-<official SHA>-<working SHA>` by a regular merge. Never rebase, reset or force-push the working version.
4. On conflicts, abort the merge and upload `upstream-compatibility-report` listing the affected files. The workflow fails visibly in Actions.
5. A clean candidate is cross-compiled for macOS ARM64 on a Mac runner with universal SDL frameworks. Model/storage tests run with AddressSanitizer and UndefinedBehaviorSanitizer on Linux and the macOS build host. The engine regression harness also compiles, so API integration failures are detected.
6. Download the candidate application from that run's artifacts. Proprietary Caesar III assets and personal saves are not uploaded to GitHub, so full engine runtime/visual checks require a local test installation.

No candidate is promoted automatically. Only after successful checks and a local play/visual check should the working branch advance. A maintainer can use:

```sh
git fetch origin
git switch feature/planning-mode
git pull --ff-only origin feature/planning-mode
# Substitute the exact branch displayed by the successful Actions report.
git merge --ff-only origin/updates/upstream-<official>-<working>
git push origin feature/planning-mode
```

If fast-forward is impossible because development has continued, do not force it. Prepare a new candidate from the updated working head. Keep prior Releases and downloaded applications for rollback. GitHub Actions artifacts expire after 90 days; Releases persist until deliberately removed.

A conflict-free merge does not establish runtime compatibility. In particular, review changes to building type ids, construction transaction boundaries, image/rotation APIs, save paths and the interpretation of footprints. Preserve the user's working branch until that review and local testing pass.

## Reproducing a build on Apple Silicon

```sh
brew install cmake ninja sdl2 sdl2_mixer
git clone https://github.com/CuriosityZh/augustus.git
git -C augustus switch feature/planning-mode
cd augustus
git remote add upstream https://github.com/Keriew/augustus.git
cmake -S tests/planning -B build-tests -G Ninja
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
cmake -S . -B build-planning -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-planning --parallel 3
scripts/planning/package-macos.sh build-planning dist
```

No OpenAI API or other paid service is used. No PR is opened in the official repository. This workflow prepares candidates and reports through GitHub Actions; it does not send messages or open issues.
