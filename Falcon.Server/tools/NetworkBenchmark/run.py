#!/usr/bin/env python3
"""Run external Gophertunnel clients against an isolated local Falcon server."""

import argparse
import json
import hashlib
import platform
import pathlib
import signal
import subprocess
import tempfile
import time
import uuid

ROOT = pathlib.Path(__file__).resolve().parent


def wait_for(path, marker, process, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if marker in path.read_text():
            return
        if process.poll() is not None:
            raise RuntimeError(f"server exited before {marker}; see {path}")
        time.sleep(0.05)
    raise TimeoutError(f"missing {marker}; see {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("server", type=pathlib.Path)
    parser.add_argument("--players", type=int, default=1000, choices=range(1, 1001), metavar="1..1000")
    parser.add_argument("--group-size", type=int, default=50, choices=range(2, 51, 2), metavar="even 2..50")
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--settle-seconds", type=int, default=15)
    parser.add_argument("--port", type=int, default=29867)
    args = parser.parse_args()
    if args.seconds < 1 or args.settle_seconds < 0 or not 1024 <= args.port <= 65534:
        parser.error("seconds must be positive; port must be 1024..65534")
    server_path = args.server.resolve(strict=True)
    result = pathlib.Path(tempfile.mkdtemp(prefix="falcon-network-benchmark-"))
    print(result, flush=True)
    with server_path.open("rb") as binary:
        server_hash = hashlib.file_digest(binary, "sha256").hexdigest()
    (result / "run.json").write_text(json.dumps({
        "players": args.players, "seconds": args.seconds, "settleSeconds": args.settle_seconds, "groupSize": args.group_size, "groupSpacing": 256,
        "platform": platform.platform(), "machine": platform.machine(),
        "serverSha256": server_hash,
    }, indent=2))
    subprocess.run(["go", "build", "-o", str(result / "clients"), "."], cwd=ROOT, check=True)
    pack = result / "behavior_packs" / "NetworkArena"
    (pack / "scripts").mkdir(parents=True)
    (pack / "manifest.json").write_text(json.dumps({
        "format_version": 2,
        "header": {"name": "NetworkArena", "description": "External network benchmark arena",
                   "uuid": str(uuid.uuid4()), "version": [1, 0, 0], "min_engine_version": [1, 21, 0]},
        "modules": [{"type": "script", "language": "javascript", "uuid": str(uuid.uuid4()),
                     "version": [1, 0, 0], "entry": "scripts/main.js"}],
        "dependencies": [{"module_name": "@minecraft/server", "version": "2.0.0"}],
    }))
    groups = (args.players + args.group_size - 1) // args.group_size
    arena = (ROOT / "arena.js").read_text().replace("const GROUP_SIZE = 50;", f"const GROUP_SIZE = {args.group_size};")
    arena = arena.replace("const GROUP_COUNT = 20;", f"const GROUP_COUNT = {groups};")
    (pack / "scripts" / "main.js").write_text(arena)
    (result / "server.properties").write_text(
        f"server-port={args.port}\nserver-portv6={args.port + 1}\n"
        "content-log-console-output-enabled=true\nenable-lan-visibility=false\n"
        "online-mode=false\nallow-unofficial-clients=true\nspawn-protection=0\n"
        "level-seed=42\nmax-players=1010\nview-distance=4\ntick-distance=4\n"
        "allow-cheats=true\ntransport=nethernet\n"
    )
    log_path = result / "captured.log"
    client = None
    with log_path.open("w") as log, (result / "client.log").open("w") as client_log:
        server = subprocess.Popen([str(server_path), "--no-wizard"], cwd=result, stdin=subprocess.PIPE,
                                  stdout=log, stderr=subprocess.STDOUT, text=True)

        def command(text):
            server.stdin.write(text + "\n")
            server.stdin.flush()

        try:
            wait_for(log_path, "NET_ARENA_READY", server, max(180, groups * 5))
            client = subprocess.Popen(
                [str(result / "clients"), "-players", str(args.players), "-seconds", str(args.seconds),
                 "-settle-seconds", str(args.settle_seconds), "-group-size", str(args.group_size), "-address", f"http://127.0.0.1:{args.port}", "-out", str(result / "network.json")],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            for line in client.stdout:
                client_log.write(line)
                client_log.flush()
                if line.startswith("JOINED"):
                    continue
                print(line.strip(), flush=True)
                if line.strip() == "MEASURE":
                    command("scriptevent netbench:start run")
                    wait_for(log_path, "NET_MEASURE_START", server, 10)
                    command("profiler on")
                elif line.strip() == "MEASURE_END":
                    command("scriptevent netbench:stop run")
                    wait_for(log_path, "NET_MEASURE_RESULT", server, 15)
                    command("profiler off")
                    wait_for(log_path, "Profiler stopped", server, 15)
                    client.stdin.write("close\n")
                    client.stdin.flush()
            if client.wait() != 0:
                raise RuntimeError(f"client run failed; see {result / 'client.log'}")
        finally:
            if client is not None and client.poll() is None:
                client.terminate()
                try:
                    client.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    client.kill()
                    client.wait()
            if server.poll() is None:
                command("stop")
                try:
                    server.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    server.send_signal(signal.SIGINT)
                    server.wait(timeout=15)
    if server.returncode != 0:
        raise RuntimeError(f"server exit {server.returncode}; see {log_path}")
    subprocess.run(["python3", str(ROOT / "analyse.py"), str(result)], check=True)


if __name__ == "__main__":
    main()
