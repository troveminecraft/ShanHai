#ifdef SHANHAI_MC_26_51
static_assert(sizeof(br::worldgen::WorldGenRandom)==56);
#endif
// Adapt modern native structure instances to the same saved-chunk lifecycle.
bool modernValid(br::worldgen::StructureInstance const& instance) {
#ifdef SHANHAI_MC_26_51
 return !instance.mSections->empty();
#else
 return instance.isValid();
#endif
}
class ModernStart final:public StructureStart {
public:
 std::shared_ptr<br::worldgen::StructureInstance> instance;
 std::shared_ptr<br::worldgen::Structure const> definition;
 std::string name;
 ModernStart(std::shared_ptr<br::worldgen::StructureInstance> i,std::shared_ptr<br::worldgen::Structure const> d,std::string n,ChunkPos cp):instance(std::move(i)),definition(std::move(d)),name(std::move(n)){*boundingBox=*instance->mBox;chunkX=cp.x;chunkZ=cp.z;maxYSpawnOffset=0;}
 bool isValid()const override{return instance&&modernValid(*instance);}
 std::string_view getStructureName()const override{return name;}
 bool postProcess(BlockSource& b,Random& r,BoundingBox const& clip)override{return instance->postProcess(b,r,clip);}
};
LL_TYPE_INSTANCE_HOOK(ModernGround,ll::memory::HookPriority::Normal,br::worldgen::ChunkAccessor,&br::worldgen::ChunkAccessor::getFirstFreeHeight,int,int x,int z,br::worldgen::HeightmapProjection::Type type){
 if(adaptHeightMap)return field->surface(x+32768,z+32768).height+1;
 return origin(x,z,type);
}
std::unique_ptr<StructureStart> createModern(Dimension& d,int type,ChunkPos cp){
 auto& pools=bedrock_compat::level(d).getJigsawStructureRegistry();auto name=type==10?"minecraft:trail_ruins":"minecraft:trial_chambers";
 auto def=pools.mStructureRegistry->get(name);if(!def)def=pools.mStructureRegistry->get(type==10?"trail_ruins":"trial_chambers");if(!def)throw std::runtime_error("Modern native structure definition missing");
 auto s=field->surface(cp.x*16+32768,cp.z*16+32768);auto biome=d.getBiomeRegistry().lookupByName(type==10?"minecraft:jungle":"minecraft:plains");if(!biome)throw std::runtime_error("Modern biome missing");FixedBiomeSource bs(*biome);
 br::worldgen::ChunkAccessor access(d,bs);
 // SDK omits the native context constructor. Own the documented member types
 // and check the complete layout rather than inventing raw field offsets.
 struct ContextStorage {JigsawStructureRegistry const& pools;decltype(br::worldgen::GenerationContext::mRandom) random;int64_t seed;ChunkPos cp;br::worldgen::ChunkAccessor& accessor;};
 static_assert(sizeof(ContextStorage)==sizeof(br::worldgen::GenerationContext));
 static_assert(offsetof(ContextStorage,accessor)==offsetof(br::worldgen::GenerationContext,mChunkAccessor));
 #ifdef SHANHAI_MC_26_51
 // Value initialization constructs the SDK variant's SimpleRandom alternative.
 // Native setLargeFeatureSeed supplies its seed before the first random read.
 ContextStorage storage{pools,{},int64_t(structureSeed(d)),cp,access};
#else
 ContextStorage storage{pools,br::worldgen::WorldGenRandom::create(structureSeed(d)),int64_t(structureSeed(d)),cp,access};
#endif
 storage.random->setLargeFeatureSeed(storage.seed,cp.x,cp.z);
 auto instance=def->generate(reinterpret_cast<br::worldgen::GenerationContext&>(storage));if(!instance||!modernValid(*instance))return {};
 return std::make_unique<ModernStart>(std::move(instance),std::move(def),name,cp);
}
