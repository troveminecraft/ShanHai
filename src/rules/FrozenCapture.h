#pragma once
#include "StructurePlan.h"
#include "StructureLifecycle.h"

namespace worldgen_rules {
// The upstream runs whole native features on bounded seeded scratch chunks.
// Only adapters own native objects. These opaque registered-state handles are
// returned to the same adapter; classification/equality never assume an ABI.
enum class FrozenCaptureKind {Portal,Pyramid};
enum class FrozenLootKind {Chest,Brushable};
struct FrozenCaptureSpec {
 FrozenCaptureKind kind;ChunkPosition anchor;uint32_t worldSeed{};
 StructureBox chunks,scan,placement;size_t pieceCount{},chunkCount{},cellCount{};
};
struct FrozenCaptureState {uintptr_t primary{},secondary{};bool operator==(FrozenCaptureState const&)const=default;};
struct FrozenCaptureWrite {StructurePoint position;FrozenCaptureState state;};
struct FrozenCaptureLoot {StructureLootAssignment assignment;FrozenLootKind kind;};
struct FrozenCaptureFragment {std::vector<FrozenCaptureWrite>writes;std::vector<FrozenCaptureLoot>loot;};
struct FrozenCaptureProgram {
 FrozenCaptureSpec spec;std::map<VillageXZ,FrozenCaptureFragment>owners;
 size_t writes{},chests{},brushable{},frames{},classifiedStates{};
};
inline FrozenCaptureSpec planFrozenCapture(FrozenCaptureKind kind,ChunkPosition anchor,uint32_t seed,StructureBox bounds,int floor,size_t pieces){
 if(!bounds.valid()||pieces<1||pieces>4096)throw std::runtime_error("Invalid native capture descriptor");
 int margin=kind==FrozenCaptureKind::Portal?16:8;
 auto checked=[](int64_t n){if(n<INT_MIN||n>INT_MAX)throw std::runtime_error("Native capture coordinate overflow");return int(n);};
 auto cell=[](int n){return int(n>=0?int64_t(n)/16:(int64_t(n)-15)/16);};
 int minX=cell(checked(int64_t(bounds.min.x)-margin)),maxX=cell(checked(int64_t(bounds.max.x)+margin));
 int minZ=cell(checked(int64_t(bounds.min.z)-margin)),maxZ=cell(checked(int64_t(bounds.max.z)+margin));
 int64_t chunks=(int64_t(maxX)-minX+1)*(int64_t(maxZ)-minZ+1);
 if(chunks<1||chunks>(kind==FrozenCaptureKind::Portal?64:2048))throw std::runtime_error("Native capture chunk limit exceeded");
 StructureBox full{{checked(int64_t(minX)*16),-64,checked(int64_t(minZ)*16)},{checked(int64_t(maxX)*16+15),319,checked(int64_t(maxZ)*16+15)}};
 if(full.min.x< -30000000||full.min.z< -30000000||full.max.x>=30000000||full.max.z>=30000000)throw std::runtime_error("Native capture footprint outside game coordinates");
 StructureBox scan=full;
 scan.min.y=int(std::max<int64_t>(-58,int64_t(kind==FrozenCaptureKind::Portal?floor:bounds.min.y)-24));
 scan.max.y=int(std::min<int64_t>(319,int64_t(kind==FrozenCaptureKind::Portal?floor:bounds.max.y)+(kind==FrozenCaptureKind::Portal?48:8)));
 if(!scan.valid())throw std::runtime_error("Native capture has no valid vertical span");
 int64_t cells=chunks*256*(int64_t(scan.max.y)-scan.min.y+1);
 // Bound both the native scratch footprint and the temporary before-image.
 if(cells>32*1024*1024)throw std::runtime_error("Native capture snapshot limit exceeded");
 return {kind,anchor,seed,full,scan,full,pieces,size_t(chunks),size_t(cells)};
}
inline uint32_t frozenCaptureSeed(FrozenCaptureSpec const&s,size_t piece){
 if(piece>=s.pieceCount)throw std::runtime_error("Native capture piece out of range");
 return s.kind==FrozenCaptureKind::Portal?extraPieceSeed(1,piece,s.anchor,s.anchor,s.worldSeed):placementSeed(s.anchor.x,s.anchor.z,14357617,s.worldSeed);
}
template<class F> void forEachFrozenCaptureChunk(FrozenCaptureSpec const&s,F fn){
 for(int z=spacingCell(s.chunks.min.z,16);z<=spacingCell(s.chunks.max.z,16);++z)for(int x=spacingCell(s.chunks.min.x,16);x<=spacingCell(s.chunks.max.x,16);++x)fn(ChunkPosition{x,z});
}
template<class F> void forEachFrozenCaptureCell(StructureBox const&box,F fn){
 for(int z=box.min.z;;++z){for(int x=box.min.x;;++x){for(int y=box.min.y;;++y){fn(StructurePoint{x,y,z});if(y==box.max.y)break;}if(x==box.max.x)break;}if(z==box.max.z)break;}
}
// read/equals/name/loot are native block services; place executes the complete
// native piece (portal) or original fresh start (pyramid). Nothing is published
// until all native placements, two-layer diffs and original loot validate.
template<class Read,class Place,class Equal,class Name,class Loot>
FrozenCaptureProgram captureFrozenStructure(FrozenCaptureSpec const&spec,Read read,Place place,Equal equal,Name name,Loot loot){
 std::vector<FrozenCaptureState>before;before.reserve(spec.cellCount);
 forEachFrozenCaptureCell(spec.scan,[&](auto p){auto s=read(p);if(!s.primary||!s.secondary)throw std::runtime_error("Native capture returned missing block state");before.push_back(s);});
 if(before.size()!=spec.cellCount)throw std::runtime_error("Native capture descriptor cell count mismatch");
 for(size_t i=0;i<(spec.kind==FrozenCaptureKind::Portal?spec.pieceCount:1);++i)if(!place(i,frozenCaptureSeed(spec,i),spec.placement))throw std::runtime_error("Whole native structure placement refused seeded ground");
 FrozenCaptureProgram out;out.spec=spec;size_t index=0;
 struct Class {bool frame=false;std::optional<FrozenLootKind>loot;};std::map<uintptr_t,Class>classes;
 forEachFrozenCaptureCell(spec.scan,[&](auto p){
  auto s=read(p);if(!s.primary||!s.secondary)throw std::runtime_error("Native capture returned missing final block state");
  auto key=VillageXZ{spacingCell(p.x,16),spacingCell(p.z,16)};
  if(!equal(before.at(index++),s)){out.owners[key].writes.push_back({p,s});++out.writes;}
  auto[it,added]=classes.try_emplace(s.primary);if(added){auto n=name(s.primary);it->second.frame=n=="minecraft:obsidian"||n=="minecraft:crying_obsidian";if(n=="minecraft:chest")it->second.loot=FrozenLootKind::Chest;else if(spec.kind==FrozenCaptureKind::Pyramid&&n=="minecraft:suspicious_sand")it->second.loot=FrozenLootKind::Brushable;}
  auto const&c=it->second;out.frames+=c.frame;
  if(c.loot){auto captured=loot(p,*c.loot);if(!captured||captured->assignment.position!=p||captured->kind!=*c.loot||captured->assignment.table.empty())throw std::runtime_error("Native captured container or original loot missing");
   out.owners[key].loot.push_back(std::move(*captured));if(*c.loot==FrozenLootKind::Chest)++out.chests;else ++out.brushable;}
 });
 out.classifiedStates=classes.size();
 if(spec.kind==FrozenCaptureKind::Portal&&(out.frames<4||!out.chests))throw std::runtime_error("Native portal frame or original chest incomplete");
 if(spec.kind==FrozenCaptureKind::Pyramid&&(out.chests!=4||!out.brushable))throw std::runtime_error("Native pyramid trap or archaeology loot incomplete");
 return out;
}
// Owner lookup is O(log(chunks)+local writes), with no full-feature scan or
// temporary allocation per fragment. Preflight bedrock before the first write.
template<class Bedrock,class Write,class Loot>
bool executeFrozenCapture(FrozenCaptureProgram const&program,StructureBox const&clip,Bedrock bedrock,Write write,Loot loot){
 if(!program.spec.scan.intersectsXZ(clip))return false;bool any=false;
 int minX=spacingCell(std::max(clip.min.x,program.spec.scan.min.x),16),maxX=spacingCell(std::min(clip.max.x,program.spec.scan.max.x),16);
 int minZ=spacingCell(std::max(clip.min.z,program.spec.scan.min.z),16),maxZ=spacingCell(std::min(clip.max.z,program.spec.scan.max.z),16);
 auto visit=[&](auto fn){for(int z=minZ;z<=maxZ;++z)for(int x=minX;x<=maxX;++x){auto it=program.owners.find({x,z});if(it!=program.owners.end())fn(it->second);}};
 visit([&](auto const&f){for(auto const&w:f.writes)if(inStructureOwner(w.position,clip)&&bedrock(w.position))throw std::runtime_error("Native structure fragment would replace bedrock");});
 visit([&](auto const&f){for(auto const&w:f.writes)if(inStructureOwner(w.position,clip)){if(!write(w))throw std::runtime_error("Native captured block write failed");any=true;}});
 // Even a multi-owner clip finishes every block layer before creating loot.
 visit([&](auto const&f){for(auto const&l:f.loot)if(inStructureOwner(l.assignment.position,clip)){loot(l);any=true;}});
 return any;
}
}
