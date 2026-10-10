# Enlarge hand-off, protocol 1

Lucent is the client. Enlarge is the server. This file is the contract the client and the mock server implement.

`suite/IPC-ENLARGE.md` in [project-notes](https://github.com/Mitroshenkov87/project-notes) was not readable from this checkout: the GitHub API returned 404. The text below is PLAN-v1 section 4, plus the two additions asked for with this milestone: the error code `model_missing`, and `gpu.status` of `none` with a `reason` in `--capabilities --json`. When the shared file is available, reconcile this document with it.

Every message is one UTF-8 JSON object per line and carries `"protocol": 1`. Unknown fields are ignored. A major change bumps `protocol`.

## Discovery

Enlarge registers itself on install. Lucent only reads.

- Windows: `HKCU\Software\Voxprint\Enlarge` values `Exe` and `ProtocolVersion`. Protocol 1 uses that exe.
- Linux: `ai.voxprint.Enlarge.desktop` under `$XDG_DATA_HOME` or `$XDG_DATA_DIRS` (`applications/`), with `X-Voxprint-Protocol=1` and an `Exec=` line. `%` placeholders are dropped.
- Override for tests and local mocks: `LUCENT_ENLARGE_EXE` (a command line, for example `python -m voxprint_lucent.enlarge_mock`). Otherwise `voxprint-enlarge` on `PATH`.

Flatpak is recognized when its exported desktop file is on the data path. This client does not probe `flatpak run` on its own.

## Capabilities

`voxprint-enlarge --capabilities --json` prints one JSON object and exits 0. Lucent caches nothing in this milestone; it probes when the Enlarge window opens.

`gpu.status` is `ok`, `warn`, or `none`. `none` includes a `reason`. Lucent hides Quick enlarge and Open in Enlarge when the program is missing or `gpu.status` is `none`. `warn` (the below-RTX-40 case) stays visible and the reason is the button tooltip.

The mock defaults to `gpu.status` `none` and the reason `No usable GPU is available.` `LUCENT_ENLARGE_GPU=ok` or `warn` changes the mock probe.

## Transport

`QLocalSocket` / `QLocalServer`.

- Windows pipe name: `voxprint-enlarge-` plus the first 16 hex characters of SHA-256 of the user SID. Qt serves it as `\\.\pipe\<name>` with the current-user ACL the server creates.
- Linux: `$XDG_RUNTIME_DIR/voxprint/enlarge.sock`. The directory is `0700` and the socket is `0600`. If `XDG_RUNTIME_DIR` is unset, the mock and the client use `/tmp/voxprint-<uid>/voxprint/enlarge.sock`.
- Tests set `LUCENT_ENLARGE_SOCKET` so they never touch the real socket.

If nothing is listening, Lucent starts `voxprint-enlarge --serve --hidden` and waits up to 3 seconds. If that fails, Lucent falls back to the CLI.

## Messages

Client to server:

- `hello` with `client` and `version`.
- `enlarge.request` with `id`, `input`, `scale`, `preset`, `format`, `output`, `output_dir`, `open_ui`.
- `cancel` with `id`.

Server to client:

- `hello` with `server`, `version`, `protocol_min`, `protocol_max`.
- `accepted` with `queue_pos`.
- `progress` with `pct`, `stage`, and optional `eta_s`.
- `done` with `output`, `width`, `height`, `format`, `ms`.
- `error` with `code`, `message`, `retryable`.

Error codes: `gpu_unsupported`, `gpu_busy`, `oom`, `bad_input`, `unsupported_format`, `io_error`, `cancelled`, `internal`, `model_missing`.

`open_ui` false is Quick enlarge. `open_ui` true is Open in Enlarge. Both finish with a `done` path when the job succeeds, and Lucent opens the compare view on that path.

The default output is `<name>.enlarged-<scale>x.avif` beside the source. PNG is never an output. An existing file is not overwritten; the name gains `-2`, `-3`, and so on. The source file is never the output.

## CLI fallback

```
voxprint-enlarge --input FILE --scale 2 --preset photo [--format avif] [--output FILE] --json-progress
```

The same `accepted`, `progress`, `done`, and `error` objects are written to stdout. Exit 0 on `done`, non-zero on `error`. Cancelling a CLI run stops the process; the socket `cancel` message is the interactive cancel.

## Mock

`python -m voxprint_lucent.enlarge_mock` is not Voxprint AI Enlarge. It implements this contract so CI can run before the server exists.

- An input whose name contains `missing-model` returns `model_missing` with `retryable` false.
- An input whose name contains `cancel-me` accepts the job, sends progress, and waits for `cancel` on the socket. The CLI prints `cancelled` instead of waiting.
- Any other readable image is nearest-neighbour scaled and saved as AVIF, JPEG XL, or WebP.
- `LUCENT_ENLARGE_MOCK_SERVE_FAIL=1` makes `--serve` exit 1 so the client can prove the CLI fallback.
