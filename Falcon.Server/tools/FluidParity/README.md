# Fluid parity tools

Evidence behind Falcon's liquids, taken from BDS 1.26.61 with the `fluidtrace` behavior pack in `pack/`.

## Running BDS

Copy BDS to its own folder, put `pack/` in `development_behavior_packs/fluidtrace`, and list it in the world's
`world_behavior_packs.json`:

```json
[{"pack_id": "6d3f8a3e-2a51-4b0e-9a40-0f5c1d7e9a11", "version": [1, 0, 0]}]
```

Feed the console from a fifo and keep chunks 0 to 2 ticking, plus the bench area:

```
tickingarea add 0 0 0 47 0 47 fl
execute in nether run tickingarea add 0 0 0 47 0 47 fln
tickingarea add 96 0 96 239 0 191 grid
```

## The block table

`liquid_blocks.tsv` holds what BDS answers for every block; `liquid_table.py` prints the `ENTRIES` table of
`src/World/Level/Block/LiquidProperties.cpp` from it. To measure again, send these and keep the `FP`, `FG` and
`FS` lines of the log:

```
scriptevent fl:props x
scriptevent fl:grid water 240 100
scriptevent fl:grid lava 230 400
scriptevent fl:solid 220 60
```

then `python3 measure.py props.txt grid.txt solid.txt` with the three sets of lines, the prefix removed.
