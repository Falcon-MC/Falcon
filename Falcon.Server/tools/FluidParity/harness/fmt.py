# grid view per step and y layer: rows z, columns x; '.' empty, '#' other block, digit water depth, F falling, a-g lava, L falling lava, w waterlogged
import sys
def parse(path):
    steps=[]
    for l in open(path):
        l=l.strip()
        if not l.startswith('FL T'): continue
        p=l.split(' ')
        cells={}
        for c in p[3:]:
            k,v=c.split('=')
            cells[tuple(map(int,k.split(',')))]=v
        steps.append((int(p[2]),cells))
    return steps
def ch(v):
    if v is None: return '.'
    n=v.split(':')
    if n[0] in('water','flowing_water','lava','flowing_lava'):
        d=int(n[1].split('+')[0]); w=n[0].endswith('water')
        return ('F' if d>=8 else str(d)) if w else ('L' if d>=8 else 'abcdefgh'[d])
    return 'w' if '+w' in v else '#'
if __name__=='__main__':
    for t,cells in parse(sys.argv[1]):
        ys=sorted(set(k[1] for k,v in cells.items() if ch(v) not in '#.'))
        print('t=%d'%t)
        for y in ys:
            print(' y=%d'%y)
            for z in range(16):
                row=''.join(ch(cells.get((x,y,z))) for x in range(16))
                if row.strip('.#'): print('  %2d %s'%(z,row))
