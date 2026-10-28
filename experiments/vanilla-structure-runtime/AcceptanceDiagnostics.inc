// Explicit developer readbacks; never called by ordinary chunk generation.
void sampleAcceptance(Dimension&d,BlockPos pos){
 BoundingBox box{{pos.x-2,std::max(-63,pos.y-4),pos.z-2},{pos.x+2,std::min((field->generationHeight()-66),pos.y+4),pos.z+2}};ensureLoaded(d,box);
 auto&blocks=d.getBlockSourceFromMainChunkSource();J cells=J::array();
 for(int z=box.min.z;z<=box.max.z;++z)for(int x=box.min.x;x<=box.max.x;++x)for(int y=box.min.y;y<=box.max.y;++y){BlockPos q{x,y,z};auto const&b=blocks.getBlock(q);cells.push_back({{"pos",{x,y,z}},{"name",b.getTypeName()},{"survives",b.canSurvive(blocks,q)},{"state",J::parse(b.mSerializationId->toSnbt(SnbtFormat::Jsonify),nullptr,false,true)}});}
 auto s=field->surface(pos.x+32768,pos.z+32768);
 std::ofstream(folder/"acceptance-sample.json")<<J{{"world",activeWorld},{"pos",{pos.x,pos.y,pos.z}},{"surface",s.height},{"biome",s.biome},{"water",s.water},{"cells",cells}}.dump(2);
}
void inspectVillageTemplates(VillagePlan const&plan){
 J rows=J::array();for(auto const&ptr:*plan.start->pieces){auto&p=*static_cast<PoolElementStructurePiece*>(ptr.get());auto const&opt=*p.mElement.mTemplate;if(!opt)continue;auto const&variant=*opt->mStructureVariant;auto legacy=std::get_if<std::reference_wrapper<LegacyStructureTemplate>>(&variant);if(!legacy)continue;J cells=J::array();
  for(auto const&i:*legacy->get().mBlockInfo){auto q=bedrock_compat::transform(*i.mPos,Mirror::None,p.mRotation);cells.push_back({q.x+p.mPosition->x,q.y+p.mPosition->y,q.z+p.mPosition->z,i.mBlock.getTypeName()});}
  rows.push_back({{"name",*p.mElement.mLocation},{"projection",int(p.mElement.getProjection())},{"base",p.mPosition->y},{"box",boxJson(*p.mBoundingBox)},{"cells",cells}});
 }
 std::ofstream(folder/"village-template-cells.json")<<rows.dump();
}

// Manual actual-block readback for cavern/structure integration. No writes,
// neighbour generation, or scans during normal gameplay.
void inspectCityEnvironment(Dimension&d,ExtraPlan const&plan){
 auto const&start=*plan.start;auto b=*start.boundingBox;auto core=b;b.min.x-=40;b.min.z-=40;b.max.x+=40;b.max.z+=40;b.min.y=-58;b.max.y=std::min(110,b.max.y+48);
 int64_t volume=int64_t(b.max.x-b.min.x+1)*(b.max.z-b.min.z+1)*(b.max.y-b.min.y+1);if(volume>20000000)throw std::runtime_error("City environment readback too large");ensureLoaded(d,b);auto&blocks=d.getBlockSourceFromMainChunkSource();
 std::map<std::string,uint16_t> ids;J palette=J::array(),counts=J::object(),unsupported=J::array(),links=J::array();std::ofstream raw(folder/"city-environment.bin",std::ios::binary);
 for(int z=b.min.z;z<=b.max.z;++z)for(int x=b.min.x;x<=b.max.x;++x)for(int y=b.min.y;y<=b.max.y;++y){BlockPos pos{x,y,z};auto const&block=blocks.getBlock(pos);auto const&n=block.getTypeName();auto it=ids.find(n);if(it==ids.end()){uint16_t id=uint16_t(ids.size());it=ids.emplace(n,id).first;palette.push_back(n);}auto id=it->second;raw.write(reinterpret_cast<char*>(&id),2);counts[n]=counts.value(n,0)+1;
  if((n=="minecraft:red_mushroom"||n=="minecraft:brown_mushroom"||n=="minecraft:moss_carpet"||n=="minecraft:sculk_vein"||n=="minecraft:spore_blossom")&&!block.canSurvive(blocks,pos)&&unsupported.size()<200)unsupported.push_back({{"pos",{x,y,z}},{"name",n}});
 }
 for(auto const&l:cityConnections(start,structureSeed(d),cityNeighbours(d,start,structureSeed(d))))links.push_back({{"a",{l.a.x,l.a.y,l.a.z}},{"b",{l.b.x,l.b.y,l.b.z}}});
 std::ofstream(folder/"city-environment.json")<<J{{"world",activeWorld},{"anchor",plan.diagnostics["anchor"]},{"sampleBox",boxJson(b)},{"structureBox",boxJson(core)},{"palette",palette},{"counts",counts},{"invalidAttachments",unsupported},{"links",links}}.dump(2);
}
