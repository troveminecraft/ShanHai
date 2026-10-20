// Filter the caller-owned terrain buffer before its first chunk write. This also
// covers disk/memory terrain cache hits; never edit loaded/saved player chunks.
std::set<std::pair<int,int>> prefilteredChunks;
unsigned prefillLogRows=0;
unsigned filterVegetationVolume(BlockVolume const& volume,ChunkPos cp,VegetationPair& pair){
 auto before=pair.normal->vegetationChunk(cp.x,cp.z),after=pair.reserved->vegetationChunk(cp.x,cp.z);auto air=Block::tryGetFromRegistry("minecraft:air");unsigned count=0;auto data=volume.mBlocks->mBegin;
 for(int z=0;z<16;++z)for(int x=0;x<16;++x)for(int y=4;y<384;++y){int i=(z*16+x)*384+y;if(!worldgen_rules::structureReservationCandidate(before[i],after[i]))continue;auto idx=volume.indexNoBoundsCheck(BlockPos{x,y,z});if(!worldgen_rules::structureReservationRemoves(before[i],after[i],data[idx]->getTypeName()))continue;data[idx]=air;++count;
  if(y<383){auto above=volume.indexNoBoundsCheck(BlockPos{x,y+1,z});if(data[above]->getTypeName()=="minecraft:snow_layer"){data[above]=air;++count;}}
 }return count;
}
LL_TYPE_INSTANCE_HOOK(StructurePrefill,ll::memory::HookPriority::Normal,LevelChunk,&LevelChunk::setBlockVolume,void,BlockVolume const& volume,uint yOffset){
 auto cp=getPosition();auto& dim=getDimension();
 if(!automaticStructures||volume.mWidth!=16||volume.mDepth!=16||volume.mHeight!=384||volume.mDimensionBottom!=-64||yOffset!=0||getState().load()>ChunkState::Generated||static_cast<int>(dim.getDimensionId())!=0||dim.getLevel().getLevelData().getLevelName()!=activeWorld||!structureChunkInside(cp.x,cp.z)){origin(volume,yOffset);return;}
 // Vanilla void initialization precedes our terrain fill; do not plan for it.
 if(volume.mBlocks->mBegin[volume.indexNoBoundsCheck(BlockPos{0,0,0})]->getTypeName()!="minecraft:bedrock"){origin(volume,yOffset);return;}
 uint seed=dim.getLevel().getLevelData().getSeed().to32BitRandomSeed();std::vector<ChunkPos> vc,sc;
 for(int z=structureCell(cp.z-10);z<=structureCell(cp.z+10);++z)for(int x=structureCell(cp.x-10);x<=structureCell(cp.x+10);++x){auto c=villageCandidate(x,z,seed);if(std::abs(c.x-cp.x)<=10&&std::abs(c.z-cp.z)<=10)vc.push_back(c);}
 for(int z=structureCell(cp.z-3);z<=structureCell(cp.z+2);++z)for(int x=structureCell(cp.x-3);x<=structureCell(cp.x+2);++x){auto c=nativeCandidateForCell(x,z,seed);if(c.x<=cp.x+2&&c.x+3>=cp.x&&c.z<=cp.z+2&&c.z+3>=cp.z)sc.push_back(c);}
 auto ec=nearbyExtra(cp,seed);std::erase_if(ec,[](auto const&e){return !extraSurface(e.first);});
 if(vc.empty()&&sc.empty()&&ec.empty()){origin(volume,yOffset);return;}
 try {std::lock_guard guard(stageMutex);if(!storedChunk(dim,cp)){
 auto began=std::chrono::steady_clock::now();unsigned cleared=0,plans=0;
 auto intersects=[&](BoundingBox b){return !(b.max.x+28<cp.x*16||b.min.x-28>cp.x*16+15||b.max.z+28<cp.z*16||b.min.z-28>cp.z*16+15);};
 for(auto c:vc){auto&p=villagePlan(dim,c,seed);if(p.start&&intersects(*p.start->boundingBox)){cleared+=filterVegetationVolume(volume,cp,villageVegetationPair(p));++plans;}}
 for(auto c:sc){auto&p=autoBlueprint(dim,c,seed);if(p.start&&p.start->getStructureName()!="minecraft:desert_pyramid"&&intersects(*p.start->boundingBox)){cleared+=filterVegetationVolume(volume,cp,structureVegetationPair(p));++plans;}}
 for(auto [type,c]:ec){auto&p=extraPlan(dim,type,c);if(p.start&&intersects(*p.start->boundingBox)){cleared+=filterVegetationVolume(volume,cp,extraVegetationPair(p));++plans;}}
 if(plans){prefilteredChunks.insert({cp.x,cp.z});if(prefillLogRows++<2048)std::ofstream(folder/"structure-prefill.jsonl",std::ios::app)<<J{{"chunk",{cp.x,cp.z}},{"state",int(getState().load())},{"cleared",cleared},{"plans",plans},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()}}.dump()<<'\n';}
 }}catch(std::exception const&e){automaticStructures=false;++stageErrors;std::ofstream(folder/"automatic-errors.log",std::ios::app)<<"prefill: "<<e.what()<<'\n';}
 origin(volume,yOffset);
}
