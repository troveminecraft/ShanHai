#pragma once
#include "BedrockAccess.h"
#include "BedrockStreaming.h"
#include "rules/StructureRandom.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/BlockChangeContext.h"
#include "mc/world/level/chunk/LevelChunkTag.h"
#include "mc/world/level/levelgen/structure/StructurePiece.h"
#include "mc/world/level/levelgen/structure/StructureStart.h"
#include "mc/world/level/levelgen/structure/RandomScatteredLargeFeature.h"
#include "mc/world/level/levelgen/structure/random_scattered_large_feature_details/ScatteredFeatureStart.h"
#include "mc/world/level/levelgen/structure/LegacyStructureTemplate.h"
#include "mc/world/level/biome/source/BiomeSource.h"
#include "mc/world/level/biome/GetBiomeOptions.h"
#include "mc/world/level/levelgen/v1/IPreliminarySurfaceProvider.h"
#include "mc/util/Random.h"
#include "mc/world/level/material/Material.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockVolume.h"
#include "mc/world/level/levelgen/feature/gamerefs_feature/GameRefsFeature.h"
#include "mc/world/level/levelgen/feature/registry/FeatureRegistry.h"
#include "mc/world/level/levelgen/feature/IFeature.h"
#include "mc/world/level/chunk/LevelChunkBlockActorAccessToken.h"
#include "mc/world/level/block/components/BlockLiquidDetectionComponent.h"
#ifdef SHANHAI_MC_26_51
#include "NativeGeneration26_51.h"
#include "NativePortalPatch26_51.h"
#include "mc/deps/core/threading/TaskResult.h"
#include "mc/util/random/MarsagliaPolarGaussian.h"
#include "mc/util/MolangVariableMap.h"
#include "mc/world/level/IBlockWorldGenAPI.h"
// The native inlined constructor starts with an empty Gaussian cache.
inline MarsagliaPolarGaussian::MarsagliaPolarGaussian():mUnkc48506{},mUnk7bf66f{}{}
static_assert(sizeof(MarsagliaPolarGaussian)==16);
inline MolangVariableMap::MolangVariableMap()
 :mMapFromVariableIndexToVariableArrayOffset{},mVariables{},mHasPublicVariables(false){}
// The base interface's folded default slots are not exported by 26.51.9.
// Owned WorldBlockTarget subclasses override them; base construction still
// requires their definitions for its temporary vtable.
inline bool IBlockWorldGenAPI::canGetChunk()const{return false;}
inline LevelChunk* IBlockWorldGenAPI::getChunk(ChunkPos const&){return nullptr;}
#endif

