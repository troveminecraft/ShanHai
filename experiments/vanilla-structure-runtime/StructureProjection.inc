thread_local int shipFloor=-32768,portalFloor=-32768,ruinFloor=-32768;
thread_local BlockSource* portalWriteSource=nullptr;
thread_local BoundingBox const* portalWriteClip=nullptr;
thread_local BoundingBox const* mineshaftQueryClip=nullptr;
std::vector<Block const*> auditPalette();
#include "WorldScopedCache.h"
WorldScopedCache<std::vector<Block const*>> structurePalette;
std::vector<Block const*> const& currentStructurePalette(){return structurePalette.get([]{return auditPalette();});}
thread_local BlockSource* seededReadSource=nullptr;
thread_local BoundingBox const* seededReadClip=nullptr;
thread_local LevelChunk* nativeFragmentOwner=nullptr;
thread_local worldgen_rules::ScatteredSeededReadCache<Block const*>* scatteredReadCache=nullptr;
#ifdef SHANHAI_MC_26_51
bool captureNativeSimpleWrite(LevelChunk&,ChunkBlockPos const&,Block const&);
void rememberNativeSimpleWrite(LevelChunk&,ChunkBlockPos const&,Block const&);
#endif
LL_TYPE_INSTANCE_HOOK(FragmentChunkWrite,ll::memory::HookPriority::Normal,LevelChunk,&LevelChunk::setBlock,Block const&,ChunkBlockPos const&pos,Block const&block,BlockSource*source,std::shared_ptr<BlockActor> actor,BlockChangeContext const&context){if(nativeFragmentOwner&&this!=nativeFragmentOwner)return getBlock(pos);return origin(pos,block,source,std::move(actor),context);}
LL_TYPE_INSTANCE_HOOK(FragmentChunkFastWrite,ll::memory::HookPriority::Normal,LevelChunk,&LevelChunk::setBlockSimple,void,ChunkBlockPos const&pos,Block const&block){
#ifdef SHANHAI_MC_26_51
 if(captureNativeSimpleWrite(*this,pos,block))return;
#endif
 if(nativeFragmentOwner&&this!=nativeFragmentOwner)return;origin(pos,block);
#ifdef SHANHAI_MC_26_51
 rememberNativeSimpleWrite(*this,pos,block);
#endif
}
bool seededNeighbourRead(BlockSource const*s,BlockPos const&p){return s==seededReadSource&&seededReadClip&&worldgen_rules::scatteredSeededRead(sharedBox(*seededReadClip),{p.x,p.y,p.z});}
Block const&seededNeighbourBlock(BlockPos const&p){auto const&palette=currentStructurePalette();auto query=[&](auto q){auto s=field->surface(q.x+32768,q.z+32768);return palette.at(field->material(s,q.x+32768,q.y,q.z+32768));};return *(scatteredReadCache?scatteredReadCache->get({p.x,p.y,p.z},query):query(worldgen_rules::StructurePoint{p.x,p.y,p.z}));}
LL_TYPE_INSTANCE_HOOK(SeededStructureRead,ll::memory::HookPriority::Normal,BlockSource,(Block const&(BlockSource::*)(BlockPos const&)const)&BlockSource::$getBlock,Block const&,BlockPos const&p){if(seededNeighbourRead(this,p))return seededNeighbourBlock(p);return origin(p);}
LL_TYPE_INSTANCE_HOOK(SeededStructureLayer,ll::memory::HookPriority::Normal,BlockSource,(Block const&(BlockSource::*)(BlockPos const&,uint)const)&BlockSource::$getBlock,Block const&,BlockPos const&p,uint layer){if(seededNeighbourRead(this,p)){if(!layer)return seededNeighbourBlock(p);return *currentStructurePalette().at(0);}return origin(p,layer);}
LL_TYPE_INSTANCE_HOOK(SeededStructureSolid,ll::memory::HookPriority::Normal,BlockSource,(bool(BlockSource::*)(BlockPos const&)const)&BlockSource::$isSolidBlockingBlock,bool,BlockPos const&p){if(seededNeighbourRead(this,p))return bedrock_compat::solidBlock(seededNeighbourBlock(p));return origin(p);}
class MineNeighbourSource final:public bedrock_compat::BlockSourceView {
 BlockSource const&source;BoundingBox const&clip;std::vector<Block const*>const&palette;
public:
 MineNeighbourSource(BlockSource&s,BoundingBox const&b):BlockSourceView(s.getChunkSource(),false,true),source(s),clip(b),palette(currentStructurePalette()){}
 Block const&getBlock(BlockPos const&p)const override{if(p.x>=clip.min.x&&p.x<=clip.max.x&&p.z>=clip.min.z&&p.z<=clip.max.z)return source.getBlock(p);auto query=[&](auto q){auto s=field->surface(q.x+32768,q.z+32768);return palette.at(field->material(s,q.x+32768,q.y,q.z+32768));};return *(scatteredReadCache?scatteredReadCache->get({p.x,p.y,p.z},query):query(worldgen_rules::StructurePoint{p.x,p.y,p.z}));}
 Block const&getBlock(BlockPos const&p,uint layer)const override{if(!layer)return getBlock(p);if(p.x<clip.min.x||p.x>clip.max.x||p.z<clip.min.z||p.z>clip.max.z)return *palette[0];return source.getExtraBlock(p);}
 bool isSolidBlockingBlock(BlockPos const&p)const override{return bedrock_compat::solidBlock(getBlock(p));}
 bool isSolidBlockingBlock(int x,int y,int z)const override{return isSolidBlockingBlock({x,y,z});}
};
#ifndef SHANHAI_MC_26_51
LL_TYPE_INSTANCE_HOOK(MineSupport,ll::memory::HookPriority::Normal,MineshaftPiece,&MineshaftPiece::_isSupportingBox,bool,int x0,int x1,BlockSource&region,int y1,int z){if(!mineshaftQueryClip)return origin(x0,x1,region,y1,z);MineNeighbourSource view(region,*mineshaftQueryClip);return origin(x0,x1,view,y1,z);}
#endif
// 26.51 inlines support checks into corridor postProcess, but still dispatches
// BlockSource::getBlock virtually. SeededStructureRead covers those calls with
// the same clipped seed terrain, without replacing the write source.
LL_TYPE_INSTANCE_HOOK(MineWeb,ll::memory::HookPriority::Normal,MineshaftCorridor,&MineshaftCorridor::_placeCobWeb,void,BlockSource&region,BoundingBox const&clip,Random&random,float chance,int x,int y,int z){if(!mineshaftQueryClip){origin(region,clip,random,chance,x,y,z);return;}MineNeighbourSource view(region,*mineshaftQueryClip);origin(view,clip,random,chance,x,y,z);}
thread_local std::set<std::array<int,3>>* structureWallWrites=nullptr;
void rememberStructureWall(BlockPos const&p,Block const&block){if(structureWallWrites&&block.getTypeName().ends_with("_wall"))structureWallWrites->insert({p.x,p.y,p.z});}
bool outsidePortalFragment(BlockSource* source,BlockPos const&p){
 if(source!=portalWriteSource||!portalWriteClip)return false;
 return !worldgen_rules::scatteredFragmentWriteOwned(sharedBox(*portalWriteClip),{p.x,p.y,p.z});
}
#ifdef SHANHAI_MC_26_51
// Ocean ruin height selection is now inlined. The native program still calls
// TemplateStructurePiece::postProcess after selecting height and archaeology
// rules. Correct only that ruin's final template origin at this boundary.
thread_local TemplateStructurePiece* projectedRuin=nullptr;
LL_TYPE_INSTANCE_HOOK(RuinPieceScope,ll::memory::HookPriority::Normal,OceanRuinPieces::OceanRuinPiece,
 &OceanRuinPieces::OceanRuinPiece::$postProcess,bool,BlockSource& region,Random& random,BoundingBox const& clip){
 auto* previous=projectedRuin;projectedRuin=ruinFloor>-32768?this:nullptr;
 struct Reset{TemplateStructurePiece* previous;~Reset(){projectedRuin=previous;}}reset{previous};
 return origin(region,random,clip);
}
LL_TYPE_INSTANCE_HOOK(RuinGround,ll::memory::HookPriority::Normal,TemplateStructurePiece,
 &TemplateStructurePiece::$postProcess,bool,BlockSource& region,Random& random,BoundingBox const& clip){
 if(this==projectedRuin)mTemplatePosition->y=ruinFloor-1;
 return origin(region,random,clip);
}
#else
LL_TYPE_INSTANCE_HOOK(RuinGround,ll::memory::HookPriority::Normal,OceanRuinPieces::OceanRuinPiece,&OceanRuinPieces::OceanRuinPiece::_getHeight,int,BlockPos& pos,BlockSource& region,BlockPos const& corner){
 if(ruinFloor>-32768)return ruinFloor;return origin(pos,region,corner);
}
LL_STATIC_HOOK(ShipGround,ll::memory::HookPriority::Normal,&ShipwreckPiece::_calculateTargetPos,BlockPos,BlockSource& region,BlockPos pos,Rotation rot,BlockPos size){
 auto result=origin(region,pos,rot,size);if(shipFloor>-32768)result.y=shipFloor;return result;
}
LL_STATIC_HOOK(ShipGroundLegacy,ll::memory::HookPriority::Normal,&ShipwreckPiece::_calculateTargetPosLegacy,BlockPos,BlockSource& region,BlockPos pos,Rotation rot,BlockPos size){
 auto result=origin(region,pos,rot,size);if(shipFloor>-32768)result.y=shipFloor;return result;
}
#endif
// Both game lines use FrozenShipStart for every ShanHai ship. Its template
// origin already carries the projected floor; no procedural ship hook is
// needed on 26.51. The legacy hooks above remain for the older ABI.
#ifdef SHANHAI_MC_26_51
bedrock_compat::NativePortalPatch portalGroundPatch;
bool projectPortalGround(RuinedPortalPiece* piece)noexcept{
 if(portalFloor<=-32768)return false;
 int dy=portalFloor-piece->mStructureBounds->min.y;
 piece->mStructureBounds->min.y+=dy;piece->mStructureBounds->max.y+=dy;return true;
}
struct PortalGround {
 static int hook(){portalGroundPatch.install(reinterpret_cast<void*>(&projectPortalGround));return 0;}
 static void unhook(){portalGroundPatch.remove();}
};
#else
LL_TYPE_INSTANCE_HOOK(PortalGround,ll::memory::HookPriority::Normal,RuinedPortalPiece,&RuinedPortalPiece::_moveStructureBoundsToSuitableY,void,Random& random,BlockSource& region,PortalEnvironmentModifiers const& modifiers){
 if(portalFloor>-32768){int dy=portalFloor-mStructureBounds->min.y;mStructureBounds->min.y+=dy;mStructureBounds->max.y+=dy;return;}origin(random,region,modifiers);
}
#endif
