# usage: bdsrun.py name ticks "x0 y0 z0 x1 y1 z1" dim "cmd;cmd;..."
import sys, time, os
B=os.environ['FLUID_BDS_DIR']
name,ticks,box,dim,cmds=sys.argv[1:6]
log=B+'/bds.log'
start=os.path.getsize(log)
with open(B+'/console.fifo','w') as f:
    f.write('scriptevent fl:run %s %s %s %s %s\n'%(name,ticks,box,dim,cmds))
deadline=time.time()+int(ticks)/10+60
out=[]
while time.time()<deadline:
    with open(log) as f:
        f.seek(start); data=f.read()
    if 'FL END '+name in data:
        for l in data.splitlines():
            i=l.find('FL ')
            if i>=0: out.append(l[i:])
        break
    time.sleep(0.5)
print('\n'.join(out))
