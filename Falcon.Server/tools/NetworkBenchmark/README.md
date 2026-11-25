# External network benchmark

This harness connects 1,000 Gophertunnel clients over NetherNet (UDP/DTLS/SCTP), with the stock Pion transport. Clients run in a separate process, send actual gameplay packets and decode the packets Falcon sends back.

Run from the repository root with Go and Python 3 installed:

```sh
python3 Falcon.Server/tools/NetworkBenchmark/run.py build/Falcon.Server/FalconServer
```

The runner builds the client executable and creates a temporary server directory. It prints the result directory, waits for every client to spawn, allows 15 seconds for settling, measures 60 seconds, then closes the clients and stops the server. Use `--players`, `--group-size`, `--seconds`, `--settle-seconds` and `--port` for smaller checks or a different local port. The client uses the loopback interface (`lo0` on macOS, `lo` on Linux).

## Workload

- By default, 20 groups of 50 survival players; `--group-size 20` uses 50 groups of 20. Groups are separated by 256 blocks in one normally generated world (seed 42).
- Each client sends movement at 20 Hz, attempts a block break and an attack every two seconds, and sends a global chat message every six seconds. Actions are staggered between clients.
- Clients receive other players' movement, global chat, block changes and the normal login/chunk stream.
- A behavior pack builds the platforms at Y=199, replenishes the slime blocks and restores health once per second to sustain combat. It does not create bots, move them during the measurement, generate their input, or suppress network recipients.
- Mob spawning and item drops are disabled. View distance and tick distance are 4. The server uses offline identities with unofficial clients enabled; NetherNet's transport encryption remains enabled.

The server and clients share the same machine. This measures local network load and includes client CPU/memory contention. It does not measure Internet latency or the case where all 1,000 players are mutually visible.

## Results

`summary.json` contains tick count, elapsed time, minimum online count, accepted damage and block breaks, per-client connection errors, aggregate sent/received actions, tick interval statistics and chat delivery delay. `network.json` contains each client's counters. `captured.log`, `client.log` and `profiler/*.html` retain the raw evidence.

The runner fails when clients disconnect, do not receive other players' movement and chat, or the measurement is interrupted. `run.json` records the requested duration, platform and server binary hash. On macOS, prefix the command with `caffeinate -i` to prevent automatic sleep during the run.

Tick throughput alone is insufficient: check delivery delay too, since an overloaded transport can accumulate packets while the tick loop continues running.

To summarize an existing result directory:

```sh
python3 Falcon.Server/tools/NetworkBenchmark/analyse.py /path/to/results
```

## Recorded run

On an Apple M4 (10 CPU cores, 16 GiB RAM), macOS 26.5.1, running server and clients together, the following command retained all 1,000 connections for 1,200 ticks in 59.996 seconds (20.00 TPS):

```sh
caffeinate -i python3 Falcon.Server/tools/NetworkBenchmark/run.py build/Falcon.Server/FalconServer --group-size 20 --settle-seconds 60
```

The clients decoded 18,892,324 movements from other players and 9,870,961 global chat messages. Falcon accepted 29,636 block breaks and 24,627 hits. Chat delivery averaged 109 ms, with a maximum of 412 ms. Tick intervals had a 57 ms p95 and a 69 ms maximum; the native profiler measured 32.02 ms average work per tick and a 62.95 ms peak. Two connection attempts retried before measurement; there were no client read errors or disconnections during measurement.

[results.json](results.json) records this run and two earlier runs with the same server binary. With 20 groups of 50 and 15 seconds of settling, tick throughput was 19.99 TPS but chat delay averaged 4.9 seconds. With 50 groups of 20 and only 15 seconds of settling, tick throughput was 20.00 TPS and mean chat delay was 267 ms, but the worst message was delayed 17.8 seconds. Density and settling time are part of the reported scenario; these results do not establish capacity for 1,000 mutually visible players or uniformly sub-50-ms ticks.
