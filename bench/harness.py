"""Measure launch-to-first-painted-pixel.

Cold on this harness is an approximation: drop the OS page cache when permitted,
otherwise ``posix_fadvise`` the image. A real cold number is one run after reboot
on the reference laptop. CI virtual machines are indicative only.

Pass rule (PLAN-v1): on the reference laptop, cold median <= 250 ms and cached
next/previous <= 30 ms. ``--reference-laptop`` is how that machine records a pass
or a fail. Every other machine is reported as indicative.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path
from queue import Empty, Queue
from threading import Thread

from bench.fixtures import ensure_fixtures
from bench.stats import evaluate, summarise

FIRST_PIXEL_BUDGET_MS = 250
NAVIGATE_BUDGET_MS = 30


def _fadvise(path: Path) -> None:
    if not hasattr(os, "posix_fadvise"):
        return
    fd = os.open(path, os.O_RDONLY)
    try:
        os.posix_fadvise(fd, 0, 0, os.POSIX_FADV_DONTNEED)
    finally:
        os.close(fd)


def _drop_caches() -> bool:
    proc = subprocess.run(["sync"], check=False)
    if proc.returncode != 0:
        return False
    try:
        Path("/proc/sys/vm/drop_caches").write_text("3\n", encoding="utf-8")
    except OSError:
        return False
    return True


def _prepare_cold(image: Path) -> str:
    if _drop_caches():
        return "drop_caches"
    _fadvise(image)
    _fadvise(Path(sys.executable))
    return "fadvise"


def measure_once(
    command: list[str],
    image: Path,
    neighbour: Path | None,
    *,
    timeout_s: float,
) -> dict[str, float]:
    cmd = [*command, "--benchmark", str(image)]
    if neighbour is not None:
        cmd.extend(["--navigate-next", str(neighbour)])
    lines: Queue[tuple[float, str]] = Queue()

    def _reader(stream) -> None:  # type: ignore[no-untyped-def]
        for line in stream:
            lines.put((time.perf_counter(), line))

    started = time.perf_counter()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    assert proc.stdout is not None
    thread = Thread(target=_reader, args=(proc.stdout,), daemon=True)
    thread.start()
    first_ms: float | None = None
    navigate_ms: float | None = None
    deadline = started + timeout_s
    while time.perf_counter() < deadline:
        try:
            stamp, line = lines.get(timeout=0.05)
        except Empty:
            if proc.poll() is not None and lines.empty():
                break
            continue
        if line.startswith("FIRST_PIXEL") and first_ms is None:
            first_ms = (stamp - started) * 1000
            if neighbour is None:
                break
        elif line.startswith("NAVIGATE_MS"):
            navigate_ms = float(line.split()[1])
            break
    proc.wait(timeout=10)
    stderr = ""
    if proc.stderr is not None:
        stderr = proc.stderr.read()
    if proc.returncode != 0 or first_ms is None:
        raise RuntimeError(
            f"viewer exited {proc.returncode} without FIRST_PIXEL\n{stderr}".rstrip()
        )
    result = {"first_pixel_ms": first_ms}
    if navigate_ms is not None:
        result["navigate_ms"] = navigate_ms
    return result


def _run_profile(
    *,
    name: str,
    runs: int,
    command: list[str],
    image: Path,
    neighbour: Path | None,
    timeout_s: float,
    warmup: bool,
) -> dict[str, object]:
    if warmup:
        measure_once(command, image, neighbour, timeout_s=timeout_s)
    method = "warm"
    samples: list[float] = []
    navigations: list[float] = []
    for _ in range(runs):
        if name == "cold":
            method = _prepare_cold(image)
        sample = measure_once(command, image, neighbour, timeout_s=timeout_s)
        samples.append(sample["first_pixel_ms"])
        if "navigate_ms" in sample:
            navigations.append(sample["navigate_ms"])
    payload: dict[str, object] = {
        "profile": name,
        "cache_method": method,
        "first_pixel": summarise(samples),
        "samples_ms": [round(value, 3) for value in samples],
    }
    if navigations:
        payload["navigate"] = summarise(navigations)
        payload["navigate_samples_ms"] = [round(value, 3) for value in navigations]
    return payload


def run_benchmark(args: argparse.Namespace) -> dict[str, object]:
    fixtures = ensure_fixtures(Path(args.fixtures))
    command = [sys.executable, "-m", "voxprint_lucent"]
    if args.executable:
        command = [args.executable]
    formats = ("jpeg", "avif", "jxl", "webp")
    profiles = [part.strip() for part in args.profiles.split(",") if part.strip()]
    per_format: dict[str, object] = {}
    cold_medians: list[float] = []
    for fmt in formats:
        image = fixtures[fmt]
        neighbour = fixtures["jpeg_next"] if fmt == "jpeg" else None
        per_format[fmt] = {
            profile: _run_profile(
                name=profile,
                runs=args.runs,
                command=command,
                image=image,
                neighbour=neighbour,
                timeout_s=args.timeout,
                warmup=profile == "warm",
            )
            for profile in profiles
        }
        cold = per_format[fmt].get("cold")
        if isinstance(cold, dict):
            first = cold.get("first_pixel")
            if isinstance(first, dict) and "median_ms" in first:
                cold_medians.append(float(first["median_ms"]))
    jpeg_nav = None
    jpeg_payload = per_format.get("jpeg")
    if isinstance(jpeg_payload, dict):
        warm = jpeg_payload.get("warm")
        if isinstance(warm, dict) and isinstance(warm.get("navigate"), dict):
            jpeg_nav = float(warm["navigate"]["median_ms"])
    cold_median = median_of(cold_medians)
    verdict = evaluate(
        cold_median_ms=cold_median,
        navigate_median_ms=jpeg_nav,
        reference_laptop=bool(args.reference_laptop),
    )
    return {
        "target_first_pixel_ms": FIRST_PIXEL_BUDGET_MS,
        "target_navigate_ms": NAVIGATE_BUDGET_MS,
        "runs": args.runs,
        "reference_laptop": bool(args.reference_laptop),
        "verdict": verdict,
        "cold_median_across_formats_ms": cold_median,
        "jpeg_navigate_median_ms": jpeg_nav,
        "formats": per_format,
        "note": (
            "Pass/fail applies only with --reference-laptop "
            "(RTX 4090 Laptop, Windows 11 24H2, Nuitka onedir). "
            "CI and other machines are indicative."
        ),
    }


def median_of(values: list[float]) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    mid = len(ordered) // 2
    if len(ordered) % 2 == 1:
        return ordered[mid]
    return (ordered[mid - 1] + ordered[mid]) / 2


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="bench.harness", description=__doc__)
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--profiles", default="warm,cold")
    parser.add_argument("--fixtures", type=Path, default=Path("bench/fixtures"))
    parser.add_argument("--json-out", type=Path, default=Path("bench/results/latest.json"))
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--executable", default=None, help="Nuitka onedir binary instead of Python")
    parser.add_argument(
        "--reference-laptop",
        action="store_true",
        help="Record a pass/fail against the 250 ms rule. Omit on CI and other machines.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        report = run_benchmark(args)
    except (RuntimeError, OSError) as exc:
        print(exc, file=sys.stderr)
        return 1
    text = json.dumps(report, indent=2)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(text + "\n", encoding="utf-8")
    print(text)
    print(
        f"verdict={report['verdict']} cold_median_ms={report['cold_median_across_formats_ms']} "
        f"navigate_median_ms={report['jpeg_navigate_median_ms']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
