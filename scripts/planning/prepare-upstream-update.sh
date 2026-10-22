#!/usr/bin/env bash
# Only prepare a candidate. Never reset, rebase or force-push the working branch.
set -euo pipefail
report=${REPORT_PATH:-update-report.txt}
base_branch=feature/planning-mode
git config user.name 'Augustus Planning updater'
git config user.email 'actions@users.noreply.github.com'
if ! git remote get-url upstream >/dev/null 2>&1; then
    git remote add upstream https://github.com/Keriew/augustus.git
fi
git fetch --no-tags upstream master
git fetch --no-tags origin "$base_branch"
base=$(git rev-parse "origin/$base_branch")
official=$(git rev-parse FETCH_HEAD)
# The second fetch overwrites FETCH_HEAD; read upstream explicitly.
official=$(git rev-parse upstream/master)
{
    echo 'Augustus Planning upstream compatibility report'
    echo "Working version: $base"
    echo "Official upstream: $official"
} > "$report"
if git merge-base --is-ancestor "$official" "$base"; then
    echo 'No upstream changes to integrate.' >> "$report"
    cat "$report" >> "${GITHUB_STEP_SUMMARY:-/dev/stdout}"
    exit 0
fi
candidate="updates/upstream-${official:0:12}-${base:0:12}"
existing=$(git ls-remote --heads origin "refs/heads/$candidate" | cut -f1)
if [[ -n "$existing" ]]; then
    git fetch origin "$candidate"
    candidate_sha=$(git rev-parse FETCH_HEAD)
    git merge-base --is-ancestor "$base" "$candidate_sha"
    git merge-base --is-ancestor "$official" "$candidate_sha"
    echo "Rechecking existing candidate: $candidate ($candidate_sha)" >> "$report"
else
    git checkout -b "$candidate" "$base"
    if ! git merge --no-edit "$official"; then
        {
            echo 'CONFLICT: working version was preserved. These files require integration:'
            git diff --name-only --diff-filter=U
            echo
            git diff --stat
        } >> "$report"
        git merge --abort
        cat "$report" >> "${GITHUB_STEP_SUMMARY:-/dev/stdout}"
        exit 1
    fi
    candidate_sha=$(git rev-parse HEAD)
    git push origin "HEAD:refs/heads/$candidate"
    echo "Prepared candidate: $candidate ($candidate_sha)" >> "$report"
fi
{
    echo "candidate=$candidate_sha"
    echo "branch=$candidate"
} >> "${GITHUB_OUTPUT:-/dev/stdout}"
echo 'Build and automated tests follow. Promote only after both pass; the working branch is unchanged.' >> "$report"
cat "$report" >> "${GITHUB_STEP_SUMMARY:-/dev/stdout}"
