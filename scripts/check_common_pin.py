"""Fail when the voxprint-common pin is not the wanted tag and commit.

The suite pins voxprint-common by a git tag (``git+...@vX.Y.Z``) and records the
tag's commit in ``share/voxprint-common.pin.json``. This check reads both, then
asks the remote which commit the tag points to. Release downloads and wheel
files are refused.

Usage: ``python scripts/check_common_pin.py --tag v0.8.0 --commit <sha>``.
``--offline`` skips the remote lookup.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = "https://github.com/Mitroshenkov87/voxprint-common"
_DEP = re.compile(r'"(voxprint-common(?:\[[^\]]*\])?\s*@\s*[^"]+)"')


def dependency_lines(pyproject: str) -> list[str]:
    """Return every quoted voxprint-common requirement in ``pyproject``."""
    found = [match.group(1) for match in _DEP.finditer(pyproject)]
    plain = re.findall(r'"(voxprint-common(?:\[[^\]]*\])?\s*(?:==|>=|<=|~=|<|>)[^"]*)"', pyproject)
    return found + plain


def check(pyproject: str, pin: dict, tag: str, commit: str) -> list[str]:
    """Return the reasons the pin does not match ``tag`` and ``commit``."""
    errors: list[str] = []
    wanted = f"git+{REPO}@{tag}"
    deps = dependency_lines(pyproject)
    if not deps:
        errors.append("pyproject.toml does not name voxprint-common")
    for dep in deps:
        url = dep.split("@", 1)[1].strip() if "@" in dep else ""
        if url != wanted:
            errors.append(f"voxprint-common requirement {dep!r} is not {wanted!r}")
    if "releases/download" in pyproject or ".whl" in pyproject:
        errors.append("pyproject.toml names a release download or a wheel file")
    if pin.get("tag") != tag:
        errors.append(f"pin file tag {pin.get('tag')!r} is not {tag!r}")
    if pin.get("commit") != commit:
        errors.append(f"pin file commit {pin.get('commit')!r} is not {commit!r}")
    if pin.get("source") != wanted:
        errors.append(f"pin file source {pin.get('source')!r} is not {wanted!r}")
    return errors


def remote_commit(tag: str) -> str:
    """Return the commit ``tag`` points to on the remote (peeled for annotated tags)."""
    out = subprocess.run(
        ["git", "ls-remote", REPO, f"refs/tags/{tag}", f"refs/tags/{tag}^{{}}"],
        check=True,
        capture_output=True,
        text=True,
        timeout=60,
    ).stdout
    refs = dict(reversed(line.split("\t", 1)) for line in out.splitlines() if "\t" in line)
    return refs.get(f"refs/tags/{tag}^{{}}") or refs.get(f"refs/tags/{tag}", "")


def main(argv: list[str] | None = None) -> int:
    """Run the check and return a process exit code."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tag", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--root", default=".")
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args(argv)
    root = Path(args.root)
    pyproject = (root / "pyproject.toml").read_text(encoding="utf-8")
    pin = json.loads((root / "share" / "voxprint-common.pin.json").read_text(encoding="utf-8"))
    errors = check(pyproject, pin, args.tag, args.commit)
    if not args.offline:
        try:
            found = remote_commit(args.tag)
        except (OSError, subprocess.SubprocessError) as exc:
            errors.append(f"could not read {args.tag} from {REPO}: {exc}")
        else:
            if found != args.commit:
                errors.append(f"{args.tag} on {REPO} is {found or 'missing'}, not {args.commit}")
    for line in errors:
        print(line, file=sys.stderr)
    if not errors:
        print(f"voxprint-common pin ok: {args.tag} = {args.commit}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
