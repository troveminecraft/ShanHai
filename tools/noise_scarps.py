"""Fold the existing noise hillside in space; bake intervals, not shaped blocks.

D(x,y,z) = H((x,z) + direction * envelope * A * tanh((y-mid)/S)) - y.
H is the unchanged native continuous ground. There is no separate crest plane.
"""
from pathlib import Path
from world_bounds import HALF
import json,math,sys
import numpy as np
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'tests'))
from verify_water import run
from verify_cliff_profile import floating_rock
E=P/'build/windows/x64/release/native-worldgen-check.exe'

def build(source,output,c,index):
    c=dict(c);c.update(front=80.,back=80.,halfLength=min(80.,c['halfLength']*1.25),warpAmount=26.,scaleSlopeRatio=.75)
    nx,nz=c['nx'],c['nz'];half=c['halfLength'];span=max(c['front'],c['back'])
    rx=math.ceil(abs(nx)*span+abs(nz)*half)+2;rz=math.ceil(abs(nz)*span+abs(nx)*half)+2
    minx,minz=round(c['x'])-rx,round(c['z'])-rz;w,h=2*rx+1,2*rz+1
    pad=30;sx,sz=minx-pad,minz-pad;sw,sh=w+2*pad,h+2*pad
    if min(sx,sz)<-HALF or sx+sw>HALF or sz+sh>HALF:return None,dict(reason='warp sampling outside bounds')
    file=output/f'noise-ground-{index}.bin'
    run(E,source,[dict(scarpNoiseScan=True,x=sx,z=sz,width=sw,height=sh,output=str(file))])
    H=np.fromfile(file,'<f8').reshape(sh,sw)
    def at(x,z):
        X=x-sx;Z=z-sz;ix=np.floor(X).astype(int);iz=np.floor(Z).astype(int);fx=X-ix;fz=Z-iz
        assert np.all((ix>=0)&(iz>=0)&(ix<sw-1)&(iz<sh-1))
        return (H[iz,ix]*(1-fx)+H[iz,ix+1]*fx)*(1-fz)+(H[iz+1,ix]*(1-fx)+H[iz+1,ix+1]*fx)*fz
    def smooth(a,b,x):
        t=np.clip((x-a)/(b-a),0,1);return t*t*(3-2*t)
    zz,xx=np.mgrid[minz:minz+h,minx:minx+w];dx,dz=xx-c['x'],zz-c['z'];u=dx*nx+dz*nz;v=-dx*nz+dz*nx
    mask=(1-smooth(half*.28,half,np.abs(v)))*(1-smooth(span*.28,span,np.abs(u)))
    # Mid-height and direction follow the original hillside along the contour.
    # A modest gradient turn keeps this connected to curved natural ridges.
    along=np.clip(v,-half,half);mx=c['x']-nz*along;mz=c['z']+nx*along
    mid=at(mx,mz);gx=(at(mx+8,mz)-at(mx-8,mz))/16;gz=(at(mx,mz+8)-at(mx,mz-8))/16
    scale=np.clip(c['warpAmount']*c['scaleSlopeRatio']*np.maximum(.4,gx*nx+gz*nz),12.,36.)
    length=np.maximum(.01,np.hypot(gx,gz));gx=.75*nx+.25*gx/length;gz=.75*nz+.25*gz/length
    length=np.hypot(gx,gz);gx/=length;gz/=length
    original=np.floor(at(xx,zz)+1e-6).astype(np.int16)
    low=max(32,int(np.floor(H.min()))-2);high=min(300,int(np.ceil(H.max()))+2)
    solid=np.empty((h,w,high-low+1),bool)
    for y in range(low,high+1):
        shift=c['warpAmount']*mask*np.tanh((y-mid)/scale)
        solid[:,:,y-low]=at(xx+gx*shift,zz+gz*shift)+1e-6>=y
    # Voxel-scale oscillations are not a second cave system. Close only one-
    # or two-block vertical pinholes before encoding the one open cliff span.
    pinholes=solid[:,:,:-2]&solid[:,:,2:]
    solid[:,:,1:-1]|=pinholes
    pinholes=solid[:,:,:-3]&solid[:,:,3:]
    solid[:,:,1:-2]|=pinholes;solid[:,:,2:-1]|=pinholes
    # Preserve the ground and connected upper mountain, but remove intermediate
    # thin shelves if the original fine noise folds more than once in a column.
    # The supported representation is one exterior recess, not stacked caves.
    mergedShelves=0
    for z in range(h):
        for x in range(w):
            edges=np.diff(np.r_[False,solid[z,x],False].astype(np.int8))
            starts=np.flatnonzero(edges==1);ends=np.flatnonzero(edges==-1)
            if len(starts)>2:
                mergedShelves+=int(solid[z,x,ends[0]:starts[-1]].sum())
                solid[z,x,ends[0]:starts[-1]]=False
    # Quantization may leave a thin disconnected tip. Remove only unattached
    # components; do not invent a level platform to thicken the whole crest.
    fragments=floating_rock(solid,True);removed=sum(b-a for _,_,a,b in fragments)
    for x,z,a,b in fragments:solid[z,x,a:b]=False
    grid=np.empty((h,w,4),dtype=np.int16);grid[:]=[-32768,-32768,-32768,0]
    changed=0;openings=0;maxvoid=0;maxrise=0;maxcut=0;multispan=0
    for z in range(h):
        for x in range(w):
            transitions=np.diff(np.r_[False,solid[z,x],False].astype(np.int8));beg=np.flatnonzero(transitions==1);end=np.flatnonzero(transitions==-1)
            if len(beg)>2:multispan+=1;continue
            if not len(end) or beg[0]!=0:return None,dict(reason='ungrounded density column')
            top=int(end[-1]+low-1);lo=hi=-32768
            if len(beg)==2:lo=int(end[0]+low);hi=int(beg[1]+low-1);openings+=1;maxvoid=max(maxvoid,hi-lo+1)
            if top==original[z,x] and lo==-32768:continue
            if top<40 or top>290:return None,dict(reason='unsupported height')
            grid[z,x]=[top,lo,hi,int(lo>-32768)];changed+=1;maxrise=max(maxrise,top-int(original[z,x]));maxcut=max(maxcut,int(original[z,x])-top)
    if multispan:return None,dict(reason='multiple folds',columns=multispan)
    if openings<40 or maxvoid<20:return None,dict(reason='insufficient natural cliff',openings=openings,maxvoid=maxvoid)
    rows=[]
    for z in range(h):
        xs=np.flatnonzero(grid[z,:,0]!=-32768)
        if not len(xs):continue
        a,b=int(xs[0]),int(xs[-1])+1;rows.append([z+minz,a+minx,*grid[z,a:b].flatten().tolist()])
    c.update(bounds=[minx,minz,w,h],rows=rows,midlinePolicy='original-noise-height-and-gradient',toe=int(original[mask>.8].min()),crest=int(original[mask>.8].max()))
    np.savez_compressed(output/f'noise-volume-{index}.npz',original=original,columns=grid,mask=mask,low=low,high=high)
    report=dict(changedColumns=changed,openFaceColumns=openings,maxOpenHeight=maxvoid,maxUplift=maxrise,maxCut=maxcut,removedUnattachedVoxels=removed,mergedInternalShelfVoxels=mergedShelves,serializedBytes=len(json.dumps(rows)),bounds=c['bounds'])
    return c,report
