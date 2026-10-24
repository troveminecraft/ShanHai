// World-space candidates, screened once; no per-voxel noise or neighbour chunks.
 struct SmallPool {int x=0,z=0,level=0,shoreCount=0;double radius=0,phase=0;bool lava=false,valid=false;int count=1;std::array<std::array<double,3>,2> lobes{};std::array<std::array<int,2>,4> shore{};};
 static double singlePoolDistance(SmallPool const&p,int x,int z){double dx=x-p.x,dz=z-p.z,a=std::atan2(dz,dx);return std::hypot(dx,dz)/(p.radius*(1+.12*std::sin(3*a+p.phase)));}
 static double poolDistance(SmallPool const&p,int x,int z){
  double q=singlePoolDistance(p,x,z);
  for(int i=0;i<p.count-1;++i){SmallPool l;l.x=int(p.lobes[i][0]);l.z=int(p.lobes[i][1]);l.radius=p.lobes[i][2];l.phase=p.phase;
   q=std::min(q,singlePoolDistance(l,x,z));}
  return q;
 }
 bool clusteredPools=C.value("smallPoolsVersion",0)==2;
 bool evaluatingSmallPool=false;
 Cache<uint64_t,SmallPool> smallPools{256};
 SmallPool smallPool(int gx,int gz){return smallPools.get(key2(gx,gz),[&]{SmallPool p;
  auto r=[&](int k){return hash01(gx,gz,seedHash(terrainSeed,9100+k));};
  if(r(0)>.22)return p;p.x=gx*160+24+int(r(1)*112);p.z=gz*160+24+int(r(2)*112);p.radius=3+r(3)*4;p.phase=r(4)*6.2831853;
  evaluatingSmallPool=true;struct Reset{bool&v;~Reset(){v=false;}}reset{evaluatingSmallPool};
  auto c=surface(p.x,p.z,false);if((gradualIceEcotone&&(c.giant>.01||c.lava>=c.height))||c.kind||c.lake||c.river||c.iceHabitat>.01||c.height<38||c.height>160||c.water>=c.height||c.stone>.01||c.volcanic>.01||c.coast>.05)return p;
  int lo=c.height,hi=c.height;for(int i=0;i<12;++i){double a=i*.5235987756;auto t=surface(p.x+int(std::cos(a)*(p.radius+3)),p.z+int(std::sin(a)*(p.radius+3)),false);if((gradualIceEcotone&&(t.giant>.01||t.lava>=t.height))||t.kind||t.lake||t.river||t.iceHabitat>.01||t.water>=t.height||t.stone>.01||t.volcanic>.01)return p;lo=std::min(lo,t.height);hi=std::max(hi,t.height);}if(hi-lo>2)return p;
  p.level=lo-1;p.lava=c.T>.65&&r(5)<.035;
  if(gradualIceEcotone&&!p.lava){
   // Pick one connected low shore along the real pool contour, once per pool.
   // The other banks stay intact; never lower an entire rectangular footprint.
   constexpr int N=21;std::array<bool,N*N> wet{},bank{};
   for(int z=1;z<N-1;++z)for(int x=1;x<N-1;++x)wet[z*N+x]=poolDistance(p,p.x+x-10,p.z+z-10)<1;
   std::vector<int> candidates;
   for(int z=1;z<N-1;++z)for(int x=1;x<N-1;++x){int i=z*N+x;if(!wet[i]&&(wet[i-1]||wet[i+1]||wet[i-N]||wet[i+N])){bank[i]=true;candidates.push_back(i);}}
   int wanted=2+int(r(6)*3),start=int(r(7)*candidates.size());
   for(int attempt=0;attempt<int(candidates.size())&&p.shoreCount==0;++attempt){
    std::array<int,4> path{};path[0]=candidates[(start+attempt)%candidates.size()];
    auto walk=[&](auto&&self,int count)->bool{
     if(count==wanted){for(int j=0;j<count;++j){int X=p.x+path[j]%N-10,Z=p.z+path[j]/N-10;auto t=surface(X,Z,false);
       if((gradualIceEcotone&&(t.giant>.01||t.lava>=t.height))||t.kind||t.lake||t.river||t.iceHabitat>.01||t.stone>.01||t.volcanic>.01||t.height<=p.level||t.height>p.level+3||t.water>=t.height||t.lava>=t.height)return false;
      }return true;}
     for(int delta:{1,N,-1,-N}){int next=path[count-1]+delta;if(next<0||next>=N*N||!bank[next])continue;bool used=false;for(int j=0;j<count;++j)used|=path[j]==next;if(used)continue;path[count]=next;if(self(self,count+1))return true;}return false;
    };
    if(walk(walk,1)){p.shoreCount=wanted;for(int j=0;j<wanted;++j)p.shore[j]={p.x+path[j]%N-10,p.z+path[j]/N-10};}
   }
   if(!p.shoreCount)return p;
  }
  if(clusteredPools&&!p.lava){
   ++p.level;p.shoreCount=0;
   auto sealed=[&](SmallPool const&test){
    int reach=28;
    for(int z=test.z-reach;z<=test.z+reach;++z)for(int x=test.x-reach;x<=test.x+reach;++x){
     if(poolDistance(test,x,z)>1.35)continue;
     if(x<=gx*160+1||x>=(gx+1)*160-2||z<=gz*160+1||z>=(gz+1)*160-2)return false;
     auto t=surface(x,z,false);
     if(t.kind||t.lake||t.river||t.iceHabitat>.01||t.stone>.01||t.volcanic>.01||t.giant>.01||t.water>=t.height||t.lava>=t.height||t.height<test.level||t.height>test.level+3)return false;
    }return true;
   };
   if(!sealed(p))return p;
   int wanted=r(8)<.45?(r(9)<.34?3:2):1;
   auto cluster=p;double angle=r(10)*6.283185307179586;
   double lastX=p.x,lastZ=p.z,lastRadius=p.radius;
   for(int i=0;i<wanted-1;++i){
    double radius=3+r(11+i)*3,step=(lastRadius+radius)*.70;
    lastX=roundJS(lastX+std::cos(angle)*step);lastZ=roundJS(lastZ+std::sin(angle)*step);
    cluster.lobes[i]={lastX,lastZ,radius};lastRadius=radius;angle+=(r(13+i)-.5)*.8;
   }
   cluster.count=wanted;
   if(wanted>1&&sealed(cluster))p=cluster;
  }
  p.valid=true;return p;});}
 void smallSurfacePool(Surface&s,int x,int z){
  if(evaluatingSmallPool||s.kind||s.lake||s.river||s.iceHabitat>.01||s.volcanic>.01||s.stone>.01||s.giant>.01||s.water>=s.height||s.lava>=s.height)return;
  int gx=int(std::floor(x/160.)),gz=int(std::floor(z/160.));
  int px=gx*160+24+int(hash01(gx,gz,seedHash(terrainSeed,9101))*112),pz=gz*160+24+int(hash01(gx,gz,seedHash(terrainSeed,9102))*112);
  if(std::abs(x-px)>(clusteredPools?28:9)||std::abs(z-pz)>(clusteredPools?28:9))return;
  auto p=smallPool(gx,gz);if(!p.valid||(clusteredPools&&!p.lava?s.height<p.level:s.height<=p.level)||std::abs(s.height-p.level)>4)return;
  for(int i=0;i<p.shoreCount;++i)if(p.shore[i][0]==x&&p.shore[i][1]==z){s.height=p.level;s.treeD=0;s.giant=0;s.rockCap=0;return;}
  double q=poolDistance(p,x,z);if(q>=1)return;
  int depth=q<.5?2:1;s.height=std::min(s.height,p.level-depth);s.treeD=0;s.giant=0;s.kind=1;s.surf=p.lava?0:5;
  if(p.lava)s.lava=p.level;else s.water=p.level;
 }
