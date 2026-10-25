#undef NDEBUG
#include "native/Engine.h"
#include "rules/StructurePlan.h"
#include "ChunkCodec.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("recipe/report required");
 J data;std::ifstream(argv[1])>>data;auto&c=data["config"];
 c["seaLevel"]=64;c["landscapeRevision"]=1;c["volcanoRevision"]=1;c["surfaceMoss"]=1;
 Field field(data);Engine engine(data,{},false,&field);
 assert(field.seaLevel()==64);
 assert(volcanicPlumbingRadius(-64,200,12,65)==0);
 assert(volcanicPlumbingRadius(-25,200,12,65)>volcanicPlumbingRadius(50,200,12,65)*3);
 assert(volcanicDiamondBonus(0,450)==1&&volcanicDiamondBonus(450,450)==0);
 std::array<int,7> tiers{};int land=0,maximum=0;J samples=J::array();
 for(int z=-8192;z<=8192;z+=256)for(int x=-8192;x<=8192;x+=256){
  auto s=field.bootstrapSurface(x+32768,z+32768);if(s.height<=64||s.stone||s.volcanic)continue;
  ++land;maximum=std::max(maximum,s.height);int t=s.height<110?0:s.height<180?1:s.height<260?2:s.height<360?3:s.height<440?4:s.height<480?5:6;++tiers[t];
  assert(s.height<512);samples.push_back({x,z,s.height,s.upland});
 }
 // A 256-block lattice can miss narrow peaks. Refine the highest sampled
 // ridges before judging the summit range, without changing the recipe.
 std::vector<std::array<int,3>> peaks;for(auto const&p:samples)peaks.push_back({int(p[2]),int(p[0]),int(p[1])});
 std::sort(peaks.begin(),peaks.end(),std::greater<>());
 for(size_t i=0;i<std::min(size_t(8),peaks.size());++i)for(int dz=-128;dz<=128;dz+=16)for(int dx=-128;dx<=128;dx+=16){
  auto s=field.bootstrapSurface(peaks[i][1]+dx+32768,peaks[i][2]+dz+32768);if(!s.volcanic&&!s.stone)maximum=std::max(maximum,s.height);
 }
 auto volcanoes=field.inspectVolcanoes(-8192,8192);assert(!volcanoes.empty());
 auto v=volcanoes[0];int X=int(v["x"]),Z=int(v["z"]);int deepslate=0,lava=0,air=0;
 for(int dz=-256;dz<=256;dz+=16)for(int dx=-256;dx<=256;dx+=16){
  auto s=field.surface(X+dx+32768,Z+dz+32768,false);
  for(int y=-59;y<s.height-8;y+=2){int b=field.material(s,X+dx+32768,y,Z+dz+32768);deepslate+=b==95;lava+=b==43;air+=b==0;}
 }
 assert(deepslate>0&&lava>0&&air>0);
 int cx=int(std::floor(X/16.)),cz=int(std::floor(Z/16.));
 auto bytes=engine.chunk(cx,cz);auto decoded=terrain_runtime::decodeChunk(bytes,178,29,576);assert(decoded.height==576);
 engine.chunk(cx+1,cz);assert(engine.chunk(cx,cz)==bytes);
 int oldSites=0,newSites=0;uint32_t seed=c.at("seed");
 auto sample=[&](int x,int z){return field.surface(x+32768,z+32768,false);};
 for(int z=-5;z<=5;++z)for(int x=-5;x<=5;++x){
  auto old=worldgen_rules::candidateForCell(x,z,seed,32,8,worldgen_rules::VillageSalt);
  auto next=worldgen_rules::villageCandidateForTerrain(x,z,seed,sample);
  oldSites+=worldgen_rules::villageSiteStyle(old,sample)>=0;newSites+=worldgen_rules::villageSiteStyle(next,sample)>=0;
  assert(worldgen_rules::spacingCell(next.x,32)==x&&worldgen_rules::spacingCell(next.z,32)==z);
 }
 assert(newSites>oldSites);
 J report{{"landSamples",land},{"heightTiers",tiers},{"maxHeight",maximum},{"oldVillageSites",oldSites},{"newVillageSites",newSites},{"volcano",v},{"deepRockSamples",deepslate},{"magmaSamples",lava},{"caveAirSamples",air},{"chunkRevisitByteEqual",true},{"gameplayTested",false},{"samples",samples}};
 std::ofstream(argv[2])<<report.dump();std::cout<<"tiers "<<J(tiers)<<" max "<<maximum<<" villages "<<oldSites<<" -> "<<newSites<<" geology "<<deepslate<<","<<lava<<","<<air<<'\n';
 for(int i=0;i<5;++i)assert(tiers[i]>0);assert(maximum>=470&&maximum<512);
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
