"""Exercise the real updater against disposable local remotes; never contact GitHub."""
import os
from pathlib import Path
import subprocess
import tempfile

SCRIPT = Path(__file__).resolve().parents[2] / 'scripts/planning/prepare-upstream-update.sh'
ROOT = Path(tempfile.mkdtemp(prefix='augustus-updater-tests-'))

def run(cwd, *args, check=True, env=None):
    return subprocess.run(args, cwd=cwd, env=env, check=check, text=True, capture_output=True)

def git(cwd, *args):
    return run(cwd, 'git', *args).stdout.strip()

def fixture(name, conflict):
    folder = ROOT / name
    folder.mkdir()
    upstream, origin, author, worker = [folder / p for p in ('upstream.git', 'origin.git', 'author', 'worker')]
    git(folder, 'init', '--bare', str(upstream))
    git(folder, 'init', '--bare', str(origin))
    git(folder, 'init', '-b', 'master', str(author))
    git(author, 'config', 'user.name', 'Fixture')
    git(author, 'config', 'user.email', 'fixture@example.invalid')
    (author / 'construction.c').write_text('base\n')
    git(author, 'add', '.')
    git(author, 'commit', '-m', 'base')
    git(author, 'remote', 'add', 'upstream', str(upstream))
    git(author, 'remote', 'add', 'origin', str(origin))
    git(author, 'push', 'upstream', 'master')
    git(author, 'checkout', '-b', 'feature/planning-mode')
    (author / 'planning.c').write_text('planning retained\n')
    if conflict:
        (author / 'construction.c').write_text('planning integration\n')
    git(author, 'add', '.')
    git(author, 'commit', '-m', 'planning')
    git(author, 'push', 'origin', 'feature/planning-mode')
    original = git(author, 'rev-parse', 'HEAD')
    git(author, 'checkout', 'master')
    (author / 'construction.c').write_text('official update\n')
    git(author, 'commit', '-am', 'upstream update')
    git(author, 'push', 'upstream', 'master')
    official = git(author, 'rev-parse', 'HEAD')
    git(folder, 'clone', '--branch', 'feature/planning-mode', str(origin), str(worker))
    git(worker, 'remote', 'add', 'upstream', str(upstream))
    environment = dict(os.environ, REPORT_PATH=str(folder / 'report.txt'),
                       GITHUB_OUTPUT=str(folder / 'output.txt'), GITHUB_STEP_SUMMARY=str(folder / 'summary.txt'))
    result = run(worker, 'bash', str(SCRIPT), check=False, env=environment)
    assert git(origin, 'rev-parse', 'refs/heads/feature/planning-mode') == original
    assert (worker / 'planning.c').read_text() == 'planning retained\n'
    if conflict:
        assert result.returncode != 0
        assert 'CONFLICT' in (folder / 'report.txt').read_text()
        assert 'construction.c' in (folder / 'report.txt').read_text()
        assert not (worker / '.git/MERGE_HEAD').exists()
        assert not (folder / 'output.txt').exists()
        print('PASS updater: conflict report, aborted merge, working branch preserved')
    else:
        assert result.returncode == 0, result.stderr
        outputs = dict(line.split('=', 1) for line in (folder / 'output.txt').read_text().splitlines())
        assert git(origin, 'rev-parse', 'refs/heads/' + outputs['branch']) == outputs['candidate']
        assert (worker / 'construction.c').read_text() == 'official update\n'
        assert git(worker, 'merge-base', '--is-ancestor', original, outputs['candidate']) == ''
        assert git(worker, 'merge-base', '--is-ancestor', official, outputs['candidate']) == ''
        again = run(worker, 'bash', str(SCRIPT), check=False, env=environment)
        assert again.returncode == 0, again.stderr
        assert git(origin, 'rev-parse', 'refs/heads/' + outputs['branch']) == outputs['candidate']
        # Explicitly promote in this disposable fixture to test the no-update case.
        git(worker, 'push', 'origin', 'HEAD:feature/planning-mode')
        (folder / 'output.txt').write_text('')
        unchanged = run(worker, 'bash', str(SCRIPT), check=False, env=environment)
        assert unchanged.returncode == 0, unchanged.stderr
        assert 'No upstream changes' in (folder / 'report.txt').read_text()
        assert not (folder / 'output.txt').read_text()
        print('PASS updater: clean candidate, idempotent recheck, no-update case, no force push')

fixture('clean', False)
fixture('conflict', True)
print('PASS updater: temporary fixtures at', ROOT)