namespace bedrock_compat {
inline bool canContainLiquid(Block const& block){
#ifdef SHANHAI_MC_26_51
    return block.mDirectData->mWaterDetectionRule->mCanContainLiquid;
#else
    return BlockLiquidDetectionComponent::canContainLiquid(block);
#endif
}
inline bool setExtraBlockSimple(BlockSource& source,BlockPos const& pos,Block const& block){
#ifdef SHANHAI_MC_26_51
    // Preserve the native simple-write path: no ticks, neighbour notifications
    // or block-source callbacks, and no writes to missing/read-only chunks.
    if(!block.getBlockType().mCanBeExtraBlock||pos.y<source.mMinHeight||pos.y>=source.mMaxHeight)return false;
    auto* chunk=source.getChunkAt(pos);
    if(!chunk||chunk->mReadOnly)return false;
    chunk->setExtraBlockSimple(ChunkBlockPos(pos,source.mMinHeight),block);
    return true;
#else
    return source.setExtraBlockSimple(pos,block);
#endif
}
inline LevelChunkBlockActorAccessToken blockActorAccess(LevelChunk& chunk){
#ifdef SHANHAI_MC_26_51
    // The pinned native release has no per-thread access gate; the old release
    // also returned an empty token. Keep this separate from chunk write locks.
    static_assert(std::is_empty_v<LevelChunkBlockActorAccessToken>);
    return {};
#else
    return chunk.enableBlockEntityAccessForThisThread();
#endif
}
inline std::vector<LevelChunkBlockActorAccessToken> blockActorAccess(ChunkViewSource& view){
#ifdef SHANHAI_MC_26_51
    return {};
#else
    return view.enableBlockEntityAccess();
#endif
}
template<class Fn> void forEachFeature(FeatureRegistry& registry,Fn&& fn){
#ifdef SHANHAI_MC_26_51
    for(auto const& [name,index]:*registry.mFeatureLookupMap)
        fn(name,registry.lookupByName(name.getString()));
#else
    registry.forEachFeature(std::forward<Fn>(fn));
#endif
}
inline bool featureNameMatches(FeatureRegistry const& registry,std::string const& name,uint64_t index,IFeature const* feature){
#ifdef SHANHAI_MC_26_51
    auto const& map=*registry.mFeatureLookupMap;
    auto found=map.find(HashedString(name));
    return found!=map.end()&&found->second==index&&index<registry.mFeatureRegistry->size()
        &&registry.mFeatureRegistry->at(index).get()==feature;
#else
    return registry.reverseLookupString(*feature)==name;
#endif
}
class Volume:public BlockVolume {
public:
    Volume(buffer_span_mut<Block const*> blocks,int width,int height,int depth,Block const& air,int bottom)
#ifdef SHANHAI_MC_26_51
        :BlockVolume{} {
        mBlocks=blocks;mWidth=width;mHeight=height;mDepth=depth;
        mDimensionBottom=bottom;mInitBlock=&air;
    }
#else
        :BlockVolume(blocks,width,height,depth,air,bottom){}
#endif
};
inline size_t volumeIndex(BlockVolume const& volume,BlockPos const& p){
#ifdef SHANHAI_MC_26_51
    return (p.x*volume.mDepth+p.z)*volume.mHeight+p.y;
#else
    return volume.indexNoBoundsCheck(p);
#endif
}
inline void setBiomes(LevelChunk& chunk,ChunkVolume::VolumeOf<Biome> const& volume){
#ifdef SHANHAI_MC_26_51
    chunk.mBiomes->setBiomeFromVolume(volume,volume.mNumSubChunks);
#else
    chunk.setBiomeFromVolume(volume);
#endif
}
class BlockSourceView:public BlockSource {
public:
    BlockSourceView(ChunkSource& source,bool publicSource,bool allowUnpopulated)
#ifdef SHANHAI_MC_26_51
        :BlockSource(*source.mLevel,*source.mDimension,source,publicSource,allowUnpopulated,false,true){}
#else
        :BlockSource(source,publicSource,allowUnpopulated){}
#endif
};
// Mirrors the inlined 26.51 BlockSource solid-block test.
inline bool solidBlock(Block const& block){
#ifdef SHANHAI_MC_26_51
    auto const& material=block.getMaterial();
    return block.hasProperty(BlockProperty::CubeShaped)&&material.mBlocksMotion&&!material.mNeverBuildable;
#else
    return block.isSolidBlockingBlock();
#endif
}
inline void calculateBoundingBox(StructureStart& start){
#ifdef SHANHAI_MC_26_51
    constexpr int limit=std::numeric_limits<int>::max();
    BoundingBox box{{limit,limit,limit},{-limit,-limit,-limit}};
    for(auto const& piece:*start.pieces){
        auto const& b=*piece->mBoundingBox;
        box.min.x=std::min(box.min.x,b.min.x);box.min.y=std::min(box.min.y,b.min.y);box.min.z=std::min(box.min.z,b.min.z);
        box.max.x=std::max(box.max.x,b.max.x);box.max.y=std::max(box.max.y,b.max.y);box.max.z=std::max(box.max.z,b.max.z);
    }
    start.boundingBox=box;
#else
    start.calculateBoundingBox();
#endif
}
inline void moveBoundingBoxes(StructureStart& start,int dy){
#ifdef SHANHAI_MC_26_51
    start.boundingBox->min.y+=dy;start.boundingBox->max.y+=dy;
    for(auto& piece:*start.pieces)piece->moveBoundingBox(0,dy,0);
#else
    start.moveBoundingBoxes(dy);
#endif
}
inline BlockPos transform(BlockPos p,Mirror mirror,Rotation rotation){
#ifdef SHANHAI_MC_26_51
    if(int(mirror)==1)p.z=-p.z;else if(int(mirror)==2)p.x=-p.x;
    auto x=p.x,z=p.z;
    switch(int(rotation)){
        case 1:p.x=-z;p.z=x;break;
        case 2:p.x=-x;p.z=-z;break;
        case 3:p.x=z;p.z=-x;break;
    }
    return p;
#else
    return LegacyStructureTemplate::transform(p,mirror,rotation);
#endif
}
inline auto featureBiome(BiomeSource const& biomes,ChunkPos const& cp,IPreliminarySurfaceProvider const& surface){
#ifdef SHANHAI_MC_26_51
    int y=surface.getPreliminarySurfaceLevel({cp.x*4,cp.z*4}).value_or(1);
    BlockPos origin{cp.x*16,y,cp.z*16};
    // Native selection queries the centre, but returns the chunk origin.
    auto biome=biomes._getBiome(GetBiomeOptions{{origin.x+8,y,origin.z+8},true});
    return std::pair<BlockPos const,Biome const*>{origin,biome};
#else
    return StructureFeature::getBiomeForFeatureGeneration(biomes,cp,surface);
#endif
}
inline void seedStructure(Random& random,int x,int z,int salt,uint32_t seed){
#ifdef SHANHAI_MC_26_51
    // All adapter callers own deterministic local RNGs. Native reconstruction
    // resets both MT state and cached Gaussian values without using reset(),
    // which reseeds from the operating system in 26.51.
    if(!random.mOwnedRandom->has_value())throw std::logic_error("Structure RNG must own its state");
    std::destroy_at(&random);
    std::construct_at(&random,worldgen_rules::placementSeed(x,z,salt,seed),true);
#else
    StructureFeature::setRandomSeedFor(random,x,z,salt,seed);
#endif
}
inline bool setBlockNoUpdate(BlockSource& source,BlockPos const& pos,Block const& block){
#ifdef SHANHAI_MC_26_51
    return source.setBlock(pos,block,4,nullptr,BlockChangeContext{});
#else
    return source.setBlockNoUpdate(pos,block);
#endif
}
inline std::unique_ptr<RandomScatteredLargeFeatureDetails::ScatteredFeatureStart> scatteredStart(
    Dimension& dimension,BiomeSource const& biomes,short sea,int x,int z,IPreliminarySurfaceProvider const& surface){
    using Start=RandomScatteredLargeFeatureDetails::ScatteredFeatureStart;
#ifdef SHANHAI_MC_26_51
    RandomScatteredLargeFeature feature(0);Random unused(0,true);
    auto native=feature.createStructureStart(dimension,biomes,unused,{x,z},surface);
    auto start=std::unique_ptr<Start>(static_cast<Start*>(native.release()));
    // The inlined constructor reads the dimension's sea level only to place
    // initial piece boxes at sea+1. Keep the terrain adapter's explicit sea
    // level by moving those boxes before any placement/ground averaging.
    if(start&&!start->pieces->empty())moveBoundingBoxes(*start,int(sea)-int(dimension.mSeaLevel));
    return start;
#else
    return std::make_unique<Start>(biomes,sea,x,z,surface);
#endif
}
}
