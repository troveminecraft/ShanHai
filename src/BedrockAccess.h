#pragma once
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/Level.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/LevelSeed64.h"
#include "mc/world/level/storage/LevelDataValue.h"
#include "mc/world/level/chunk/SubChunk.h"
#include "BedrockSeed.h"
#include <stdexcept>
#include <limits>

namespace bedrock_compat {
// These SDK fields exist on both pinned ABIs. Read the native state; never
// synthesize completion to work around a missing accessor in newer headers.
inline auto const& loadState(LevelChunk const& chunk) { return *chunk.mLoadState; }
inline void markUnsaved(LevelChunk& chunk){
#ifdef SHANHAI_MC_26_51
    // Preserve the native setUnsaved contract: all six data counters, every
    // allocated subchunk and actor data become dirty; reset explicit save hints.
    auto dirty=[](DirtyTicksCounter& counter){
        counter.totalTime=counter.lastChange=std::numeric_limits<int>::max();
    };
    for(unsigned i=0;i<6;++i)dirty(chunk.mFullChunkDirtyTicksCounters[i]);
    for(auto& section:*chunk.mSubChunks)dirty(*section.mDirtyTicksCounter);
    dirty(*chunk.mChunkActorsDirtyTicksCounter);
    chunk.mSaveNonActorDataIfDirty=false;chunk.mSaveIfNeverSaved=false;
#else
    chunk.setUnsaved();
#endif
}
inline void requestSave(LevelChunk& chunk){
    markUnsaved(chunk);
    chunk.mSaveIfNeverSaved=true;chunk.mSaveNonActorDataIfDirty=true;
}
inline Block const& extraBlock(LevelChunk const& chunk,ChunkBlockPos const& pos,Block const& air){
#ifdef SHANHAI_MC_26_51
    // Native BlockSource::getExtraBlock uses this same optional layer and XZY index.
    auto section=unsigned(pos.y.mVal)>>4;
    if(section>=chunk.mSubChunks->size())return air;
    auto* storage=chunk.mSubChunks->at(section).mBlocksReadPtr[1];
    return storage?storage->getElement(uint16_t((pos.x<<8)|(pos.z<<4)|(pos.y.mVal&15))):air;
#else
    return chunk.getExtraBlock(pos);
#endif
}
inline unsigned viewRadius(Player const& player) { return player.mChunkRadius; }
inline auto& buildPolicy(Dimension& dimension) { return *dimension.mChunkBuildOrderPolicy; }
template<class T> decltype(auto) level(T& value) {
 // Adapter callers are native Level-backed chunks, dimensions and sources.
 if constexpr(requires { value.getLevel(); }) {
   if constexpr(std::is_pointer_v<decltype(value.getLevel())>)return value.getLevel();
   else return static_cast<Level&>(value.getLevel());
 }
 else return static_cast<Level&>(value.mLevel);
}
template<class T> decltype(auto) dimension(T& value) {
 if constexpr(requires { value.getDimension(); }) return value.getDimension();
 else return (value.mDimension);
}
template<class T> decltype(auto) position(T const& value) {
 if constexpr(requires { value.getPosition(); }) return value.getPosition();
 else return *value.mPosition;
}
template<class T> decltype(auto) generator(T& value) {
 if constexpr(requires { value.getWorldGenerator(); }) return value.getWorldGenerator();
 else return (value.mWorldGenerator);
}
template<class T> decltype(auto) heightRange(T const& value) {
 if constexpr(requires { value.getHeightRange(); }) return value.getHeightRange();
 else if constexpr(requires { *value.mHeightRange; }) return *value.mHeightRange;
 else return *value.mDimension.mHeightRange;
}
template<class T> decltype(auto) levelName(T const& value) {
 if constexpr(requires { value.getLevelName(); }) return value.getLevelName();
 else return *value.mLevelName;
}
template<class T> decltype(auto) weather(T& value) {
 if constexpr(requires { value.getWeather(); }) return value.getWeather();
 else return *value.mWeather;
}
}
