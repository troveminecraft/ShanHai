bool arrangeVillage(StructureStart&start,VillagePlan&result,ChunkPos cp,uint seed,int biome,
                    LegacyJigsawPlacement::PieceFactory const&factory){
 if(!factory)throw std::runtime_error("Native village piece factory unavailable");
 worldgen_rules::VillageBlueprint source{{cp.x,cp.z},seed,biome,{}};
 // Packing only needs immutable template bounds, never a copy of all blocks.
 for(auto const&ptr:*start.pieces){auto const&p=*static_cast<PoolElementStructurePiece*>(ptr.get());
  worldgen_rules::StructureBlueprintPiece b;b.box=sharedBox(*p.mBoundingBox);b.origin={p.mPosition->x,p.mPosition->y,p.mPosition->z};
  b.rotation=int(p.mRotation);b.location=*p.mElement.mLocation;b.rigid=p.mElement.getProjection()==Projection::Rigid;
  b.kind=p.mElement.type()==StructurePoolElementType::Single?worldgen_rules::StructurePieceKind::Template:worldgen_rules::StructurePieceKind::Other;
  source.pieces.push_back(std::move(b));
 }
 auto sample=[](int x,int z){return field->surface(x+32768,z+32768);};
 auto plan=input.at("config").value("orderedVillages",0)>=2
    ?worldgen_rules::clusterVillage(source,sample):worldgen_rules::orderVillage(source,sample);
 result.diagnostics["ordered"]={{"baselineBuildings",plan.baseline},{"targetBuildings",plan.target},
  {"accepted",plan.accepted},{"reason",plan.rejection},{"surfaceQueries",plan.surfaceQueries}};
 if(!plan.accepted)return false;
 std::vector<std::unique_ptr<StructurePiece>> pieces;pieces.reserve(plan.lots.size());
 // Expand housing, not the initial AI population. The first copy of each
 // original template retains its native residents and animal markers.
 result.populationPieces.clear();result.populationPieces.reserve(plan.lots.size());
 for(auto const&lot:plan.lots){auto const&p=*static_cast<PoolElementStructurePiece*>((*start.pieces)[lot.prototype].get());
  auto junction=*p.mJigsawJunction;
  // Pass the native reference unchanged, exactly as the parent builder does;
  // moveBoundingBox updates native caches and junction coordinates together.
  auto clone=factory(p.mElement,*p.mPosition,p.mRotation,p.mGenDepth,junction,*p.mBoundingBox,*p.mRefPos);
  if(!clone)throw std::runtime_error("Native village piece factory returned null");
  clone->moveBoundingBox(lot.delta.x,lot.delta.y,lot.delta.z);pieces.push_back(std::move(clone));
  result.populationPieces.push_back(lot.populate);
 }
 *start.pieces=std::move(pieces);bedrock_compat::calculateBoundingBox(start);result.plannedRoads=std::move(plan.roads);
 result.diagnostics["ordered"]["actualBuildings"]=plan.target;return true;
}
