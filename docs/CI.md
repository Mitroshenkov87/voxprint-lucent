# CI minutes

Private GitHub Actions bills Ubuntu minutes at 1x and Windows minutes at 2x. The free monthly allowance is the budget. The owner decides about billing. This workflow is arranged so a pull request stays inside a small Ubuntu slice, and the long Windows jobs run when `main` or a release tag moves, or when someone starts the workflow by hand.

## What runs where

| Event | Jobs |
|---|---|
| Pull request, and a listed path changed | Ubuntu lint and tests, Ubuntu C++ smoke |
| Push to `main` | Those two, plus Ubuntu and Windows benchmarks, the deb, the WiX package, and the M0 Windows harness |
| Tag `v*` or `release*` | Same as `main`. Tags are not path-filtered, so a release always builds packages |
| `workflow_dispatch` | Same as `main` |
| Pull request that only touches docs or other unlisted paths | Nothing |

Feature-branch pushes do not start a run. A pull request used to start two full matrices, one for the branch push and one for the pull request.

Path filter for pull requests: `.github/workflows/**`, `bench/**`, `cpp/**`, `packaging/**`, `pyproject.toml`, `share/**`, `tests/**`, `voxprint_lucent/**`.

Concurrency group is `ci-${{ github.workflow }}-${{ github.ref }}` with cancel-in-progress. A new push to the same branch or pull request stops the previous run.

## Caching

- pip, via `actions/setup-python` and `pyproject.toml`, on lint, smoke, benchmarks, and the harness.
- Qt 6.12 for the Ubuntu smoke job, at `${{ runner.temp }}/lucent-qt`, key `qt-6.12.0-linux-gcc_64-qtbase-icu-v1`. The smoke script skips `aqtinstall` when `libQt6Core` is already there.
- ccache for that gcc/g++ smoke build. The key includes `cpp/**` and the smoke script, with restore prefix `ccache-linux-`. ccache does not apply to the MSVC harness.
- Windows harness: `C:\Qt\6.12.0` and `C:\vcpkg\installed`, `downloads`, and `packages`, key `m0-win-qt-6.12.0-vcpkg-libavif-dav1d-libjxl-v1`. The harness skips the Qt download when `windeployqt.exe` is already present. `vcpkg install` is incremental when those directories are restored. The C++ build directory is not cached.

The 250 ms check is unchanged. The smoke script still warms one `--benchmark` launch and fails the job when `LUCENT_PHASE first_image_presented_ms` is missing or above 250.

## Minute estimates

These are estimates. The only measured figure from this repo is the M0 Windows harness, about 28 minutes of wall clock on a cold `windows-latest` run (56 billed minutes at 2x). The other jobs failed in a few seconds once the spending limit was hit, so their durations are inferred from what they install and compile.

A pull request that matches the path filter:

- Ubuntu lint and tests: about 3–5 minutes.
- Ubuntu C++ smoke: about 8–15 minutes cold (Qt download and a full compile), about 3–6 minutes once the Qt and ccache entries hit.
- No Windows jobs, so no 2x multiplier.

About **10 billed minutes** for a warm pull request, and about **15–20** for a cold one. A docs-only pull request is **0**.

A push to `main`, a release tag, or a manual run, warm caches:

| Job | Wall minutes | Billed |
|---|---|---|
| Ubuntu lint and tests | 4 | 4 |
| Ubuntu C++ smoke | 5 | 5 |
| Ubuntu deb | 1 | 1 |
| Ubuntu benchmark | 5 | 5 |
| Windows benchmark | 6 | 12 |
| Windows WiX package | 3 | 6 |
| M0 Windows harness | 15 | 30 |
| **Total** |  | **about 60** |

Cold, the harness is the measured ~28 minutes of wall clock (56 billed) and the smoke job is about 12, so a cold main push is about **100 billed minutes**. Windows is the whole of that gap.

The previous workflow could bill that heavy set twice per pull-request update, on the order of 80–140 minutes, because every branch push and every pull request each ran Ubuntu and Windows lint, both benchmarks, both packages, the smoke build, and the harness.
