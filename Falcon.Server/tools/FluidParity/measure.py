# folds the three BDS dumps of the fluidtrace pack into liquid_blocks.tsv
#   props.txt  "FP" lines of `scriptevent fl:props`: canContainLiquid, isLiquidBlocking, liquidSpreadCausesSpawn
#   grid.txt   "FG" lines of `scriptevent fl:grid lava 230 400` and `fl:grid water 240 100`: does the liquid
#              poured beside the block replace it
#   solid.txt  "FS" lines of `scriptevent fl:solid 220 60`: do two water sources make a third on top of it
# '?' marks what the dump could not place
import sys

props, grid, water, solid = {}, {}, {}, {}
for line in open(sys.argv[1]):
    parts = line.split()
    values = dict(part.split('=', 1) for part in parts[2:] if '=' in part)
    props[parts[0]] = values if parts[1] == 'ok' else None
for line in open(sys.argv[2]):
    liquid, name, placed, now = line.split()
    table = grid if liquid == 'lava' else water
    table[name] = None if placed[7:] != name else now[4:].startswith('minecraft:' + liquid)
for line in open(sys.argv[3]):
    parts = line.split()
    if len(parts) < 4:
        continue
    name, placed, top, below = parts[0], parts[1][7:], parts[2][2:], parts[3][2:]
    solid[name] = None if placed != name or below != name else top != 'minecraft:water:1'

with open('liquid_blocks.tsv', 'w') as out:
    out.write('# name\tcontains\tblocking\tpopped\tlava_flows_in\tsolid\twater_flows_in\n')
    for name in sorted(props):
        values = props[name]
        def flag(key):
            return '?' if values is None else values.get(key, '?')
        def measured(table):
            value = table.get(name)
            return '?' if value is None else ('1' if value else '0')
        out.write('\t'.join([name, flag('contain'), flag('blocking'), flag('spawn'), measured(grid),
                             measured(solid), measured(water)]) + '\n')
