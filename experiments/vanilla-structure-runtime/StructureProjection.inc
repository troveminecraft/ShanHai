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
LL_TYPE_INSTANCE_HOOK(FragmentChunkWrite,ll::memory::HookPriority::Normal,LevelChunk,&LevelChunk::setBlock,Block const&,ChunkBlockPos const&pos,Block const&block,BlockSource*source,std::shared_ptr<BlockActor> actor,BlockChangeContext const&context){if(nativeFragmentOwner&&this!=nativeFragmentOwner)return getBlock(pos);return origin(pos,block,source,std::move(actor),context);}
LL_TYPE_INSTANCE_HOOK(FragmentChunkFastWrite,ll::memory::HookPriority::Normal,LevelChunk,&LevelChunk::setBlockSimple,void,ChunkBlockPos const&pos,Block const&block){if(nativeFragmentOwner&&this!=nativeFragmentOwner)return;origin(pos,block);}
bool seededNeighbourRead(BlockSource const*s,BlockPos const&p){return s==seededReadSource&&seededReadClip&&worldgen_rules::scatteredSeededRead(sharedBox(*seededReadClip),{p.x,p.y,p.z});}
Block const&seededNeighbourBlock(BlockPos const&p){auto const&palette=currentStructurePalette();auto query=[&](auto q){auto s=field->surface(q.x+32768,q.z+32768);return palette.at(field->material(s,q.x+32768,q.y,q.z+32768));};return *(scatteredReadCache?scatteredReadCache->get({p.x,p.y,p.z},query):query(worldgen_rules::StructurePoint{p.x,p.y,p.z}));}
LL_TYPE_INSTANCE_HOOK(SeededStructureRead,ll::memory::HookPriority::Normal,BlockSource,(Block const&(BlockSource::*)(BlockPos const&)const)&BlockSource::$getBlock,Block const&,BlockPos const&p){if(seededNeighbourRead(this,p))return seededNeighbourBlock(p);return origin(p);}
LL_TYPE_INSTANCE_HOOK(SeededStructureLayer,ll::memory::HookPriority::Normal,BlockSource,(Block const&(BlockSource::*)(BlockPos const&,uint)const)&BlockSource::$getBlock,Block const&,BlockPos const&p,uint layer){if(seededNeighbourRead(this,p)){if(!layer)return seededNeighbourBlock(p);return *currentStructurePalette().at(0);}return origin(p,layer);}
LL_TYPE_INSTANCE_HOOK(SeededStructureSolid,ll::memory::HookPriority::Normal,BlockSource,(bool(BlockSource::*)(BlockPos const&)const)&BlockSource::$isSolidBlockingBlock,bool,BlockPos const&p){if(seededNeighbourRead(this,p))return seededNeighbourBlock(p).isSolidBlockingBlock();return origin(p);}
class MineNeighbourSource final:public BlockSource {
 BlockSource const&source;BoundingBox const&clip;std::vector<Block const*>const&palette;
public:
 MineNeighbourSource(BlockSource&s,BoundingBox const&b):BlockSource(s.getChunkSource(),false,true),source(s),clip(b),palette(currentStructurePalette()){}
 Block const&getBlock(BlockPos const&p)const override{if(p.x>=clip.min.x&&p.x<=clip.max.x&&p.z>=clip.min.z&&p.z<=clip.max.z)return source.getBlock(p);auto query=[&](auto q){auto s=field->surface(q.x+32768,q.z+32768);return palette.at(field->material(s,q.x+32768,q.y,q.z+32768));};return *(scatteredReadCache?scatteredReadCache->get({p.x,p.y,p.z},query):query(worldgen_rules::StructurePoint{p.x,p.y,p.z}));}
 Block const&getBlock(BlockPos const&p,uint layer)const override{if(!layer)return getBlock(p);if(p.x<clip.min.x||p.x>clip.max.x||p.z<clip.min.z||p.z>clip.max.z)return *palette[0];return source.getExtraBlock(p);}
 bool isSolidBlockingBlock(BlockPos const&p)const override{return getBlock(p).isSolidBlockingBlock();}
 bool isSolidBlockingBlock(int x,int y,int z)const override{return isSolidBlockingBlock({x,y,z});}
};
LL_TYPE_INSTANCE_HOOK(MineSupport,ll::memory::HookPriority::Normal,MineshaftPiece,&MineshaftPiece::_isSupportingBox,bool,int x0,int x1,BlockSource&region,int y1,int z){if(!mineshaftQueryClip)return origin(x0,x1,region,y1,z);MineNeighbourSource view(region,*mineshaftQueryClip);return origin(x0,x1,view,y1,z);}
LL_TYPE_INSTANCE_HOOK(MineWeb,ll::memory::HookPriority::Normal,MineshaftCorridor,&MineshaftCorridor::_placeCobWeb,void,BlockSource&region,BoundingBox const&clip,Random&random,float chance,int x,int y,int z){if(!mineshaftQueryClip){origin(region,clip,random,chance,x,y,z);return;}MineNeighbourSource view(region,*mineshaftQueryClip);origin(view,clip,random,chance,x,y,z);}
thread_local std::set<std::array<int,3>>* structureWallWrites=nullptr;
void rememberStructureWall(BlockPos const&p,Block const&block){if(structureWallWrites&&block.getTypeName().ends_with("_wall"))structureWallWrites->insert({p.x,p.y,p.z});}
bool outsidePortalFragment(BlockSource* source,BlockPos const&p){
 if(source!=portalWriteSource||!portalWriteClip)return false;
 return !worldgen_rules::scatteredFragmentWriteOwned(sharedBox(*portalWriteClip),{p.x,p.y,p.z});
}
LL_TYPE_INSTANCE_HOOK(RuinGround,ll::memory::HookPriority::Normal,OceanRuinPieces::OceanRuinPiece,&OceanRuinPieces::OceanRuinPiece::_getHeight,int,BlockPos& pos,BlockSource& region,BlockPos const& corner){
 if(ruinFloor>-32768)return ruinFloor;return origin(pos,region,corner);
}
LL_STATIC_HOOK(ShipGround,ll::memory::HookPriority::Normal,&ShipwreckPiece::_calculateTargetPos,BlockPos,BlockSource& region,BlockPos pos,Rotation rot,BlockPos size){
 auto result=origin(region,pos,rot,size);if(shipFloor>-32768)result.y=shipFloor;return result;
}
LL_STATIC_HOOK(ShipGroundLegacy,ll::memory::HookPriority::Normal,&ShipwreckPiece::_calculateTargetPosLegacy,BlockPos,BlockSource& region,BlockPos pos,Rotation rot,BlockPos size){
 auto result=origin(region,pos,rot,size);if(shipFloor>-32768)result.y=shipFloor;return result;
}
LL_TYPE_INSTANCE_HOOK(PortalGround,ll::memory::HookPriority::Normal,RuinedPortalPiece,&RuinedPortalPiece::_moveStructureBoundsToSuitableY,void,Random& random,BlockSource& region,PortalEnvironmentModifiers const& modifiers){
 if(portalFloor>-32768){int dy=portalFloor-mStructureBounds->min.y;mStructureBounds->min.y+=dy;mStructureBounds->max.y+=dy;return;}origin(random,region,modifiers);
}
