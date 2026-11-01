#undef NDEBUG
#include "rules/OrderedVillage.h"
#include <cassert>
#include <iostream>
#include <chrono>
using namespace worldgen_rules;
struct Surface {int height=140,water=100,biome=12;double lake=0,river=0,volcanic=0,stone=0,iceHabitat=0;};
VillageBlueprint village(int houses){VillageBlueprint b{{0,0},35,12,{}};
 for(int i=0;i<houses;++i){StructureBlueprintPiece p;p.box={{0,140,0},{7+i%5,148,9+i%4}};p.origin={0,140,0};p.location="village/savanna/houses/house";
  for(int z=0;z<=p.box.max.z;++z)for(int x=0;x<=p.box.max.x;++x){p.cells.push_back({{x,0,z},{"minecraft:grass_block"}});p.cells.push_back({{x,1,z},{"minecraft:air"}});}
  p.cells.push_back({{3,1,1},{"minecraft:acacia_door"}});p.cells.push_back({{3,2,3},{"minecraft:structure_block"},{},"minecraft:villager_v2"});b.pieces.push_back(std::move(p));
 }return b;}
int main(){auto start=std::chrono::steady_clock::now();
 for(int count:{8,24,49,80})for(int slope:{0,1}){auto b=village(count);auto sample=[&](int x,int){auto s=Surface{};s.height+=slope*int(std::floor(x/32.));return s;};
  auto ordered=orderVillage(b,sample);if(!ordered.accepted)std::cerr<<count<<" slope="<<slope<<" reason="<<ordered.rejection<<std::endl;
  assert(ordered.accepted&&ordered.target==count*6&&ordered.lots.size()==size_t(count*6));
  auto again=orderVillage(b,sample);assert(ordered.roads==again.roads&&ordered.lots.size()==again.lots.size());
  VillageBlueprint next{b.anchor,b.seed,b.biome,{}};next.plannedRoads=ordered.roads;next.reach=OrderedVillageReach;
  for(auto const&lot:ordered.lots){auto p=b.pieces[lot.prototype];p.origin.x+=lot.delta.x;p.origin.y+=lot.delta.y;p.origin.z+=lot.delta.z;p.box=lot.box;p.populate=lot.populate;
   for(auto const&q:next.pieces)assert(!p.box.intersectsXZ(q.box,2));next.pieces.push_back(std::move(p));}
  auto plan=planVillageLayout(next,sample,[](int,int,uint32_t){return .5;},511);assert(plan.accepted);
  size_t population=0;for(auto const&[cp,p]:plan.population)population+=p.size();assert(population==size_t(count));
  std::set<VillageXZ> seen;std::vector<VillageXZ> queue{ordered.roads.begin()->first};seen.insert(queue[0]);
  for(size_t i=0;i<queue.size();++i)for(auto [dx,dz]:{VillageXZ{1,0},{-1,0},{0,1},{0,-1}}){VillageXZ q{queue[i].first+dx,queue[i].second+dz};if(ordered.roads.contains(q)&&seen.insert(q).second)queue.push_back(q);}
  assert(seen.size()==ordered.roads.size());
  for(auto const&[xz,unused]:ordered.roads)for(auto [dx,dz]:{VillageXZ{1,0},{0,1}}){
   VillageXZ q{xz.first+dx,xz.second+dz};if(ordered.roads.contains(q))
    assert(std::abs(plan.ground.fitted.at(xz)-plan.ground.fitted.at(q))<=1);
  }
  for(auto const&[owner,ops]:plan.placements)for(auto const&o:ops){auto box=next.pieces[o.index].box.moved(plan.verticalMoves[o.index]);assert(box.intersectsXZ({{owner.first*16,-64,owner.second*16},{owner.first*16+15,511,owner.second*16+15}}));}
  std::cout<<"houses="<<count<<" -> "<<ordered.target<<" slope="<<slope<<" roadColumns="<<ordered.roads.size()<<" survey="<<ordered.surfaceQueries<<std::endl;
 }
 for(int kind=0;kind<3;++kind){auto bad=orderVillage(village(8),[&](int x,int){auto s=Surface{};if(kind==0)s.water=145;if(kind==1)s.biome=13;if(kind==2)s.height=x<0?120:180;return s;});assert(!bad.accepted);}
 auto candidate=[](int x,int z){return ChunkPosition{x*16+4,z*16+4};};
 auto flat=[](int,int){return Surface{};};std::vector<ChunkPosition> towns;
 for(int z=-8;z<=8;++z)for(int x=-8;x<=8;++x){auto p=candidate(x,z);
  if(villageTownOwner(p,35,16,candidate,flat))towns.push_back(p);
 }
 assert(!towns.empty());
 for(size_t i=0;i<towns.size();++i)for(size_t j=i+1;j<towns.size();++j)
  assert(std::abs(towns[i].x-towns[j].x)*16>=520||std::abs(towns[i].z-towns[j].z)*16>=520);
 std::reverse(towns.begin(),towns.end());for(auto p:towns)assert(villageTownOwner(p,35,16,candidate,flat));
 std::cout<<"ordered villages passed seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;
}
