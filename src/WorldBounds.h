#pragma once
namespace terrain_bounds {
inline constexpr int Chunks=2500,HalfChunks=Chunks/2,MinBlock=-HalfChunks*16,MaxBlock=HalfChunks*16-1;
inline constexpr bool containsChunk(int x,int z){return x>=-HalfChunks&&x<HalfChunks&&z>=-HalfChunks&&z<HalfChunks;}
// "Infinite" means the game's coordinate envelope, with no custom test square.
inline constexpr int GameChunkLimit=30000000/16;
inline constexpr bool gameContainsChunk(int x,int z){return x>=-GameChunkLimit&&x<GameChunkLimit&&z>=-GameChunkLimit&&z<GameChunkLimit;}
inline constexpr unsigned long long coordinateKey(int x,int z){return (static_cast<unsigned long long>(static_cast<unsigned int>(x))<<32)|static_cast<unsigned int>(z);}
}
