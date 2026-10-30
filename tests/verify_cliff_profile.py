"""Check the complete loft, including raised shoulder, rim support and ends.

Assertions use final blocks as well as surface descriptors. Old cut-bank tests
alone could pass a tall hole while missing the user's continuous cliff shape.
"""
from pathlib import Path
import sys,json,math
import numpy as np
from verify_water import run,decode
P=Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0,str(P/'tools'))
from voxel_connectivity import floating_rock
E=P/'build/windows/x64/release/native-worldgen-check.exe'


def verify(source,output):
    j=json.loads(source.read_text('utf-8'));base=dict(j);base.pop('overhangCliffs')
    basefile=output/'without-cliffs.json';basefile.write_text(json.dumps(base,separators=(',',':')),'utf-8')
    result=[]
    for index,c in enumerate(j['overhangCliffs']['sites']):
        radius=math.ceil(math.hypot(c['halfLength'],max(c['front'],c['back'])))+12
        minx=(int(c['x'])-radius)//16*16;minz=(int(c['z'])-radius)//16*16
        maxx=(int(c['x'])+radius)//16*16+15;maxz=(int(c['z'])+radius)//16*16+15
        coords=[(x,z) for z in range(minz,maxz+1) for x in range(minx,maxx+1)]
        query=[[x+32768,z+32768] for x,z in coords]
        rows=run(E,source,query);before=run(E,basefile,query)
        chunks=[(x,z) for z in range(minz//16,maxz//16+1) for x in range(minx//16,maxx//16+1)]
        run(E,source,[dict(cx=x,cz=z,output=str(output/f'loft-{index}-{x}-{z}.bin')) for x,z in chunks])
        raw={(x,z):decode(output/f'loft-{index}-{x}-{z}.bin',94,24) for x,z in chunks}
        low=max(32,int(c['toe']-radius*abs(c['tangent'])-12));high=min(318,int(c['crest']+radius*abs(c['tangent'])+60))
        solid=np.zeros((maxz-minz+1,maxx-minx+1,high-low+1),dtype=bool)
        uplift=0;changed=0;outward=0;rim=0;shell=0;edgeDelta=0
        modified=np.zeros(solid.shape[:2],dtype=bool)
        for (x,z),s,b in zip(coords,rows,before):
            col=raw[x//16,z//16][z%16,x%16]
            solid[z-minz,x-minx]=np.isin(col[low+64:high+65],[1,2,3,4,6,7,20,27,32,33,42,85])
            dx=x-c['x'];dz=z-c['z'];w=-dx*c['nz']+dz*c['nx'];u=dx*c['nx']+dz*c['nz']
            if abs(w)>=c['halfLength'] or u<=-c['front']-3 or u>=c['back']+3:
                assert s==b,(x,z,'changed outside footprint')
            if s['scarp']<=0:continue
            modified[z-minz,x-minx]=True
            assert s['water']<s['height'] and not s['river'] and not s['lake']
            delta=s['height']-b['height'];uplift=max(uplift,delta);changed+=delta!=0
            if c['halfLength']-2<=abs(w)<c['halfLength']:
                edgeDelta=max(edgeDelta,abs(delta))
            lo,hi=s['cliffLow'],s['cliffHigh']
            if lo==-32768:continue
            assert np.all(col[lo+64:hi+65]==0)
            assert np.all(col[hi+65:s['height']+65]==2),'rock lip/support changed by caves or foliage'
            assert np.all(col[lo+58:lo+64]==2),'thin foot shell'
            assert s['height']>=hi+1
            shell+=6;rim+=s['height']-hi
            # The exposed face is open towards the valley, not a closed cave.
            y=(lo+hi)//2
            # At the tapered ends the old sloping ground can re-enter a level
            # sightline. Use the tall central face for this open-valley check.
            for k in (range(1,16) if s['scarp']>.55 and hi-lo>=10 else []):
                xx=round(x-c['nx']*k);zz=round(z-c['nz']*k)
                if (xx//16,zz//16) not in raw:break
                assert raw[xx//16,zz//16][zz%16,xx%16,y+64]==0,(x,z,'face blocked towards valley')
            outward+=1
        assert uplift>=15 and changed>500 and outward>80 and edgeDelta<=1
        floatingRuns=floating_rock(solid,True)
        floating=sum(b-a for x,z,a,b in floatingRuns if modified[z,x])
        assert floating==0,('unattached rock',floating)
        # Shoulder uplift, rather than merely a longer air interval, is vital.
        result.append(dict(site=c,chunks=len(chunks),columns=len(coords),maxUplift=uplift,changedColumns=changed,openFaceColumns=outward,solidRimVoxels=rim,footShellVoxels=shell,endDelta=edgeDelta,floatingRockVoxels=floating,untouchedCaveFragments=sum(b-a for x,z,a,b in floatingRuns if not modified[z,x])))
    assert result
    report=dict(seed=j['config']['seed'],sites=result,wholeProfileVerified=True,clientVisualAccepted=False)
    (output/'profile-validation.json').write_text(json.dumps(report,indent=2)+'\n','utf-8')
    print(json.dumps(report),flush=True)

if __name__=='__main__':
    source=Path(sys.argv[1]);output=Path(sys.argv[2]);verify(source,output)
