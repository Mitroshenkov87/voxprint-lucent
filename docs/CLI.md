# voxprint-lucent command line

The viewer binary is `voxprint-lucent`. These two flags are handled before any window or GPU work. `--benchmark` is unchanged and still constructs only `Viewer`.

## `--version`

`voxprint-lucent --version` prints one line and exits 0:

```text
0.1.0 RC · build N "Zohar"
```

`N` is `GITHUB_RUN_NUMBER` plus the `offset` in `BUILD.json`. When that variable is unset, the run number is 0, so `N` is the offset. The line is the only output. No window is created. If `--version` is combined with `--dry-run`, `--benchmark`, or an image path, `--version` wins and the other arguments are ignored.

## `--dry-run`

`voxprint-lucent --dry-run [IMAGE]` starts `QCoreApplication` only. It does not create a window, does not touch the RHI, does not need a display, does not open Lucent's single-instance socket, and does not start `voxprint-enlarge --serve`. It prints one JSON object on stdout and does not present a first pixel.

```json
{"dry_run":true,"version":"0.1.0 RC · build N \"Zohar\"","startup":"ok","image":null,"enlarge":{"transport":"cli","installed":false,"install_url":"https://github.com/Mitroshenkov87/voxprint-enlarge/releases"}}
```

- No image argument: `startup` is `ok` and `image` is null. Exit 0 when Enlarge is absent or its capabilities probe exits 0.
- Image path exists: `startup` is `ok`, `image` is that path.
- Image path does not exist: `startup` is `missing_image`, exit 3. Enlarge may still be probed. The process does not run an enlarge job.
- `--dry-run` together with `--benchmark`: dry-run wins. `Viewer` is not constructed, and `FIRST_PIXEL` is not printed.

Enlarge discovery is the same as the app: `LUCENT_ENLARGE_EXE` when that program exists, otherwise `voxprint-enlarge` on `PATH`.

When Enlarge is absent, `enlarge.installed` is false and `enlarge.install_url` is `https://github.com/Mitroshenkov87/voxprint-enlarge/releases`. Exit 0 unless the image is missing.

When Enlarge is present, Lucent runs `voxprint-enlarge --capabilities --json` (this works without a GPU). If an image was given and that probe exits 0, Lucent then runs:

```text
voxprint-enlarge --input IMAGE --scale 2 --preset photo --format avif --json-progress --dry-run
```

The capabilities object is `enlarge.capabilities`. The job's JSON lines are `enlarge.lines`. A non-zero capabilities exit, or a non-zero job exit, becomes Lucent's exit code. A missing image still exits 3.

`--dry-run` on the Enlarge CLI skips the GPU refusal and does not write the output file. `done.output` is the path that would have been written. Missing-model and other validation errors still apply.
