# Suite app discovery

Status: draft spec v1, 2026-10-10 (written by Voxprint AI Lucent; owner of the spec: Voxprint AI Suite Lead). How every Voxprint app registers itself and finds the others, the same way on every platform. App-specific protocols build on it (first one: [IPC-ENLARGE.md](IPC-ENLARGE.md)).

`<App>` = the app name in PascalCase (`Enlarge`, `Lucent`, `Clarity`, `MovieDubber`, `AudiobookBuilder`, `Plotweaver`); `<app>` = kebab-case (`enlarge`, `lucent`, `movie-dubber`, ...). Executable: `voxprint-<app>` (`voxprint-<app>.exe` on Windows).

## 1. Registration (done by each app's installer; other apps only read)

**Windows** (per user, no admin): `HKCU\Software\Voxprint\<App>`

| Value | Type | Example |
|---|---|---|
| `InstallPath` | REG_SZ | `C:\Users\...\AppData\Local\Programs\Voxprint\Enlarge` |
| `Exe` | REG_SZ | `voxprint-enlarge.exe` |
| `Version` | REG_SZ | `0.1.0` |
| `IpcName` | REG_SZ | `voxprint-enlarge` (base name, see section 3; empty if the app has no IPC server) |
| `ProtocolVersion` | REG_DWORD | `1` (highest supported; 0 if no IPC) |

Plus the App Paths key `HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\voxprint-<app>.exe` so `start voxprint-<app>` works; it is also the fallback when the Voxprint key is missing. Uninstall removes both.

**Linux:** `ai.voxprint.<App>.desktop` in `$XDG_DATA_HOME/applications` (default `~/.local/share/applications`) or `$XDG_DATA_DIRS/*/applications`, with the custom keys:

```
X-Voxprint-Ipc=voxprint-<app>
X-Voxprint-Protocol=1
X-Voxprint-Version=0.1.0
```

`Exec=` gives the executable. Fallback: `voxprint-<app>` on `PATH`. Flatpak exports are optional and not planned.

A client verifies that the executable exists before using it (later also the code signer, once releases are signed).

## 2. Capabilities

Every app that can be driven by another app supports `voxprint-<app> --capabilities --json`: one JSON object on stdout, exit 0, **no GPU, network or UI needed** (it reports problems instead of refusing). Common fields:

```json
{
  "protocol": 1,
  "app": "<app>",
  "version": "x.y.z",
  "gpu": {"name": "...", "cc": "8.9", "status": "ok|warn|none", "reason": null},
  "input_formats": ["..."],
  "output_formats": ["..."],
  "default_output_format": "..."
}
```

- `gpu` is present only for apps that use the GPU; `reason` is an open enum (e.g. `no_nvidia`, `driver_too_old`).
- Apps add their own fields (e.g. Enlarge: `models`, `presets`, `max_scale`). Unknown fields are ignored.
- Clients cache the result for a day and refresh when `Version` changes.

## 3. Socket / pipe naming

- Base name: `voxprint-<app>` (the `IpcName` / `X-Voxprint-Ipc` value).
- **Windows:** named pipe `\\.\pipe\<base>-<first 16 hex chars of sha256(user SID)>`, ACL current user only.
- **Linux:** `$XDG_RUNTIME_DIR/voxprint/<app>.sock` (directory 0700, socket 0600).
- Transport: `QLocalServer`/`QLocalSocket`, UTF-8 JSON-lines, every message carries `"protocol": <n>`, first exchange is `hello` in both directions (client/server name, version, supported protocols).
- The same naming serves an app's own single-instance channel (a second launch forwards its arguments to the running instance).
- If nothing is listening, a client may start `voxprint-<app> --serve --hidden` and wait up to 3 s.
- No URI schemes, no COM/D-Bus, no network sockets.

## 4. CLI convention

Apps driven by others offer a CLI fallback that streams the same JSON message objects with `--json-progress` on stdout; exit code 0 on success, non-zero on error.

## 5. Implementation

Each app carries a **small copy of the discovery module** (registry/`.desktop` lookup, PATH fallback, capability probe and cache, socket naming) until a shared `voxprint-common` repo is approved by Aleksandr. Copies must follow this file; changes to the spec go here first, then into every copy.
