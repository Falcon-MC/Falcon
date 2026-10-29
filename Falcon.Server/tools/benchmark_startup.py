#!/usr/bin/env python3
"""Measure a dedicated server from process launch until 'Server started.'."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import queue
import socket
import statistics
import subprocess
import tempfile
import threading
import time


def command_output(arguments):
    try:
        return subprocess.check_output(arguments, text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def measure(binary, directory, timeout):
    lines = queue.Queue()
    output = []
    started = time.perf_counter()
    process = subprocess.Popen(
        [str(binary), "--no-wizard"], cwd=directory,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, encoding="utf-8", errors="replace",
    )

    def read_output():
        for line in process.stdout:
            lines.put((time.perf_counter(), line))
        lines.put((time.perf_counter(), None))

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    try:
        deadline = started + timeout
        while True:
            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                raise RuntimeError("Startup timed out.\n" + "".join(output[-30:]))
            try:
                timestamp, line = lines.get(timeout=remaining)
            except queue.Empty:
                raise RuntimeError("Startup timed out.\n" + "".join(output[-30:])) from None
            if line is None:
                raise RuntimeError("Server exited before readiness.\n" + "".join(output[-30:]))
            output.append(line)
            if "Server started." in line:
                elapsed = timestamp - started
                break
        process.stdin.write("stop\n")
        process.stdin.flush()
        process.wait(timeout=timeout)
        reader.join(timeout=timeout)
        if process.returncode != 0:
            raise RuntimeError("Server exited with code " + str(process.returncode))
        return elapsed
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        reader.join(timeout=1)
        process.stdin.close()
        process.stdout.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--before", type=Path, help="Alternate runs against this baseline executable")
    parser.add_argument("--runs", type=int, default=15)
    parser.add_argument("--fresh-runs", type=int, default=5)
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--label", default="startup")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 1 or args.fresh_runs < 1 or args.timeout <= 0:
        parser.error("runs and timeout must be positive")
    binary = args.binary.resolve(strict=True)
    binaries = {args.label: binary} if args.before is None else {"before": args.before.resolve(strict=True), "after": binary}
    repository = Path(__file__).resolve().parents[2]
    results = {name: {"fresh": [], "existing": []} for name in binaries}

    def order(index):
        names = list(binaries)
        return names if index % 2 == 0 else names[::-1]

    with tempfile.TemporaryDirectory(prefix="falcon-startup-") as temporary:
        # Isolate all databases, packs and configuration from the user's server.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as address:
            address.bind(("127.0.0.1", 0))
            port = address.getsockname()[1]
        properties = (
            f"server-port={port}\nserver-portv6={port}\n"
            "level-name=benchmark\nlevel-seed=12345\nenable-lan-visibility=false\n"
        )
        directories = {}
        for index in range(args.fresh_runs):
            for name in order(index):
                directory = Path(temporary, f"{name}-{index}")
                directory.mkdir()
                Path(directory, "server.properties").write_text(properties, encoding="utf-8")
                directories[name] = directory
                seconds = measure(binaries[name], directory, args.timeout)
                results[name]["fresh"].append(seconds)
                print(f"{name} fresh {index + 1}/{args.fresh_runs}: {seconds:.6f}s", flush=True)
        for name in binaries:
            measure(binaries[name], directories[name], args.timeout)
        for index in range(args.runs):
            for name in order(index):
                seconds = measure(binaries[name], directories[name], args.timeout)
                results[name]["existing"].append(seconds)
                print(f"{name} {index + 1}/{args.runs}: {seconds:.6f}s", flush=True)

    variants = {}
    for name, executable in binaries.items():
        samples = results[name]["existing"]
        cache = executable.parent.parent / "CMakeCache.txt"
        build_type = None
        if cache.is_file():
            for line in cache.read_text(encoding="utf-8").splitlines():
                if line.startswith("CMAKE_BUILD_TYPE:STRING="):
                    build_type = line.split("=", 1)[1]
        variants[name] = {
            "binary": str(executable), "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "build_type": build_type,
            "fresh_world_seconds": statistics.median(results[name]["fresh"]),
            "fresh_world_samples_seconds": results[name]["fresh"], "existing_world_samples_seconds": samples,
            "median_seconds": statistics.median(samples), "min_seconds": min(samples),
            "max_seconds": max(samples), "p95_seconds": sorted(samples)[math.ceil(0.95 * len(samples)) - 1],
        }
    report = {
        "label": args.label,
        "commit": command_output(["git", "-C", str(repository), "rev-parse", "HEAD"]),
        "dirty": bool(command_output(["git", "-C", str(repository), "status", "--porcelain"])),
        "machine": {
            "os": platform.platform(), "architecture": platform.machine(),
            "cpu": command_output(["sysctl", "-n", "machdep.cpu.brand_string"]) if platform.system() == "Darwin" else platform.processor(),
            "logical_cpus": os.cpu_count(),
            "model": command_output(["sysctl", "-n", "hw.model"]) if platform.system() == "Darwin" else None,
            "memory_bytes": command_output(["sysctl", "-n", "hw.memsize"]) if platform.system() == "Darwin" else None,
        },
        "variants": variants,
        "compiler": command_output(["c++", "--version"]),
        "definition": "Process launch to Server started.; includes loader, registries, world opening, scripts and listener setup.",
        "conditions": "Seed 12345, default settings except ephemeral ports and LAN discovery disabled; no players, plugins or packs; one warm-up; filesystem caches not flushed; comparison order alternates each trial.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for name, variant in variants.items():
        print(f"{name}: fresh median {variant['fresh_world_seconds']:.6f}s; existing median {variant['median_seconds']:.6f}s")


if __name__ == "__main__":
    main()
