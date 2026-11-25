#!/usr/bin/env python3
"""Summarize server tick timing and packets observed by external clients."""

import collections
import json
import pathlib
import re
import sys


def analyse(root):
    clients = json.loads((root / "network.json").read_text())
    lines = (root / "captured.log").read_text().splitlines()
    measurement = next(json.loads(line.split("NET_MEASURE_RESULT ", 1)[1])
                       for line in lines if "NET_MEASURE_RESULT " in line)
    intervals = sorted(measurement["intervals"])
    config = json.loads((root / "run.json").read_text()) if (root / "run.json").exists() else {}
    group_size = config.get("groupSize", 50)
    groups = (len(clients) + group_size - 1) // group_size
    chats = sum(client["Chats"] for client in clients)
    summary = {
        "scenario": f"{groups} groups of up to {group_size}, 256 blocks between groups; global chat",
        "clients": len(clients),
        "connection_retries_before_measurement": sum(line.startswith("RETRY ")
            for line in (root / "client.log").read_text().splitlines()),
        "connections_completed": sum(client["Joined"] for client in clients),
        "clients_without_read_error": sum(client["Joined"] and not client["Error"] for client in clients),
        "receiving_other_movement_and_chat": sum(client["Moves"] > 0 and client["Chats"] > 0 for client in clients),
        "totals": {key: sum(client[key] for client in clients)
                   for key in ("Moves", "Chats", "Blocks", "SentMoves", "SentChats", "SentAttacks")},
        "errors": dict(collections.Counter(client["Error"].rsplit(": ", 1)[-1]
                                           for client in clients if client["Error"])),
        "measurement": measurement,
        "measured_tps": measurement["ticks"] * 1000 / measurement["elapsedMs"],
        "interval_p95_ms": intervals[int(0.95 * (len(intervals) - 1))],
        "interval_max_ms": max(intervals),
        "chat_delay_mean_ms": sum(client["ChatDelayTotalMs"] for client in clients) / max(1, chats),
        "chat_delay_max_ms": max(client["ChatDelayMaxMs"] for client in clients),
    }
    failures = []
    expected = config.get("players", len(clients))
    if not expected or len(clients) != expected or summary["clients_without_read_error"] != expected:
        failures.append("not every expected client remained connected without a read error")
    if measurement["minOnline"] != expected or measurement["online"] != expected:
        failures.append("server did not retain every client throughout the measurement")
    if expected > 1 and summary["receiving_other_movement_and_chat"] != expected:
        failures.append("some clients did not receive other players' movement and chat")
    if not intervals or max(intervals) > 5000:
        failures.append("measurement was interrupted or the server stalled for over five seconds")
    if "seconds" in config and abs(measurement["elapsedMs"] - config["seconds"] * 1000) > 1000:
        failures.append("measured duration differs from the requested duration by over one second")
    summary["validation_errors"] = failures
    summary["run"] = config
    for report in (root / "profiler").glob("*.html"):
        text = re.sub("<[^>]+>", " ", report.read_text())
        for key in ("Average TPS", "Average MSPT", "Peak MSPT", "Lowest MSPT"):
            match = re.search(key + r"\s+([\d.]+)", text)
            if match:
                summary[key] = float(match[1])
    (root / "summary.json").write_text(json.dumps(summary, indent=2))
    printable = {key: value for key, value in summary.items() if key != "measurement"}
    printable["measurement"] = {key: value for key, value in measurement.items() if key != "intervals"}
    print(json.dumps(printable, indent=2))
    return summary


if __name__ == "__main__":
    result = analyse(pathlib.Path(sys.argv[1]))
    sys.exit(bool(result["validation_errors"]))
