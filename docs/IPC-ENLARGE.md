# Suite IPC: Enlarge hand-off, protocol 1

Status: **agreed** 2026-10-10 between Voxprint AI Lucent (client) and Voxprint AI Enlarge (server). Server-side owner: **Voxprint AI Enlarge**. First client: **Voxprint AI Lucent**. Other apps may become clients without changes to this spec.

App discovery (registry keys, `.desktop` entries, PATH fallback, common capability fields, socket naming) is defined once for all apps in [APP-DISCOVERY.md](APP-DISCOVERY.md). This file adds what is specific to Enlarge.

Change rules: unknown fields are ignored by both sides; adding optional fields or enum values does not bump the protocol; any breaking change bumps `protocol` and both sides list the versions they support in `hello`.

## 1. Discovery

- **Windows:** `HKCU\Software\Voxprint\Enlarge` -> `InstallPath`, `Exe`, `Version`, `IpcName`, `ProtocolVersion`; fallback App Paths `voxprint-enlarge.exe`.
- **Linux:** `ai.voxprint.Enlarge.desktop` in `$XDG_DATA_HOME/applications` or `$XDG_DATA_DIRS/*/applications` with `X-Voxprint-Ipc=` and `X-Voxprint-Protocol=1`; fallback `voxprint-enlarge` on `PATH`. Flatpak is optional and not planned.
- Clients cache the capabilities result for a day and refresh it when `Version` changes.

## 2. Capabilities

`voxprint-enlarge --capabilities --json` prints one JSON object on stdout and exits 0. **It works without a GPU**: it never refuses, it reports the GPU state instead. Only the UI start and `--serve` refuse to run without a usable GPU.

```json
{
  "protocol": 1,
  "app": "enlarge",
  "version": "x.y.z",
  "gpu": {"name": "NVIDIA GeForce RTX 4090 Laptop GPU", "cc": "8.9", "status": "ok", "reason": null},
  "models": [
    {"id": "nomos2-realplksr", "preset": "photo", "quality": "fast", "scales": [2, 4], "installed": true},
    {"id": "nomos2-dat2", "preset": "photo", "quality": "max", "scales": [2, 4], "installed": false},
    {"id": "realesrgan-anime6b", "preset": "art", "scales": [4], "installed": true}
  ],
  "presets": ["photo", "art"],
  "max_scale": 4,
  "input_formats": ["avif", "jxl", "webp", "png", "jpeg", "heic", "tiff"],
  "output_formats": ["avif", "jxl", "webp", "jpeg", "heic"],
  "default_output_format": "avif"
}
```

- `gpu.status`: `ok` (RTX 40 / CC 8.9+), `warn` (usable but below the suite minimum; Enlarge warns, does not block), `none` (no usable GPU).
- `gpu.reason`: `null` when `ok`; with `none` it is `no_nvidia` or `driver_too_old` (NVIDIA driver 600+ required). Open enum.
- Model ids above are illustrative; the registry is the source of truth. `installed: false` means the weights are not downloaded.
- Client behaviour (Lucent): `ok` -> action enabled; `warn` -> enabled with Enlarge's warning in the tooltip; `none` or Enlarge absent -> action hidden or disabled with the reason.

## 3. Transport

- `QLocalServer` / `QLocalSocket`.
- **Windows:** named pipe `\\.\pipe\voxprint-enlarge-<first 16 hex chars of sha256(user SID)>`, ACL current user only.
- **Linux:** `$XDG_RUNTIME_DIR/voxprint/enlarge.sock` (directory 0700, socket 0600).
- UTF-8 JSON-lines: one object per line, `\n` terminated. Every message carries `"protocol": 1`.
- **Launch:** if nothing is listening, the client starts `voxprint-enlarge --serve --hidden` and waits up to 3 s for the socket.

## 4. Messages

Client -> server:

```json
{"protocol":1,"type":"hello","client":"lucent","version":"x.y.z"}
{"protocol":1,"type":"enlarge.request","id":"<uuid>","input":"<absolute path>","scale":2,"preset":"photo","quality":"fast","model":null,"format":"avif","output":null,"output_dir":null,"open_ui":false}
{"protocol":1,"type":"cancel","id":"<uuid>"}
```

Server -> client:

```json
{"protocol":1,"type":"hello","server":"enlarge","version":"x.y.z","protocols":[1]}
{"protocol":1,"type":"accepted","id":"<uuid>","queue_pos":0}
{"protocol":1,"type":"progress","id":"<uuid>","pct":42,"stage":"tiles","eta_s":12}
{"protocol":1,"type":"done","id":"<uuid>","output":"<absolute path>","width":7680,"height":4320,"format":"avif","ms":5120}
{"protocol":1,"type":"error","id":"<uuid>","code":"model_missing","message":"...","retryable":true}
```

Request fields:
- `input` (required): absolute path. Files are passed by path, never pixels.
- `scale`: 2 or 4 (see `max_scale` and per-model `scales`).
- `preset`: open enum; MVP values `photo`, `art` (`face` is not in the MVP).
- `quality` (optional): `fast` | `max`. Mapping in MVP: photo fast = Nomos2 RealPLKSR, photo max = Nomos2 DAT2, art = Real-ESRGAN anime6B.
- `model` (optional): a registry model id; **overrides `preset`/`quality`** when set.
- `format` (optional): one of `output_formats`; default `avif`. Never PNG.
- `output` / `output_dir` (optional): explicit file, or a directory for the default name; both `null` = next to the source.
- `open_ui`: `false` = headless ("Quick enlarge"); `true` = Enlarge window opens with the image loaded, and the result still comes back if the client is still connected.

Progress `stage` values (open enum): `queued`, `waiting_gpu` (waiting for the suite GPU lock, which Enlarge holds per job), `loading`, `tiles`, `encoding`.

Error codes (open enum):

| Code | Meaning | Retryable |
|---|---|---|
| `gpu_unsupported` | no usable GPU / driver too old | no |
| `gpu_busy` | GPU lock not obtained in time | yes |
| `oom` | out of VRAM/RAM even at minimum tile size | no |
| `bad_input` | unreadable or corrupt input | no |
| `unsupported_format` | input or requested output format not supported | no |
| `model_missing` | model weights not downloaded | yes |
| `io_error` | cannot write output | no |
| `cancelled` | cancelled by the client | no |
| `internal` | anything else | no |

**`model_missing`:** with `open_ui:false` the server returns this error instead of downloading silently. The user starts the download in the Enlarge UI (the client may offer "Open in Enlarge..." which sends the same request with `open_ui:true`), then retries.

## 5. Output naming

Default: `<name>.enlarged-<scale>x.<ext>` next to the source (or in `output_dir`), where `<ext>` follows `format` (`avif`, `jxl`, `webp`, `jpg`, `heic`). If the name exists, a numeric suffix is added (`<name>.enlarged-2x-1.avif`, `-2`, ...). The server **never overwrites** an existing file, including an explicit `output`.

## 6. CLI fallback

For scripts, file managers, or when the socket fails:

```
voxprint-enlarge --input <file> --scale 2 --preset photo [--quality fast|max] [--model <id>] [--format avif] [--output <file>] --json-progress
```

Streams the same `accepted` / `progress` / `done` / `error` objects (JSON-lines) on stdout. Exit code 0 on success, non-zero on error. Same output naming and no-overwrite rule as section 5.
