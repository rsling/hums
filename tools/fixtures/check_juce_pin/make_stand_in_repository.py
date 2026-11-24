#!/usr/bin/env python3
"""Makes the fixture for the third rule of check_juce_pin.py: a small
repository standing in for Hums, in which the files of libs/JUCE are not the
files of the commit checked out there.

docs/GUARDRAILS.md -> JUCE pin. A repository cannot be checked in inside
another, and a test must not touch the real libs/JUCE, so this fixture is made
when the tests run, in the build directory. The stand-in has what the check
asks for and nothing else: libs/JUCE is a repository of its own with one
commit, which carries a tag; libs/JUCE_VERSION names that tag; and the
stand-in's index records that commit for libs/JUCE. So the first two rules
hold, and then one file in libs/JUCE is

  edited_file  changed and not committed, or
  added_file   put there and not committed.

Usage: make_stand_in_repository.py <edited_file|added_file> <directory>

The directory is made anew on every run. One that is there already is
replaced only if an earlier run of this script made it (it holds the marker
file below); any other is refused.

Runs with the Python that ships with the pinned Xcode (TOOLCHAIN.md); standard
library only.
"""

import os
import pathlib
import shutil
import subprocess
import sys

NAME = "make_stand_in_repository"
MARKER = "made-by-make_stand_in_repository"
TAG = "stand-in-1.0"
CASES = ("edited_file", "added_file")


def git(directory, *arguments):
    """Runs git in the directory, as nobody in particular: without the
    caller's variables that name a repository (set for a hook, they would
    point every call at Hums), and without the user's or the system's
    configuration, so the result is the same on every machine."""
    names = subprocess.run(
        ["git", "rev-parse", "--local-env-vars"],
        stdout=subprocess.PIPE,
        universal_newlines=True,
        check=True,
    ).stdout.split()
    environment = {name: value for name, value in os.environ.items() if name not in names}
    environment["GIT_CONFIG_GLOBAL"] = os.devnull
    environment["GIT_CONFIG_NOSYSTEM"] = "1"
    subprocess.run(
        [
            "git",
            "-C",
            str(directory),
            "-c",
            "user.name=Hums test fixture",
            "-c",
            "user.email=fixture@invalid",
            "-c",
            "advice.addEmbeddedRepo=false",
            *arguments,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
        env=environment,
        check=True,
    )


def make(case, root):
    if root.exists():
        if not (root / MARKER).is_file():
            raise RuntimeError(f"{root} is there and was not made by this script; not replaced")
        shutil.rmtree(root)

    juce = root / "libs" / "JUCE"
    header = juce / "modules" / "juce_core.h"
    header.parent.mkdir(parents=True)
    (root / MARKER).write_text("", encoding="utf-8")

    # libs/JUCE: one commit, tagged.
    git(juce, "init", "--quiet")
    header.write_text("// a stand-in for JUCE\n", encoding="utf-8")
    git(juce, "add", "modules/juce_core.h")
    git(juce, "commit", "--quiet", "--message", "A stand-in for a JUCE release")
    git(juce, "tag", TAG)

    # The stand-in for Hums: pins that tag and records that commit. The check
    # asks the index, so nothing is committed here.
    git(root, "init", "--quiet")
    (root / "libs" / "JUCE_VERSION").write_text(TAG + "\n", encoding="utf-8")
    git(root, "add", "libs/JUCE")

    # The planted violation.
    if case == "edited_file":
        header.write_text("// a stand-in for JUCE, edited\n", encoding="utf-8")
    else:
        (juce / "modules" / "juce_added.h").write_text("// not JUCE's\n", encoding="utf-8")


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in CASES:
        print(f"usage: {NAME}.py <{'|'.join(CASES)}> <directory>")
        return 2
    try:
        make(sys.argv[1], pathlib.Path(sys.argv[2]).resolve())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        detail = getattr(error, "stderr", "") or ""
        print(f"{NAME}: ERROR: {error} {detail.strip()}")
        return 2
    print(f"{NAME}: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
