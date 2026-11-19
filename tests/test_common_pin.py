import importlib.util
import json
from pathlib import Path

TAG = "v0.8.0"
COMMIT = "ab7f1a04095cd998a590963649be69e4ee11a246"


def _module():
    spec = importlib.util.spec_from_file_location("check_common_pin", "scripts/check_common_pin.py")
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def test_repository_pin_matches_the_suite_version() -> None:
    module = _module()
    pyproject = Path("pyproject.toml").read_text(encoding="utf-8")
    pin = json.loads(Path("share/voxprint-common.pin.json").read_text(encoding="utf-8"))
    assert module.check(pyproject, pin, TAG, COMMIT) == []


def test_release_wheel_pin_is_refused() -> None:
    module = _module()
    pyproject = (
        'dependencies = ["voxprint-common @ https://github.com/Mitroshenkov87/voxprint-common/'
        'releases/download/v0.8.0/voxprint_common-0.8.0-py3-none-any.whl"]'
    )
    pin = {"tag": TAG, "commit": COMMIT, "source": module.REPO and f"git+{module.REPO}@{TAG}"}
    errors = module.check(pyproject, pin, TAG, COMMIT)
    assert any("release download" in line for line in errors)


def test_wrong_commit_is_refused() -> None:
    module = _module()
    pyproject = Path("pyproject.toml").read_text(encoding="utf-8")
    pin = json.loads(Path("share/voxprint-common.pin.json").read_text(encoding="utf-8"))
    assert module.check(pyproject, pin, TAG, "0" * 40)
