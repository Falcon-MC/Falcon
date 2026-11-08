# prints the ENTRIES table of src/World/Level/Block/LiquidProperties.cpp from liquid_blocks.tsv
# lava is only trusted where water agrees the block is open: blocks that break off unsupported read as lava
# flowing in; '?' keeps the default of an absent entry: water and lava blocked, solid
import sys

LIQUIDS = {'minecraft:air', 'minecraft:water', 'minecraft:lava', 'minecraft:flowing_water',
           'minecraft:flowing_lava', 'minecraft:bubble_column'}
for line in open(sys.argv[1] if len(sys.argv) > 1 else 'liquid_blocks.tsv'):
    if line.startswith('#'):
        continue
    name, contains, blocking, popped, lava, solid, water = line.rstrip('\n').split('\t')
    if name in LIQUIDS:
        continue
    flags = []
    # a block the property dump could not place (fire) is still seen washed away in the water grid
    if blocking == '?' and water == '1':
        flags.append('WATER_BROKEN')
    if blocking == '0':
        flags.append('WATER_POPPED' if popped == '1' else 'WATER_NO_REACTION' if contains == '1' else 'WATER_BROKEN')
    if contains == '1':
        flags.append('CONTAINS_WATER')
    if lava == '1' and (blocking == '0' or (blocking == '?' and water == '1')):
        flags.append('LAVA_FLOWS_IN')
    if solid == '0':
        flags.append('NOT_SOLID')
    if flags:
        print('            {"%s", %s},' % (name[len('minecraft:'):], ' | '.join(flags)))
