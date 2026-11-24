#!/usr/bin/env python3
"""Guardrail: the JUCE submodule is at the pinned release tag.

docs/GUARDRAILS.md -> JUCE pin. libs/JUCE_VERSION names the release tag Hums
is pinned to; nothing else ties that file to the commit libs/JUCE is actually
at. This check fails when

  * the commit checked out in libs/JUCE does not carry the pinned tag, or
  * the commit the repository records for libs/JUCE (the index entry, which is
    what the next commit would contain) is not the commit checked out there, or
  * the files in libs/JUCE are not the files of the commit checked out there:
    one is edited, deleted, staged or added. The build compiles the files, not
    the commit.

Exit codes: 0 = the pin holds, 1 = violation, 2 = the check itself could not
run. A violation prints a line starting with "check_juce_pin: VIOLATION", and
after the violations a line with their number; the must-fail fixture tests
look for exactly those, so a crashing script can never pass for a detected
violation, and a fixture cannot pass for another rule than its own.

Runs with the Python that ships with the pinned Xcode (TOOLCHAIN.md); standard
library only.
"""

import argparse
import os
import pathlib
import subprocess
import sys

NAME = "check_juce_pin"
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent
SUBMODULE_PATH = "libs/JUCE"
# A violation names this many changed files and counts the rest.
MOST_FILES_NAMED = 10


class CheckError(Exception):
    """The check could not run (as opposed to: it ran and found a violation)."""


def run_git(directory, *arguments, environment=None, strip=True):
    result = subprocess.run(
        ["git", "-C", str(directory), *arguments],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
        env=environment,
    )
    if result.returncode != 0:
        command = " ".join(arguments)
        raise CheckError(f"git {command} failed in {directory}: {result.stderr.strip()}")
    return result.stdout.strip() if strip else result.stdout


def environment_that_names_no_repository(repository_root):
    """The caller's environment without the variables that tell git where a
    repository is.

    GIT_DIR and its relatives name one repository, and whoever set them (git
    itself, for a hook in a linked worktree) meant Hums. A question put to
    libs/JUCE with them set is answered for Hums: its HEAD, its tags. Without
    them git finds the submodule by looking, from the directory it is run in.
    Git itself says which variables these are.
    """
    names = run_git(repository_root, "rev-parse", "--local-env-vars").split()
    return {name: value for name, value in os.environ.items() if name not in names}


def read_pinned_tag(version_file):
    try:
        lines = version_file.read_text(encoding="utf-8").split()
    except OSError as error:
        raise CheckError(f"cannot read {version_file}: {error}")
    if len(lines) != 1:
        raise CheckError(f"{version_file} must hold exactly one tag on one line")
    return lines[0]


def recorded_commit(repository_root, submodule_path):
    """The commit the superproject's index records for the submodule."""
    entry = run_git(repository_root, "ls-files", "--stage", "--", submodule_path)
    # Format: "<mode> <object> <stage>\t<path>"; mode 160000 is a submodule.
    fields = entry.split()
    if len(fields) < 4 or fields[0] != "160000":
        raise CheckError(f"{submodule_path} is not a submodule of {repository_root}")
    return fields[1]


def files_not_as_committed(juce_directory, environment):
    """The files in the submodule that are not as its checked-out commit has
    them: edited, deleted, staged, or added (untracked). Files the submodule's
    own .gitignore names are not looked at.

    --no-optional-locks: a plain "git status" may rewrite the submodule's
    index to save itself work the next time, and a test run writes nothing.
    """
    status = run_git(
        juce_directory,
        "--no-optional-locks",
        "status",
        "--porcelain",
        environment=environment,
        strip=False,
    )
    # One line per file: two status letters, a space, the path. The first
    # letter can be a space, so the output is taken as it is, not stripped.
    return [line[3:] for line in status.splitlines() if line]


def find_violations(repository_root, submodule_path, version_file):
    pinned_tag = read_pinned_tag(version_file)
    juce_directory = repository_root / submodule_path
    if not (juce_directory / ".git").exists():
        raise CheckError(f"{juce_directory} is not checked out (git submodule update --init)")

    # The two questions to the submodule are asked without the caller's
    # repository variables. The question to Hums' own index keeps them: during
    # a commit git names there the index that the commit will be made from.
    submodule_environment = environment_that_names_no_repository(repository_root)
    checked_out = run_git(juce_directory, "rev-parse", "HEAD", environment=submodule_environment)
    tags = run_git(
        juce_directory, "tag", "--points-at", "HEAD", environment=submodule_environment
    ).split()
    recorded = recorded_commit(repository_root, submodule_path)
    changed_files = files_not_as_committed(juce_directory, submodule_environment)

    violations = []
    if pinned_tag not in tags:
        found = ", ".join(tags) if tags else "no tag"
        violations.append(
            f"{version_file} pins JUCE '{pinned_tag}', but {submodule_path} is at "
            f"commit {checked_out[:12]} ({found})"
        )
    if recorded != checked_out:
        violations.append(
            f"the repository records commit {recorded[:12]} for {submodule_path}, "
            f"but commit {checked_out[:12]} is checked out there"
        )
    if changed_files:
        shown = ", ".join(changed_files[:MOST_FILES_NAMED])
        if len(changed_files) > MOST_FILES_NAMED:
            shown += f" and {len(changed_files) - MOST_FILES_NAMED} more"
        violations.append(
            f"the files in {submodule_path} are not the files of commit "
            f"{checked_out[:12]}: changed or added there: {shown}"
        )
    return violations


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--repository-root",
        type=pathlib.Path,
        default=REPOSITORY_ROOT,
        help="the repository whose libs/JUCE is checked (a must-fail fixture "
        "passes a stand-in made for the test)",
    )
    parser.add_argument(
        "--version-file",
        type=pathlib.Path,
        help="the file holding the pinned tag, libs/JUCE_VERSION of the "
        "repository if not given (a must-fail fixture passes its own)",
    )
    arguments = parser.parse_args()

    repository_root = arguments.repository_root.resolve()
    version_file = arguments.version_file or repository_root / "libs" / "JUCE_VERSION"

    try:
        violations = find_violations(repository_root, SUBMODULE_PATH, version_file)
    except CheckError as error:
        print(f"{NAME}: ERROR: {error}")
        return 2

    for violation in violations:
        print(f"{NAME}: VIOLATION: {violation}")
    if violations:
        plural = "" if len(violations) == 1 else "s"
        print(f"{NAME}: {len(violations)} violation{plural}")
        print(f"{NAME}: moving the JUCE pin is a decision for Roland (CLAUDE.md -> Toolchain)")
        return 1

    print(f"{NAME}: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
