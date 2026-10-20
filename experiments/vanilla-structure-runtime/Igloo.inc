// Freeze the vanilla template layout before chunk placement. Igloo::postProcess
// reselects rotation/depth and reads the live heightmap on every call, so it is
// unsuitable for independent chunk writes. Geometry and transforms stay native.
struct IglooPart { LegacyStructureTemplate* model; BlockPos pos; BoundingBox bounds; worldgen_rules::IglooTemplateRole role; };
struct IglooLayout {
 Rotation rotation=Rotation::None; uint seed=0; int ladders=0;
 BoundingBox bounds; std::vector<IglooPart> parts;
 std::shared_ptr<worldgen_rules::FrozenIglooProgram const> program;
};
BoundingBox templateBounds(LegacyStructureTemplate const& model,BlockPos pos,Rotation rotation){
 auto s=model.rawSize();BoundingBox b{{INT_MAX,INT_MAX,INT_MAX},{INT_MIN,INT_MIN,INT_MIN}};
 for(int z:{0,s.z-1})for(int y:{0,s.y-1})for(int x:{0,s.x-1}){
  auto q=LegacyStructureTemplate::transform({x,y,z},Mirror::None,rotation);q.x+=pos.x;q.y+=pos.y;q.z+=pos.z;
  b.min.x=std::min(b.min.x,q.x);b.min.y=std::min(b.min.y,q.y);b.min.z=std::min(b.min.z,q.z);
  b.max.x=std::max(b.max.x,q.x);b.max.y=std::max(b.max.y,q.y);b.max.z=std::max(b.max.z,q.z);
 }return b;
}
IglooLayout planIgloo(Dimension& dim,ChunkPos cp,uint worldSeed,int floor){
 IglooLayout result;Random rng(worldSeed,true);StructureFeature::setRandomSeedFor(rng,cp.x,cp.z,14357617,worldSeed);
 auto manager=dim.getLevel().getStructureManager();
 auto model=[&](worldgen_rules::IglooTemplateRole role)->LegacyStructureTemplate&{using R=worldgen_rules::IglooTemplateRole;auto const&native=role==R::TopWithTrapdoor?Igloo::STRUCTURE_LOCATION_IGLOO_TRAPDOOR():role==R::TopWithoutTrapdoor?Igloo::STRUCTURE_LOCATION_IGLOO_NO_TRAPDOOR():role==R::Ladder?Igloo::STRUCTURE_LOCATION_LADDER():Igloo::STRUCTURE_LOCATION_LABORATORY();auto expected=worldgen_rules::iglooTemplateName(role);if(worldgen_rules::sharedStructurePlan(input.at("config").value("sharedRules",J::object()).value("revision",std::string{}))&&native!=expected)throw std::runtime_error("Igloo native template name differs from shared catalog");return manager->getOrCreateLegacy(native);};
 auto size=[&](auto role){auto s=model(role).rawSize();return worldgen_rules::StructurePoint{s.x,s.y,s.z};};
 auto plan=[&]{if(worldgen_rules::sharedStructurePlan(input.at("config").value("sharedRules",J::object()).value("revision",std::string{}))){worldgen_rules::StructureRandom shared(worldgen_rules::placementSeed(cp.x,cp.z,14357617,worldSeed));return worldgen_rules::planFrozenIgloo({cp.x,cp.z},floor,shared,size);}return worldgen_rules::planFrozenIgloo({cp.x,cp.z},floor,rng,size);}();
 result.rotation=Rotation(plan.rotation);result.ladders=plan.ladders;result.seed=plan.seed;
 for(auto const&p:plan.parts)result.parts.push_back({&model(p.role),{p.origin.x,p.origin.y,p.origin.z},{{p.bounds.min.x,p.bounds.min.y,p.bounds.min.z},{p.bounds.max.x,p.bounds.max.y,p.bounds.max.z}},p.role});
 result.bounds={{plan.bounds.min.x,plan.bounds.min.y,plan.bounds.min.z},{plan.bounds.max.x,plan.bounds.max.y,plan.bounds.max.z}};
 return result;
}
worldgen_rules::FrozenIglooPlan sharedIglooGeometry(IglooLayout const&layout){
 worldgen_rules::FrozenIglooPlan geometry;geometry.rotation=int(layout.rotation);geometry.ladders=layout.ladders;geometry.seed=layout.seed;geometry.bounds={{layout.bounds.min.x,layout.bounds.min.y,layout.bounds.min.z},{layout.bounds.max.x,layout.bounds.max.y,layout.bounds.max.z}};
 for(auto const&p:layout.parts)geometry.parts.push_back({p.role,{p.pos.x,p.pos.y,p.pos.z},{{p.bounds.min.x,p.bounds.min.y,p.bounds.min.z},{p.bounds.max.x,p.bounds.max.y,p.bounds.max.z}}});return geometry;
}
std::vector<worldgen_rules::LegacyDataMarker> nativeLegacyDataMarkers(LegacyStructureTemplate const&model){
 std::vector<worldgen_rules::LegacyDataMarker>out;for(auto const&c:*model.mBlockInfo){if(!c.mTag||c.mBlock.getTypeName()!="minecraft:structure_block")continue;std::string_view mode,metadata;if(c.mTag->contains("mode",Tag::String))mode=c.mTag->at("mode").get<StringTag>();if(c.mTag->contains("metadata",Tag::String))metadata=c.mTag->at("metadata").get<StringTag>();if(auto m=worldgen_rules::legacyDataMarker(c.mBlock.getTypeName(),mode,metadata))out.push_back({{c.mPos->x,c.mPos->y,c.mPos->z},std::string(*m)});}return out;
}
void compileIglooProgram(IglooLayout&layout){
 auto geometry=sharedIglooGeometry(layout);
 std::map<worldgen_rules::IglooTemplateRole,std::vector<worldgen_rules::LegacyDataMarker>>markers;
 for(auto const&p:layout.parts){if(markers.contains(p.role))continue;
  markers[p.role]=nativeLegacyDataMarkers(*p.model);
 }
 layout.program=std::make_shared<worldgen_rules::FrozenIglooProgram>(worldgen_rules::compileFrozenIgloo(std::move(geometry),[&](auto role)->auto const&{return markers.at(role);}));
}
bool placeIgloo(BlockSource& blocks,IglooLayout const& plan,BoundingBox const& clip){
 auto air=Block::tryGetFromRegistry("minecraft:air");if(!air)throw std::runtime_error("Missing air");
 if(plan.program)return worldgen_rules::executeFrozenIgloo(*plan.program,{{clip.min.x,clip.min.y,clip.min.z},{clip.max.x,clip.max.y,clip.max.z}},[&](size_t i,auto const&,int rotation,uint32_t seed,uint32_t randomSeed,auto const&){auto const&p=plan.parts.at(i);LegacyStructureSettings settings(Mirror::None,Rotation(rotation),nullptr,clip);settings.mIgnoreStructureBlocks=true;settings.mSeed=seed;Random rng(randomSeed,true);p.model->placeInWorld(blocks,p.pos,settings,rng);},[&](worldgen_rules::StructurePoint p){blocks.setBlockNoUpdate({p.x,p.y,p.z},*air);},[&](worldgen_rules::StructureLootAssignment const&a){BlockPos p{a.position.x,a.position.y,a.position.z};if(blocks.getBlock(p).getTypeName()!="minecraft:chest")throw std::runtime_error("Igloo chest marker has no chest");auto actor=blocks.getBlockEntity(p);if(!actor)throw std::runtime_error("Igloo chest actor absent");static_cast<ChestBlockActor*>(actor)->setLootTable(a.table,int(a.seed));});
 for(size_t i=0;i<plan.parts.size();++i){auto const&p=plan.parts[i];auto const& b=p.bounds;
  if(b.max.x<clip.min.x||b.min.x>clip.max.x||b.max.z<clip.min.z||b.min.z>clip.max.z)continue;
  LegacyStructureSettings settings(Mirror::None,plan.rotation,nullptr,clip);settings.mIgnoreStructureBlocks=true;settings.mSeed=plan.seed;
  Random rng(plan.seed+uint(i)*0x9e3779b9u,true);p.model->placeInWorld(blocks,p.pos,settings,rng);
  // Vanilla marker handling, restricted to the owning chunk. Mobs are deferred
  // until their once-only post-load lifecycle is wired; don't spawn per chunk.
  for(auto const&[pos,label]:p.model->getMarkers(p.pos,settings)){
   if(pos.x<clip.min.x||pos.x>clip.max.x||pos.z<clip.min.z||pos.z>clip.max.z)continue;
   blocks.setBlockNoUpdate(pos,*air);
   if(auto assignment=worldgen_rules::iglooMarkerLoot(label,{pos.x,pos.y,pos.z},plan.seed)){BlockPos chestPos{assignment->position.x,assignment->position.y,assignment->position.z};if(blocks.getBlock(chestPos).getTypeName()!="minecraft:chest")throw std::runtime_error("Igloo chest marker has no chest");
    auto actor=blocks.getBlockEntity(chestPos);if(!actor)throw std::runtime_error("Igloo chest actor absent");
    static_cast<ChestBlockActor*>(actor)->setLootTable(assignment->table,int(assignment->seed));
   }
  }
 }return true;
}
