# writes a BDS trace as Falcon test data: commands, then the box state at tick 0 and the changes after it
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.chdir(os.path.dirname(os.path.abspath(__file__)))
from scenarios import S, relt, frame
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'tests', 'data', 'fluids')
def cells(line):
    p = line.split(' ')
    return dict(c.split('=', 1) for c in p[3:] if '=' in c), int(p[2])
for n in (sys.argv[1:] or list(S)):
    ticks, cmds = S[n][:2]
    dim, box, clear = frame(n)
    steps = [cells(l.strip()) for l in open('traces/%s.txt' % n) if l.startswith('FL T')]
    if not steps:
        print('no trace', n); continue
    with open(os.path.join(OUT, n + '.txt'), 'w') as f:
        f.write('# BDS 1.26.61 trace, written by the fluid parity harness\n')
        f.write('ticks %d\nbox %s\ndimension %s\n' % (ticks, box, dim))
        for c in cmds:
            c = relt(c)
            at = 0
            if c.startswith('@'):
                a, c = c.split(' ', 1); at = int(a[1:])
            f.write('cmd %d %s\n' % (at, c))
        prev = {}
        for state, t in steps:
            diff = ['%s=%s' % (k, v) for k, v in sorted(state.items()) if prev.get(k) != v]
            diff += ['%s=air' % k for k in sorted(prev) if k not in state]
            f.write('T %d %s\n' % (t, ' '.join(diff)))
            prev = state
    print(n, len(steps))
