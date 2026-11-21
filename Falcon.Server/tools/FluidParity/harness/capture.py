import sys, subprocess, os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
os.makedirs('traces', exist_ok=True)
from scenarios import S, relt, frame
names = sys.argv[1:] or list(S)
for n in names:
    ticks, cmds = S[n][:2]
    dim, box, clear = frame(n)
    cmds = [relt(c) for c in cmds]
    subprocess.run(['python3', 'bdsrun.py', 'clear_' + n, '160', box, dim, clear + ';@5 ' + clear + ';@100 ' + clear], capture_output=True)
    r = subprocess.run(['python3', 'bdsrun.py', n, str(ticks), box, dim, ';'.join(cmds)], capture_output=True, text=True)
    open('traces/%s.txt' % n, 'w').write(r.stdout)
    print(n, r.stdout.count('FL T'), 'ERR' in r.stdout, flush=True)
