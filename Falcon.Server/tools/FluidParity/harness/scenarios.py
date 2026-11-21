# shared scenario table: BDS capture and Falcon test data; coordinates relative to chunk (1, 1)
OX, OZ = 16, 16
def rel(cmd):
    p = cmd.split(' ')
    i = 0
    out = []
    while i < len(p):
        if p[i] in ('setblock', 'fill', 'waterlog'):
            n = 2 if p[i] == 'fill' else 1
            out.append(p[i]); i += 1
            for k in range(n):
                out += [str(int(p[i]) + OX), p[i + 1], str(int(p[i + 2]) + OZ)]; i += 3
        else:
            out.append(p[i]); i += 1
    return ' '.join(out)
def relt(cmd):
    if cmd.startswith('@'):
        a, b = cmd.split(' ', 1)
        return a + ' ' + rel(b)
    return rel(cmd)
CLEAR = "fill 16 185 16 31 215 31 air"
FLOOR = "fill 0 199 0 15 199 15 stone"
BOX = "16 195 16 31 206 31"
S = {}
S['platform'] = (60, [FLOOR, "setblock 7 200 7 flowing_water"])
S['drain'] = (130, [FLOOR, "setblock 7 200 7 flowing_water", "@50 setblock 7 200 7 air"])
S['stairs'] = (100, ["fill 0 199 0 5 199 15 stone", "fill 6 198 0 7 198 15 stone", "fill 8 197 0 9 197 15 stone",
                     "fill 10 196 0 11 196 15 stone", "fill 12 195 0 15 195 15 stone", "setblock 2 200 7 flowing_water"])
S['twosrc'] = (60, [FLOOR, "fill 2 200 6 12 200 6 stone", "fill 2 200 8 12 200 8 stone",
                    "setblock 4 200 7 flowing_water", "setblock 6 200 7 flowing_water"])
S['twosrcair'] = (60, ["fill 2 199 6 12 199 8 stone", "setblock 5 199 7 air", "fill 2 200 6 12 200 6 stone",
                       "fill 2 200 8 12 200 8 stone", "setblock 4 200 7 flowing_water", "setblock 6 200 7 flowing_water"])
S['hole'] = (60, [FLOOR, "setblock 10 199 7 air", "setblock 7 200 7 flowing_water"])
S['hole2'] = (60, [FLOOR, "setblock 9 199 5 air", "setblock 4 199 9 air", "setblock 7 200 7 flowing_water"])
S['holefar'] = (60, [FLOOR, "setblock 13 199 7 air", "setblock 7 200 7 flowing_water"])
S['waterfall'] = (60, [FLOOR, "setblock 7 205 7 flowing_water"])
S['ledge'] = (80, ["fill 0 199 0 7 199 15 stone", "fill 0 196 0 15 196 15 stone", "setblock 4 200 7 flowing_water"])
S['still'] = (40, [FLOOR, "setblock 7 200 7 water"])
S['placeflow'] = (40, [FLOOR, "setblock 7 200 7 flowing_water [\"liquid_depth\"=3]"])
# interactions and waterlogging
S['lavaplat'] = (200, [FLOOR, "setblock 7 200 7 flowing_lava"])
S['lavawater'] = (200, [FLOOR, "setblock 4 200 7 flowing_lava", "setblock 10 200 7 flowing_water"])
S['lavafall'] = (120, [FLOOR, "fill 5 200 5 9 200 9 water", "setblock 7 204 7 flowing_lava"])
S['waterfalllava'] = (120, [FLOOR, "fill 5 200 5 9 200 9 lava", "setblock 7 204 7 flowing_water"])
S['basalt'] = (150, ["fill 0 199 0 15 199 15 soul_soil", "setblock 9 200 7 blue_ice", "setblock 7 200 7 flowing_lava"])
S['sponge'] = (60, [FLOOR, "setblock 7 200 7 flowing_water", "@40 setblock 7 200 9 sponge"])
S['concrete'] = (40, [FLOOR, "setblock 9 200 7 white_concrete_powder", "setblock 7 200 4 red_concrete_powder", "setblock 7 201 9 lime_concrete_powder", "setblock 7 200 9 stone", "setblock 7 200 7 flowing_water"])
S['drops'] = (40, [FLOOR, "setblock 8 200 7 torch", "setblock 7 200 8 snow_layer", "setblock 6 200 7 lever", "setblock 7 200 6 oak_fence", "setblock 7 200 7 flowing_water"])
S['waterlog'] = (60, [FLOOR, "setblock 8 200 7 oak_stairs", "setblock 6 200 7 smooth_stone_slab", "setblock 7 200 8 oak_fence", "setblock 7 200 7 flowing_water", "@30 setblock 8 200 7 oak_stairs [\"weirdo_direction\"=1]"])
S['waterlogout'] = (60, [FLOOR, "setblock 7 200 7 oak_fence", "waterlog 7 200 7"])
S['waterlogslab'] = (60, [FLOOR, "setblock 7 200 7 smooth_stone_slab", "waterlog 7 200 7", "setblock 8 200 7 smooth_stone_slab [\"top_slot_bit\"=true]", "waterlog 8 200 7"])
S['sourceabove'] = (60, ["fill 5 199 5 9 199 9 stone", "fill 5 200 5 9 200 9 glass", "setblock 7 200 7 air", "setblock 6 200 7 flowing_water", "setblock 7 201 7 flowing_water"])
S['netherlava'] = (120, ["fill 0 99 0 15 101 15 stone", "fill 1 100 1 14 101 14 air", "setblock 7 100 7 flowing_lava"], 'nether')

def frame(name):
    dim = S[name][2] if len(S[name]) > 2 else 'overworld'
    if dim == 'nether':
        return dim, "16 95 16 31 106 31", "fill 16 90 16 31 110 31 air"
    return dim, BOX, CLEAR
S['unwaterlog'] = (60, [FLOOR, "fill 6 200 6 8 200 8 glass", "setblock 7 200 7 oak_fence", "waterlog 7 200 7", "@20 setblock 7 200 7 air"])
