#!/usr/bin/env python3
"""Guardrail: the JUCE submodule is at the pinned release tag.

docs/GUARDRAILS.md -> JUCE pin. libs/JUCE_VERSION names the release tag Hums
is pinned to; nothing else ties that file to the commit libs/JUCE is actually
at. This check fails when

  * the commit checked out in libs/JUCE does not carry the pinned tag, or
  * the commit the repository records for libs/JUCE (the index entry, which is
    what the next commit would contain) is not the commit checked out there.

Exit codes: 0 = the pin holds, 1 = violation, 2 = the check itself could not
run. A violation prints a line starting with "check_juce_pin: VIOLATION"; the
must-fail fixture test looks for exactly that, so a crashing script can never
pass for a detected violation.

Runs with the Python that ships with the pinned Xcode (TOOLCHAIN.md); standard
library only.
"""

import argparse
import pathlib
import subprocess
import sys

NAME = "check_juce_pin"
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent


class CheckError(Exception):
    """The check could not run (as opposed to: it ran and found a violation)."""


def run_git(directory, *arguments):
    result = subprocess.run(
        ["git", "-C", str(directory), *arguments],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
    )
    if result.returncode != 0:
        command = " ".join(arguments)
        raise CheckError(f"git {command} failed in {directory}: {result.stderr.strip()}")
    return result.stdout.strip()


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


def find_violations(repository_root, submodule_path, version_file):
    pinned_tag = read_pinned_tag(version_file)
    juce_directory = repository_root / submodule_path
    if not (juce_directory / ".git").exists():
        raise CheckError(f"{juce_directory} is not checked out (git submodule update --init)")

    checked_out = run_git(juce_directory, "rev-parse", "HEAD")
    tags = run_git(juce_directory, "tag", "--points-at", "HEAD").split()
    recorded = recorded_commit(repository_root, submodule_path)

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
    return violations


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--version-file",
        type=pathlib.Path,
        default=REPOSITORY_ROOT / "libs" / "JUCE_VERSION",
        help="the file holding the pinned tag (the must-fail fixture passes its own)",
    )
    arguments = parser.parse_args()

    try:
        violations = find_violations(REPOSITORY_ROOT, "libs/JUCE", arguments.version_file)
    except CheckError as error:
        print(f"{NAME}: ERROR: {error}")
        return 2

    for violation in violations:
        print(f"{NAME}: VIOLATION: {violation}")
    if violations:
        print(f"{NAME}: moving the JUCE pin is a decision for Roland (CLAUDE.md -> Toolchain)")
        return 1

    print(f"{NAME}: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
