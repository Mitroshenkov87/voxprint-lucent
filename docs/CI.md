# CI minutes

While this repository is private, GitHub Actions bills Ubuntu minutes at 1x and Windows minutes at 2x. The free monthly allowance is the budget. The owner decides about billing. This workflow is arranged so a pull request stays inside a small Ubuntu slice, and the long Windows jobs run when `main` moves or when someone starts the workflow by hand. A version tag builds the release candidate instead of repeating that matrix.

## What runs where

| Event | Jobs |
|---|---|
| Pull request, and a listed path changed | Ubuntu lint and tests, Ubuntu C++ smoke, Qt 6.8 compile in a Debian 13 container |
| Push to `main` | Those jobs, plus the docs site (built and published to GitHub Pages), Ubuntu and Windows benchmarks, the skeleton deb, the skeleton WiX package, and the M0 Windows harness |
| Tag `v*` | [Release](../.github/workflows/release.yml) only: the Windows MSI, the measurement zip, and the Linux package, published as a GitHub release. A tag that contains `-` (such as `v0.1.0-rc1`) is a pre-release. The CI matrix does not also run, so the tag is not billed twice |
| `workflow_dispatch` | Same as a push to `main` |
| Pull request that only touches docs or other unlisted paths | Nothing |

Feature-branch pushes do not start a run. A pull request used to start two full matrices, one for the branch push and one for the pull request.

Path filter for pull requests: `.github/workflows/**`, `bench/**`, `cpp/**`, `packaging/**`, `pyproject.toml`, `scripts/**`, `share/**`, `tests/**`, `voxprint_lucent/**`.

Concurrency group is `ci-${{ github.workflow }}-${{ github.ref }}` with cancel-in-progress. A new push to the same branch or pull request stops the previous run.

## Caching

- pip, via `actions/setup-python` and `pyproject.toml`, on lint, smoke, benchmarks, and the harness.
- Qt 6.12 for the Ubuntu smoke job, at `${{ runner.temp }}/lucent-qt`, key `qt-6.12.0-linux-gcc_64-qtbase-icu-v1`. The smoke script skips `aqtinstall` when `libQt6Core` is already there.
- ccache for that gcc/g++ smoke build. The key includes `cpp/**` and the smoke script, with restore prefix `ccache-linux-`. ccache does not apply to the MSVC harness.
- Windows harness: `C:\Qt\6.12.0` and `C:\vcpkg\installed`, `downloads`, and `packages`, key `m0-win-qt-6.12.0-vcpkg-libavif-dav1d-libjxl-v1`. The harness skips the Qt download when `windeployqt.exe` is already present. `vcpkg install` is incremental when those directories are restored. The C++ build directory is not cached.

The smoke script warms one `--benchmark` launch of a tiny JPEG and fails the job when `LUCENT_PHASE first_image_presented_ms` is missing or above 250. That file has no embedded preview, so the check is the full decode. It is the warm gate, not a cold-boot sample. Accepted gates for v0.1.x (owner, 2026-10-12): a cold first pixel is at most 450 ms, warm JPEG and WebP stay at 250 ms, and a warm full-resolution decode of a 12 megapixel or larger AVIF or JPEG XL is at most 350 ms. Cached next/previous stays 30 ms. `bench/Run-M0.ps1` writes `cold_gate` against 450 ms and `warm_gate` against 350 ms for AVIF and JPEG XL and against 250 ms for JPEG and WebP. CI does not encode a 12 megapixel fixture.

A preview painted before the full decode was measured once. libavif 1.0.4 documents `AVIF_DECODER_SOURCE_THUMBNAIL_ITEM` as unimplemented. The 4000×3000 `large.jxl` fixture reports `have_preview` 0. The matching JPEG has no Exif APP1 thumbnail. `avifCodecName(AVIF_CODEC_CHOICE_AUTO)` is already dav1d. One decode of that AVIF with 4 threads: AUTO 184.6 ms, explicit dav1d 181.3 ms, libgav1 263.2 ms, aom 352.9 ms. Naming dav1d is noise, and frame threads past `maxThreads` are not a decoder switch. The decode path is unchanged. The rc6 reference numbers (AVIF warm median 296 ms, JPEG XL 253 ms, p95 338 ms) sit inside 350 ms. They were not remeasured on the test machine.

After that measurement the smoke script runs `--version` and `--dry-run`, including a stand-in Enlarge CLI, so the hand-off is exercised without a GPU. It then runs `voxprint-lucent --help` and fails if `docs/CLI.md` is not the page that command generates.

The lint job also runs Doxygen with `WARN_AS_ERROR` (public headers only) and `mkdocs build --strict`. Ruff's `D1` rules require a docstring on every public Python module, class, and function, using the Google convention. Tests and `bench/` are excluded. Those checks stay on the pull-request lint job. Publishing the site is a separate job that runs only when `main` moves or someone starts the workflow by hand, and only after lint and the C++ smoke succeed. It uses `actions/deploy-pages`. A docs-only pull request still starts no runner.

The Qt 6.8 job compiles the viewer inside `debian:trixie` with `qt6-base-dev` and `qt6-base-private-dev`. CMake links `Qt6::GuiPrivate` only when that target's include directories exist; otherwise it adds the directory that contains `rhi/qrhi.h`. The job does not run the first-visible-pixel check. That check stays on the Qt 6.12 smoke job.

## Minute estimates

These are estimates. The only measured figure from this repo is the M0 Windows harness, about 28 minutes of wall clock on a cold `windows-latest` run (56 billed minutes at 2x). The other jobs failed in a few seconds once the spending limit was hit, so their durations are inferred from what they install and compile.

A pull request that matches the path filter:

- Ubuntu lint and tests: about 3–5 minutes.
- Ubuntu C++ smoke: about 8–15 minutes cold (Qt download and a full compile), about 3–6 minutes once the Qt and ccache entries hit.
- Debian 13 Qt 6.8 compile: about 3–8 minutes, mostly the image pull and `apt-get`.
- No Windows jobs, so no 2x multiplier.

About **10 billed minutes** for a warm pull request, and about **15–20** for a cold one. Doxygen and the MkDocs build add about a minute to the lint job. A docs-only pull request is **0**. The Pages publish job does not run on a pull request.

A push to `main` or a manual run, warm caches:

| Job | Wall minutes | Billed |
|---|---|---|
| Ubuntu lint and tests | 5 | 5 |
| Docs site (GitHub Pages) | 4 | 4 |
| Ubuntu C++ smoke | 5 | 5 |
| Debian 13 Qt 6.8 compile | 6 | 6 |
| Ubuntu deb | 1 | 1 |
| Ubuntu benchmark | 5 | 5 |
| Windows benchmark | 6 | 12 |
| Windows WiX package | 3 | 6 |
| M0 Windows harness | 15 | 30 |
| **Total** |  | **about 71** |

Cold, the harness is the measured ~28 minutes of wall clock (56 billed) and the smoke job is about 12, so a cold main push is about **100 billed minutes**. Windows is the whole of that gap.

The previous workflow could bill that heavy set twice per pull-request update, on the order of 80–140 minutes, because every branch push and every pull request each ran Ubuntu and Windows lint, both benchmarks, both packages, the smoke build, and the harness.

A `v*` tag runs the release workflow instead of this matrix. Warm, that is about one Windows harness (about 15 wall minutes, 30 billed, once Qt and vcpkg are cached) plus one Ubuntu package build (about 10–15 billed, dominated by the Qt SDK and the viewer compile). Cold, use the measured harness figure of about 28 wall minutes (56 billed) plus about 15 for Ubuntu. The publish job is about 1 minute.
