# Voxprint AI Lucent: state

- **Suite coordinator:** Voxprint AI Suite Lead (bot `c0ed1455-a4b5-4f8f-8e87-24a072f7f972`). Cross-bot asks go there.

## 2026-10-10
- Phase: **onboarding / planning**. Bot created by Aleksandr; name "Voxprint AI Lucent".
- Research done (viewer research on the box: `voxprint-viewer/RESEARCH.md`), decisions recorded ([DECISIONS.md](DECISIONS.md)), v1 plan drafted ([PLAN-v1.md](PLAN-v1.md)) with M0 = 250 ms cold-start spike.
- Owner updates via Suite Lead: network policy (update check against GitHub releases and user-triggered requests allowed; statistics/diagnostics strictly opt-in, off by default; no default-on telemetry/analytics or background collection without the user's knowledge); Lucent is exempt from the suite RTX 40 GPU rule.
- Lucent section added to `suite/COLLABORATION.md`.
- Avatar set: `bot-avatars/lucent.png` on the box (neon eye/lens in a viewfinder, suite palette; generator `bot-avatars/lucent_logo.py`). App icon sizes (16-1024 + .ico, like `plotweaver/branding/`) follow once the logo is approved.
- Profile title set.
- Enlarge hand-off protocol proposal sent to Voxprint AI Enlarge (PLAN-v1 section 4); awaiting reply.
- Repo `Mitroshenkov87/voxprint-lucent` exists. M0 is in this tree.
- Suite Lead approved Plan B on 2026-10-10. `voxprint-lucent` is C++/Qt 6. `voxprint-lucent-tools` is the on-demand PySide6 process (settings, tools, info panel, Enlarge UI, updater). Shared theme tokens: `share/palette.json` and `share/lucent.qss.in`.
- PySide6 missed 250 ms cold. The C++ raster path passed the plan formats on the Linux spike except 12 MP AVIF 4:4:4, which is decode-bound. Details and the independent check are in [M0-RESULTS.md](M0-RESULTS.md).
- The owner's confirmation run is `bench/Run-M0.ps1` against the Windows harness zip from CI. It does not block M1.
- M1 core viewer is in the tree. Details and the remaining gaps are in [M1.md](M1.md). `--benchmark` is still the M0 raster path.

## Next
1. Owner runs `bench/Run-M0.ps1` on the reference machine when that session exists. CI's 250 ms check is a warm in-process guard, not that cold sample.
2. M2: info panel, tools, and the Enlarge hand-off once Enlarge replies.
3. Settle the Enlarge hand-off with Voxprint AI Enlarge (PLAN-v1 section 4), then move it to a shared spec.
