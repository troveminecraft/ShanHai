// Cave policy and groundwater boundary are kept together. Surface hydrology
// calls caveWaterColumn once; block selection and cave geometry stay separate.
 bool continuousCaves=false,boundedAquifers=false;
 bool connectedCaves=false;
 bool finiteCaveSystems=false;
 bool groupedCaverns=false,deepCaveLava=false;
 bool caveDesign=terrainRevision(data)>=186;
 CavernGroups cavernGroups{terrainSeed,terrainRevision(data)>=176||C.at("cave").value("irregularMegaHalls",false),caveDesign,terrainRevision(data)>=189,terrainRevision(data)>=193};
 std::unordered_map<uint64_t,int> cavernHeights;
 double cavernHeight(int x,int z){auto i=cavernHeights.find(key2(x,z));if(i!=cavernHeights.end())return i->second;
  // Remote hall controls need the terrain envelope, not every distant region's
  // full drainage plan. Local roof checks still use the actual carved surface.
  return unboundedRegions?regionalClimateSample(x+32768,z+32768).height:base(x+32768,z+32768).height;
 }
 CaveNetwork caveNetwork{terrainSeed};
 FiniteCaves finiteCaves{terrainSeed};
 static J underground(J const&v){if(!v.is_object()&&!v.is_array())return nullptr;if(v.is_object()&&v.value("type","")=="minecraft:range_choice"&&v.value("input",J())=="minecraft:overworld/sloped_cheese")return v.at("when_out_of_range");for(auto const&a:v){auto r=underground(a);if(!r.is_null())return r;}return nullptr;}
 void loadCaves(){
 caveRoof=num(C.at("cave"),"roof");caveFade=num(C.at("cave"),"fade");caveScale=num(C.at("epic"),"uniformScale");waterline=num(C,"undergroundWaterY");
 continuousCaves=C.at("cave").value("continuousSampling",0)==1;
 boundedAquifers=C.at("cave").value("boundedAquifers",0)==1;
 finiteCaveSystems=C.at("cave").value("algorithm","")=="finite-karst-v2";
 groupedCaverns=C.at("cave").value("algorithm","")=="bounded-cavern-groups-v3";
 deepCaveLava=C.at("cave").value("deepLava",0)==1;
 if(data.contains("cavernPlan")){auto const&p=data.at("cavernPlan");if(p.at("seed")!=terrainSeed||p.at("algorithm")!="cavern-height-controls-v1")throw std::runtime_error("Cavern height plan mismatch");
  cavernHeights.reserve(p.at("samples").size());for(auto const&r:p.at("samples"))cavernHeights.emplace(key2(r.at(0).get<int>(),r.at(1).get<int>()),r.at(2).get<int>());
 }
 finiteCaves.enableHalls(C.at("cave").value("largeHalls",0)==1);
 connectedCaves=groupedCaverns||finiteCaveSystems||C.at("cave").value("algorithm","")=="connected-passages-v1";
 if(!connectedCaves){auto branch=underground(data.at("functions").at("minecraft:overworld"));if(branch.is_null())throw std::runtime_error("No underground density");cave=caves.compile(J{{"type","minecraft:interpolated"},{"argument",J{{"type","minecraft:min"},{"argument1",branch},{"argument2","minecraft:overworld/caves/noodle"}}}});}
 }
 bool isCave(int x,int y,int z,int height){
  int depth=height-y;double roof=caveRoof,end=caveFade;
  if(y<-60||depth<=roof)return false;
  if(groupedCaverns)return depth>roof+4&&cavernGroups.contains(x-32768,y,z-32768,[&](int X,int Z){return cavernHeight(X,Z);});
  if(finiteCaveSystems)return depth>roof+4&&finiteCaves.contains(x-32768,y,z-32768,[&](int X,int Z){return regionalHeight(X+32768,Z+32768,vcont(X+32768,Z+32768));});
  if(connectedCaves)return depth>roof+4&&caveNetwork.contains(x-32768,y,z-32768,[&](int X,int Z){
   // A coarse height estimate chooses passage altitude. Do not evaluate all
   // surface ecology/hydrology around a remote chunk just to build a tunnel.
   // The actual column height/roof remains the final authoritative limiter.
   return regionalHeight(X+32768,Z+32768,vcont(X+32768,Z+32768));
  });
  double X=(x-32768)/caveScale,Y=-64+(y+64)/caveScale,Z=(z-32768)/caveScale;
  // The density graph already interpolates cached corners. Rounding here
  // added alternating one/two-unit jumps under the 0.75 scale transform.
  if(!continuousCaves){X=std::floor(X);Y=std::floor(Y);Z=std::floor(Z);}
  double d=cave(X,Y,Z);if(depth>=end)return d<0;
  double w=smooth(roof,end,depth);return(1-w)+w*d<0;
 }
Perlin aquiferNoise{seedHash(terrainSeed,911)};
struct AquiferBed {double wet;int bed;};
Cache<uint64_t,AquiferBed> aquiferBeds{32768};
AquiferBed aquiferBed(int x,int z){return aquiferBeds.get(key2(x,z),[&]{
 double wet=aquiferNoise.at((x-32768)/192.,(z-32768)/192.);
 // A bounded bowl inside the existing cave. Its rim meets the water datum
 // gradually; its base is supported above bedrock, not an infinite wet column.
 int top=deepCaveLava?-43:int(waterline)-1;
 int bed=int(std::floor(top+3-13*smooth(.22,.50,wet)));
 return AquiferBed{wet,std::clamp(bed,deepCaveLava?-49:-59,top+1)};
});}
void caveWaterColumn(Surface&s,int x,int z){
 auto a=aquiferBed(x,z);s.aquifer=a.wet>.28?2:a.wet>.20?1:0;
 s.cavePoolBed=a.bed;int top=deepCaveLava?-43:int(waterline)-1,low=a.bed;
 // Only the small wet neighbourhood needs the two-block rim. This is a
 // world-coordinate cache, so chunk order cannot change the basin boundary.
 if(a.wet>.16)for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){
  if((dx==0&&dz==0)||std::abs(dx)+std::abs(dz)>2)continue;
  low=std::min(low,aquiferBed(x+dx,z+dz).bed);
 }
 s.caveSealFloor=low<top?low-2:top+1;
}
bool caveLavaFloor(int x,int z){if(!caveDesign||!groupedCaverns)return false;auto const&bits=cavernGroups.column(x-32768,z-32768,[&](int X,int Z){return cavernHeight(X,Z);});return cavernGroups.thermalColumn()||(((bits[0]>>12)&1)&&((bits[0]>>16)&1));}
int caveWaterMaterial(Surface const&s,int y,bool openLavaFloor=false)const{
 int top=deepCaveLava?-43:int(waterline)-1;
 // A deep lava datum fills only carved space, never bedrock or all columns.
 // Water bowls have their own higher datum and at least two rock blocks below.
 if(deepCaveLava&&y<=-52)return 43;
 if(openLavaFloor)return 0;
 if(y>top)return 0;
 if(y>s.cavePoolBed)return 8;
 // The floor and neighbouring rim isolate water from lower/adjacent dry
 // caves. Above the water datum remains open: no horizontal stone lid.
 return y>=s.caveSealFloor?2:0;
}
