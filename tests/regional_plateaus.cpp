#undef NDEBUG
#include "native/Engine.h"
#include "ChunkCodec.h"
#include "TestTeleportPolicy.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("recipe/report required");
 for(double u:{-4.,0.,4.}){double prev=-1;for(double h=46;h<260;h+=.01){double v=plateauRelief(h,u);assert(v>=prev);prev=v;}}
 for(double join:{70.,130.}){double e=.0001;double l=(plateauRelief(join,0)-plateauRelief(join-e,0))/e,r=(plateauRelief(join+e,0)-plateauRelief(join,0))/e;assert(std::abs(l-r)<.001);}
 assert(plateauHeight(150,150,90,0,1,0)==90&&plateauHeight(150,150,90,1,1,0)==90&&plateauHeight(150,150,90,.5,1,1)==90);
 J old;std::ifstream(argv[1])>>old;old["config"]["regionalPlateaus"]=0;J data=old;data["config"]["regionalPlateaus"]=1;
 Field before(old),after(data);J samples=J::array();std::vector<std::tuple<double,int,int>> candidates;int oldMid=0,newMid=0,oldGentle=0,newGentle=0,peak=0,bestX=0,bestZ=0;double best=-1;
 for(int z=-8192;z<=8192;z+=128)for(int x=-8192;x<=8192;x+=128){
  auto a=before.bootstrapSurface(x+32768,z+32768),b=after.bootstrapSurface(x+32768,z+32768);
  if(b.stone>.01||b.volcanic||b.badlands||b.giant>.01)continue;
  auto slope=[&](Field&f){return std::hypot(f.bootstrapSurface(x+32784,z+32768).height-f.bootstrapSurface(x+32752,z+32768).height,f.bootstrapSurface(x+32768,z+32784).height-f.bootstrapSurface(x+32768,z+32752).height)/32.;};
  bool am=a.height>=180&&a.height<=300,bm=b.height>=180&&b.height<=300;oldMid+=am;newMid+=bm;
  double sa=am?slope(before):-1,sb=bm?slope(after):-1;oldGentle+=am&&sa<=.25;newGentle+=bm&&sb<=.25;peak=std::max(peak,b.height);
  if(b.upland>.8&&bm&&sb<=.15){double score=b.upland-sb;candidates.emplace_back(score,x,z);if(score>best){best=score;bestX=x;bestZ=z;}}
  samples.push_back({x,z,a.height,b.height,b.upland,sa,sb});
 }
 J report{{"oldMiddle",oldMid},{"newMiddle",newMid},{"oldGentleMiddle",oldGentle},{"newGentleMiddle",newGentle},{"peak",peak},{"candidate",{bestX,bestZ}},{"samples",samples},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump();std::cout<<"middle "<<oldMid<<" -> "<<newMid<<" gentle "<<oldGentle<<" -> "<<newGentle<<" peak "<<peak<<" candidate "<<bestX<<","<<bestZ<<"\n";
 assert(newMid>oldMid&&newGentle>oldGentle&&peak>=470&&peak<500&&best>=0);
 std::sort(candidates.begin(),candidates.end(),std::greater<>());bool destination=false;
 for(auto [score,x,z]:candidates){
  if(test_teleport::plateauDestination(x,z,[&](int X,int Z){return after.surface(X+32768,Z+32768,false);})){bestX=x;bestZ=z;destination=true;break;}
 }
 assert(destination);int extent=0,maxStep=0;
 for(int dz=-256;dz<=256;dz+=32)for(int dx=-256;dx<=256;dx+=32){auto s=after.bootstrapSurface(bestX+dx+32768,bestZ+dz+32768);extent+=s.upland>.65&&s.height>=180&&s.height<=300;}
 assert(extent>=81); // At least nine 32-block cells per side in aggregate.
 for(int axis=0;axis<2;++axis){int prev=0;for(int d=-256;d<=256;++d){auto s=after.bootstrapSurface(bestX+32768+(axis?0:d),bestZ+32768+(axis?d:0));if(d>-256)maxStep=std::max(maxStep,std::abs(s.height-prev));prev=s.height;}}
 assert(maxStep<=8);
 // This ridge acquired nine-block steps when the highland profile was
 // evaluated on fine density noise. Keep the discovered location covered.
 if(data.at("seedManifest").at("terrainSeed")=="15"){
  int previous=after.bootstrapSurface(-896+32768,-7221+32768).height,edgeStep=0;
  for(int z=-7220;z<=-7170;++z){int h=after.bootstrapSurface(-896+32768,z+32768).height;edgeStep=std::max(edgeStep,std::abs(h-previous));previous=h;}
  assert(edgeStep<=3);report["formerSteepEdgeMaximumStep"]=edgeStep;
 }
 Engine engine(data,{},false,&after);int cx=int(std::floor(bestX/16.)),cz=int(std::floor(bestZ/16.));auto bytes=engine.chunk(cx,cz);auto decoded=terrain_runtime::decodeChunk(bytes,178,29,576);assert(decoded.height==576);engine.chunk(cx+1,cz);assert(engine.chunk(cx,cz)==bytes);
 int wet=0,faces=0;int gx=int(std::floor(bestX/2048.)),gz=int(std::floor(bestZ/2048.));
 for(int dz=-2;dz<=2&&wet<8;++dz)for(int dx=-2;dx<=2&&wet<8;++dx){
 auto plan=after.inspectRegionalDrainage(gx+dx,gz+dz);auto const&segments=plan.at("segments");
 for(size_t i=0;i<segments.size();i+=std::max(size_t(1),segments.size()/24)){
  int x=roundJS(segments[i][0])+32768,z=roundJS(segments[i][1])+32768;auto s=after.surface(x,z,false);if(s.water<=s.height)continue;
  assert(s.water<512);int m=after.material(s,x,s.water,z);assert(m==8||m==9);++wet;
  for(auto [dx,dz]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}){auto q=after.surface(x+dx,z+dz,false);faces+=q.water>q.height&&std::min(q.water,s.water)>std::max(q.height,s.height);}
 }
 }
 assert(wet>0&&faces>0);
 report["representativeDestination"]={bestX,bestZ};report["uplandExtentSamplesOf289"]=extent;report["maxOneBlockStep"]=maxStep;report["decodedHeight"]=decoded.height;report["chunkRevisitByteEqual"]=true;report["wetRiverSamples"]=wet;report["connectedWaterFaces"]=faces;
 std::ofstream(argv[2])<<report.dump();std::cout<<"destination "<<bestX<<","<<bestZ<<" extent "<<extent<<" step "<<maxStep<<" wet "<<wet<<'\n';
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
