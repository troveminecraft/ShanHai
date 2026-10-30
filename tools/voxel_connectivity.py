"""Six-connected solid components used when baking supported scarps."""
import numpy as np

def floating_rock(solid,return_runs=False):
    # Label vertical runs, then join overlaps with their four horizontal
    # neighbours. Equivalent to six-connected voxel components without scipy.
    nz,nx,ny=solid.shape;parent=[];size=[];anchored=[];previous={};locations=[]
    def root(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    def union(i,j):
        a,b=root(i),root(j)
        if a!=b:parent[b]=a;anchored[a]|=anchored[b]
    for z in range(nz):
        current={}
        for x in range(nx):
            transitions=np.diff(np.r_[False,solid[z,x],False].astype(np.int8))
            runs=[]
            for lo,hi in zip(np.flatnonzero(transitions==1),np.flatnonzero(transitions==-1)):
                i=len(parent);parent.append(i);size.append(int(hi-lo));anchored.append(z in [0,nz-1] or x in [0,nx-1] or lo==0);locations.append((x,z,int(lo),int(hi)))
                for a,b,j in current.get(x-1,[])+previous.get(x,[]):
                    if lo<b and a<hi:union(i,j)
                runs.append((lo,hi,i))
            current[x]=runs
        previous=current
    if return_runs:return [locations[i] for i in range(len(parent)) if not anchored[root(i)]]
    return sum(size[i] for i in range(len(parent)) if not anchored[root(i)])
