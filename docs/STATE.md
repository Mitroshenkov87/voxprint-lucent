# Voxprint AI Lucent: state

- **Suite coordinator:** Voxprint AI Suite Lead. Cross-bot asks go there.

## 2026-10-10
- Phase: **onboarding / planning**. Bot created by Aleksandr; name "Voxprint AI Lucent".
- Research done (viewer research on the box: `voxprint-viewer/RESEARCH.md`), decisions recorded ([DECISIONS.md](DECISIONS.md)), v1 plan drafted ([PLAN-v1.md](PLAN-v1.md)) with M0 = 250 ms cold-start spike.
- Owner updates via Suite Lead: network policy (update check against GitHub releases and user-triggered requests allowed; statistics/diagnostics strictly opt-in, off by default; no default-on telemetry/analytics or background collection without the user's knowledge); Lucent is exempt from the suite RTX 40 GPU rule.
- Lucent section added to `suite/COLLABORATION.md`.
- Avatar set: `bot-avatars/lucent.png` on the box (neon eye/lens in a viewfinder, suite palette; generator `bot-avatars/lucent_logo.py`). App icon sizes (16-1024 + .ico, like `plotweaver/branding/`) follow once the logo is approved.
- Profile title set.
- Enlarge hand-off was proposed in PLAN-v1 section 4 and agreed on 2026-10-10. The contract in this tree is [IPC-ENLARGE.md](IPC-ENLARGE.md) and [APP-DISCOVERY.md](APP-DISCOVERY.md).
- Repo `Mitroshenkov87/voxprint-lucent` exists. M0 is in this tree.
- Suite Lead approved Plan B on 2026-10-10. `voxprint-lucent` is C++/Qt 6. `voxprint-lucent-tools` is the on-demand PySide6 process (settings, tools, info panel, Enlarge UI, updater). Shared theme tokens: voxprint-common `tokens.json` (vendored) and `share/lucent.qss.in`.
- PySide6 missed 250 ms cold. The C++ raster path passed the plan formats on the Linux spike except 12 MP AVIF 4:4:4, which is decode-bound. Details and the independent check are in [M0-RESULTS.md](M0-RESULTS.md).
- The owner's confirmation run is `bench/Run-M0.ps1` against the Windows harness zip from CI. It does not block M1.
- M1 core viewer is in the tree. Details and the remaining gaps are in [M1.md](M1.md). `--benchmark` is still the M0 raster path.
- M2 tools and the info panel are in the tools process. Details are in [M2.md](M2.md). `--benchmark` does not start that process.
- M3 Enlarge client shares that process (`--enlarge --path`). `I` opens the info panel. It follows the agreed [IPC-ENLARGE.md](IPC-ENLARGE.md) and [APP-DISCOVERY.md](APP-DISCOVERY.md). Details are in [M3.md](M3.md).

## Next
1. Owner runs `bench/Run-M0.ps1` on the reference machine when that session exists. CI's 250 ms check is a warm in-process guard, not that cold sample.
2. Point the Enlarge client at the real server when that process exists. The mock remains the CI stand-in.
3. `v0.1.0-rc1` is the keyboard-only build. `v0.1.0-rc2` adds the viewer chrome. Notes are in [release-notes/v0.1.0-rc2.md](release-notes/v0.1.0-rc2.md).
4. `v0.1.0-rc3` ("Zohar") replaces the save icons, reads the build label from `BUILD.json`, calls Enlarge through its CLI, and adds `--dry-run`. Notes are in [release-notes/v0.1.0-rc3.md](release-notes/v0.1.0-rc3.md).
5. `v0.1.0-rc4` ("Ner") is the docs site, the Enlarge v0.2.0-rc.1 hand-off, and the measurement zip's hard images. Notes are in [release-notes/v0.1.0-rc4.md](release-notes/v0.1.0-rc4.md).
6. `v0.1.0-rc5` ("Yashar") finds an installed Enlarge, tone-maps PQ and HLG for an SDR screen, and checks the image readers at startup. Notes are in [release-notes/v0.1.0-rc5.md](release-notes/v0.1.0-rc5.md).
7. `v0.1.0-rc6` ("Tov") rolls PQ highlights off with BT.2390, and the cold-start task logs its outcome. Notes are in [release-notes/v0.1.0-rc6.md](release-notes/v0.1.0-rc6.md).
