#include "rules/FrozenCapture.h"
#include "rules/FrozenGround.h"
#include <fstream>
#include <iostream>
using namespace worldgen_rules;
using J=nlohmann::json;using Key=std::array<int,3>;
void check(bool v,char const*why){if(!v)throw std::runtime_error(why);}
Key key(StructurePoint p){return {p.x,p.y,p.z};}
std::string_view name(uintptr_t h){switch(h){case 1:case 101:return "minecraft:stone";case 2:return "minecraft:air";case 3:return "minecraft:water";case 4:return "minecraft:obsidian";case 5:return "minecraft:crying_obsidian";case 6:return "minecraft:chest";case 7:return "minecraft:suspicious_sand";case 8:return "minecraft:netherrack";}throw std::runtime_error("unknown fixture state");}
bool equal(FrozenCaptureState a,FrozenCaptureState b){return name(a.primary)==name(b.primary)&&name(a.secondary)==name(b.secondary);}
struct Fixture {
 FrozenCaptureSpec spec;int floor;std::map<Key,FrozenCaptureState>native;
 std::map<Key,FrozenCaptureLoot>loot;size_t reads{},names{},nativeCalls{};std::vector<uint32_t>seeds;
 FrozenCaptureState original(StructurePoint p)const{return {uintptr_t(p.y<=floor?1:2),2};}
 FrozenCaptureState read(StructurePoint p){++reads;auto i=native.find(key(p));return i==native.end()?original(p):i->second;}
 void put(int x,int y,int z,uintptr_t main,uintptr_t extra=2){native[{x,y,z}]={main,extra};}
 bool place(size_t piece,uint32_t seed,StructureBox const&clip){
  ++nativeCalls;seeds.push_back(seed);check(clip.min==spec.placement.min&&clip.max==spec.placement.max,"native whole-feature clip changed");
  check(seed==frozenCaptureSeed(spec,piece),"native capture seed changed");
  int x=spec.anchor.x*16,z=spec.anchor.z*16,y=std::clamp(floor,-55,305);
  if(piece==0){for(int i=0;i<4;++i)put(x+i,y+1,z,4+i%2);
   int containers=spec.kind==FrozenCaptureKind::Portal?1:4;
   for(int i=0;i<containers;++i){StructurePoint p{x-1+i*9,y,z-1};put(p.x,p.y,p.z,6);loot[key(p)]={{p,"loot_tables/chests/original_native.json",uint32_t(seed+i)},FrozenLootKind::Chest};}
   if(spec.kind==FrozenCaptureKind::Pyramid)for(int i=0;i<2;++i){StructurePoint p{x+i,y-3,z+17};put(p.x,p.y,p.z,7);loot[key(p)]={{p,"loot_tables/archaeology/original_native.json",uint32_t(seed+i+9)},FrozenLootKind::Brushable};}
   put(x+16,y-1,z+1,8);put(x-3,y-1,z+2,2);put(x+2,y-1,z+2,1,3); // spread, removal, secondary-only
   put(x+3,y-1,z+3,101); // native-equal alternative handle must NOT be a diff
  }else{put(x+17,y-2,z+3,8,3);}
  return true;
 }
 FrozenCaptureProgram capture(){return captureFrozenStructure(spec,[&](auto p){return read(p);},[&](auto i,auto seed,auto const&clip){return place(i,seed,clip);},equal,[&](uintptr_t h){++names;return name(h);},[&](auto p,auto kind)->std::optional<FrozenCaptureLoot>{auto i=loot.find(key(p));return i==loot.end()?std::optional<FrozenCaptureLoot>{}:i->second;});}
};
J writes(FrozenCaptureProgram const&p){J out=J::array();std::map<Key,FrozenCaptureState>ordered;for(auto const&[owner,f]:p.owners)for(auto const&w:f.writes)ordered[key(w.position)]=w.state;for(auto const&[p,s]:ordered)out.push_back({p[0],p[1],p[2],s.primary,s.secondary});return out;}
struct Published {std::map<Key,FrozenCaptureState>states;std::map<Key,FrozenCaptureLoot>loot;size_t probes{},calls{};};
Published run(FrozenCaptureProgram const&program,std::vector<StructureBox>const&clips){Published out;
 for(auto const&clip:clips){size_t expected=0;for(auto const&[owner,f]:program.owners)for(auto const&w:f.writes)if(inStructureOwner(w.position,clip))++expected;size_t probed=out.probes;
  executeFrozenCapture(program,clip,[&](auto p){++out.probes;check(inStructureOwner(p,clip),"publication read neighbour owner");return false;},[&](auto const&w){++out.calls;out.states[key(w.position)]=w.state;return true;},[&](auto const&l){
   for(auto const&[owner,f]:program.owners)for(auto const&w:f.writes)if(inStructureOwner(w.position,clip))check(out.states.contains(key(w.position)),"loot published before all final primary/secondary layers");
   check(inStructureOwner(l.assignment.position,clip),"loot touched neighbour owner");out.loot[key(l.assignment.position)]=l;});
  check(out.probes-probed==expected,"fragment scanned unrelated captured writes");
 }return out;
}
bool same(Published const&a,Published const&b){if(a.states!=b.states||a.loot.size()!=b.loot.size())return false;for(auto const&[p,l]:a.loot){auto i=b.loot.find(p);if(i==b.loot.end()||l.assignment.table!=i->second.assignment.table||l.assignment.seed!=i->second.assignment.seed||l.kind!=i->second.kind)return false;}return true;}
struct GroundField {
 struct Surface{int height,biome;};size_t surfaces{},columns{};
 Surface surface(int x,int z,bool slope=true){check(slope,"capture baseline disabled upstream rock/slope sampling");++surfaces;return {48+int((uint32_t(x)^uint32_t(z))%31),int((uint32_t(x)*3+uint32_t(z))%19)};}
 void materialColumn(Surface const&s,int x,int z,int top,uint16_t*p){check(top==319,"capture geology truncated original vertical range");++columns;for(int y=-60;y<=top;++y)p[y+64]=uint16_t(y>s.height?(y<72?8:0):y==s.height?4:((y==-23&&((x^z)&1))?0:2));}
};
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("report.json");J cases=J::array(),groundCases=J::array(),fittedColumns=J::array();size_t cells=0,changes=0;
 for(int biome:{1,11})for(int target:{58,61,64}){J common=J::array(),old=J::array();writeStructureGroundColumn(-18,21,61,target,biome,[&](auto p,auto b){common.push_back({p.x,p.y,p.z,b.name});});
  if(target!=61){auto fill=biome==11?"minecraft:sandstone":"minecraft:dirt",top=biome==11?"minecraft:sand":"minecraft:grass_block";if(target>61)for(int y=61;y<target;++y)old.push_back({-18,y,21,fill});else for(int y=target+1;y<=63;++y)old.push_back({-18,y,21,"minecraft:air"});old.push_back({-18,target,21,top});}check(common==old,"fitted column differs from original LLBDS ground adjustment");fittedColumns.push_back({{"biome",biome},{"target",target},{"writes",common}});
 }
 for(auto cp:std::array<ChunkPosition,4>{{{0,0},{-3,-2},{20,5},{-8,7}}}){GroundField field,old;auto ground=buildFrozenCaptureGround(field,cp);check(field.surfaces==256&&field.columns==256,"capture ground sampled a column more than once");uint64_t hash=1469598103934665603ull;
  for(int z=0;z<16;++z)for(int x=0;x<16;++x){int wx=cp.x*16+x+32768,wz=cp.z*16+z+32768;auto s=old.surface(wx,wz);std::array<uint16_t,384>column{};std::fill_n(column.begin(),4,uint16_t(1));old.materialColumn(s,wx,wz,319,column.data());check(ground.biomes[z*16+x]==s.biome&&ground.heights[z*16+x]==s.height+1,"capture biome/height baseline changed");for(int y=0;y<384;++y){auto b=ground.blocks[(z*16+x)*384+y];check(b==column[y],"shared capture geology differs from upstream full column");hash=(hash^b)*1099511628211ull;}}
  groundCases.push_back({{"chunk",{cp.x,cp.z}},{"statesHash",std::to_string(hash)},{"surfaceQueries",field.surfaces},{"materialColumns",field.columns}});
 }
 for(auto kind:{FrozenCaptureKind::Portal,FrozenCaptureKind::Pyramid})for(int i=0;i<4;++i){int x=i%2?-3:7,z=i%2?-2:2,floor=std::array{0,53,310,-50}[i];uint32_t seed=std::array<uint32_t,4>{0,42,0xfedcba98u,0xffffffffu}[i];
  StructureBox bounds{{x*16-2,floor-4,z*16-2},{x*16+26,std::min(319,floor+7),z*16+22}};auto spec=planFrozenCapture(kind,{x,z},seed,bounds,floor,kind==FrozenCaptureKind::Portal?2:3);Fixture f{spec,floor};auto program=f.capture();
  std::vector<Key>chunkOrder;forEachFrozenCaptureChunk(spec,[&](auto cp){chunkOrder.push_back({cp.z,cp.x,0});});check(chunkOrder.size()==spec.chunkCount&&std::is_sorted(chunkOrder.begin(),chunkOrder.end()),"shared scratch chunk order/count changed");
  check(f.reads==spec.cellCount*2,"capture did not snapshot/compare exactly once");check(f.names==program.classifiedStates&&f.names<16,"block names decoded per voxel instead of per state");check(f.nativeCalls==(kind==FrozenCaptureKind::Portal?2:1),"native whole start/piece execution count changed");
  J old=J::array();forEachFrozenCaptureCell(spec.scan,[&](auto p){auto final=f.read(p);if(!equal(f.original(p),final))old.push_back({p.x,p.y,p.z,final.primary,final.secondary});});
  auto sorted=[](J v){std::sort(v.begin(),v.end());return v;};check(sorted(old)==sorted(writes(program)),"shared capture differs from upstream two-layer diff");
  std::vector<StructureBox>clips;for(auto const&[owner,rows]:program.owners)clips.push_back({{owner.first*16,-64,owner.second*16},{owner.first*16+15,319,owner.second*16+15}});auto a=run(program,clips);std::reverse(clips.begin(),clips.end());auto b=run(program,clips);check(same(a,b),"native frozen deltas depend on owner generation order");
  auto whole=run(program,{{{INT_MIN,-64,INT_MIN},{INT_MAX,319,INT_MAX}}});check(same(a,whole),"whole caller clip differs from separate fragment generation");
  std::vector<StructureBox>partial;for(auto clip:clips){int mid=clip.min.x+7;auto right=clip;clip.max.x=mid;right.min.x=mid+1;partial.push_back(clip);partial.push_back(right);}auto split=run(program,partial);check(same(a,split),"partial owner clips lost a block or container");
  Fixture again{spec,floor};auto rebuilt=again.capture();check(writes(program)==writes(rebuilt),"seeded capture reconstruction changed deltas");
  size_t outside=0;executeFrozenCapture(program,{{4000,-64,4000},{4015,319,4015}},[&](auto){++outside;return false;},[&](auto const&){++outside;return true;},[&](auto const&){++outside;});check(!outside,"non-overlap fragment called native services");
  size_t mutated=0;bool rejected=false;try{executeFrozenCapture(program,spec.placement,[&](auto){return true;},[&](auto const&){++mutated;return true;},[&](auto const&){++mutated;});}catch(std::exception const&){rejected=true;}check(rejected&&!mutated,"bedrock rejection published a partial program");
  cells+=spec.cellCount;changes+=program.writes;cases.push_back({{"kind",kind==FrozenCaptureKind::Portal?"portal":"pyramid"},{"seed",seed},{"floor",floor},{"anchor",{x,z}},{"chunks",spec.chunkCount},{"scan",structureBoxJson(spec.scan)},{"nativeSeeds",f.seeds},{"writes",writes(program)},{"chests",program.chests},{"brushable",program.brushable},{"frames",program.frames},{"classifiedStates",program.classifiedStates}});
 }
 auto expectReject=[](auto fn){bool rejected=false;try{fn();}catch(std::exception const&){rejected=true;}check(rejected,"invalid native capture accepted");};
 StructureBox box{{0,50,0},{20,70,20}};auto spec=planFrozenCapture(FrozenCaptureKind::Portal,{0,0},42,box,60,1);Fixture failed{spec,60};
 expectReject([&]{captureFrozenStructure(spec,[&](auto p){return failed.read(p);},[](auto,auto,auto const&){return false;},equal,name,[](auto,auto)->std::optional<FrozenCaptureLoot>{return {};});});
 expectReject([&]{captureFrozenStructure(spec,[&](auto p){return failed.read(p);},[](auto,auto,auto const&){return true;},equal,name,[](auto,auto)->std::optional<FrozenCaptureLoot>{return {};});});
 Fixture missing{spec,60};expectReject([&]{captureFrozenStructure(spec,[&](auto p){return missing.read(p);},[&](auto i,auto seed,auto const&clip){return missing.place(i,seed,clip);},equal,name,[](auto,auto)->std::optional<FrozenCaptureLoot>{return {};});});
 expectReject([&]{planFrozenCapture(FrozenCaptureKind::Portal,{0,0},42,{{0,0,0},{4096,100,4096}},60,1);});
 expectReject([&]{planFrozenCapture(FrozenCaptureKind::Portal,{0,0},42,{{INT_MIN,0,0},{INT_MIN+1,100,1}},60,1);});
 expectReject([&]{planFrozenCapture(FrozenCaptureKind::Pyramid,{0,0},42,box,60,0);});
 J result={{"passed",true},{"cases",cases},{"groundCases",groundCases},{"fittedColumns",fittedColumns},{"snapshotCells",cells},{"totalWrites",changes},{"upstreamTwoLayerDiffEqual",true},{"oppositeOrderEqual",true},{"seededReconstructionEqual",true},{"secondaryLayerAndAirRemovalRetained",true},{"namesDecodedOncePerNativeState",true},{"ownerLocalPublication",true},{"bedrockPreflightBeforeAnyWrite",true},{"failedPlacementOrLootNotPublished",true},{"boundsAndOverflowRejected",true},{"nativeScratchObjectsExecuted",false},{"nativeTemplatePlacementExecuted",false},{"actualNativeLootExecuted",false},{"nativeReloadVerified",false},{"gameplayTestPerformed",false}};
 std::ofstream out(argv[1]);out<<result.dump(2)<<'\n';if(!out)throw std::runtime_error("report failed");std::cout<<"Shared frozen capture, upstream diff, owner publication and rejection checks passed\n";return 0;
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
