// Compact anchors on surveyed scarps, shared by every intersecting chunk.
struct CliffTree {int x=0,z=0,y=0;double nx=0,nz=0,length=0;};
Cache<uint64_t,CliffTree> cliffTreeAnchors{1024};
CliffTree cliffTreeAnchor(int gx,int gz){return cliffTreeAnchors.get(key2(gx,gz),[&]{
 CliffTree tree;if(!cliffTreesEnabled||hash01(gx,gz,terrainSeed^89401)>.65)return tree;
 auto a=patch(gx,gz);if(!a.valid)return tree;
 double w=(hash01(gx,gz,terrainSeed^89403)-.5)*72,offset=cliff_transition::traceOffset(a.trace,w);
 int x=roundJS(a.x-a.nz*w+a.nx*(offset+2)),z=roundJS(a.z+a.nx*w+a.nz*(offset+2));
 auto top=surface(x,z,false),outside=surface(roundJS(x-a.nx*5),roundJS(z-a.nz*5),false);
 int drop=top.height-outside.height;
 if(drop<60||top.volcanic>0||top.iceHabitat>.15||top.water>=top.height||outside.water>=outside.height||top.H<.2)return tree;
 int y=top.height-12-int(hash01(gx,gz,terrainSeed^89405)*std::min(60,drop-30));
 if(material(top,x,y,z)!=2)return tree;
 return CliffTree{x,z,y,a.nx,a.nz,6+4*hash01(gx,gz,terrainSeed^89407)};
});}
std::vector<CliffTree> cliffTreeSites(int x0,int z0,int size){
 std::vector<CliffTree> out;if(!cliffTreesEnabled)return out;
 double grid=parameters.cliffs.grid;
 for(int gz=int(std::floor((z0-180)/grid));gz<=int(std::floor((z0+size+180)/grid));++gz)
 for(int gx=int(std::floor((x0-180)/grid));gx<=int(std::floor((x0+size+180)/grid));++gx){
  if(hash01(gx,gz,terrainSeed^89401)>.65)continue;
  auto p=patch(gx,gz);if(!p.valid)continue;
  double w=(hash01(gx,gz,terrainSeed^89403)-.5)*72,offset=cliff_transition::traceOffset(p.trace,w);
  int x=roundJS(p.x-p.nz*w+p.nx*(offset+2)),z=roundJS(p.z+p.nx*w+p.nz*(offset+2));
  // Do not build distant hydrology plans merely to reject a tree whose
  // entire footprint cannot touch this chunk.
  if(x<x0-14||x>=x0+size+14||z<z0-14||z>=z0+size+14)continue;
  auto a=cliffTreeAnchor(gx,gz);if(a.length&&a.x>=x0-14&&a.x<x0+size+14&&a.z>=z0-14&&a.z<z0+size+14)out.push_back(a);
 }
 return out;
}
