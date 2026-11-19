# Contributing

Lucent is Apache-2.0. Open a pull request for a bug fix or a small change that matches the [roadmap](ROADMAP.md).

Write English in code comments, docs, commit messages, and the pull request. The viewer is C++ and Qt 6.12. The tools window is Python 3.14.

## Check a change

```bash
python3.14 -m venv .venv
.venv/bin/pip install -e ".[dev]"
.venv/bin/ruff check .
.venv/bin/pytest
```

`packaging/linux/build-cpp-smoke.sh` builds the viewer and checks that a picture reaches the screen. Windows packaging lives under `packaging/windows/`.

## Pull requests

Keep the change focused. Public Python functions need a Google-style docstring. Public C++ headers need a Doxygen comment. CI fails when those are missing.

Opening an image stays a local operation. Do not add a network call on that path.

The Support section in `README.md` is copied byte for byte from a fixed block. Leave those bytes unchanged.
