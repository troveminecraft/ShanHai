// Forest identities follow existing climate and landforms. This pass changes
// ecology only: it never reshapes a coast, water surface, mountain or river.
void forestHabitat(Surface& s,int x,int z){
 if(!forest_capabilities::enabled(forestProfile))return;
 if(s.mushroomZone||s.badlands||s.badlandsDesert||s.redSandCoast||s.desert>.05||s.aridType
    ||s.volcanic>.01||s.barren>.01||s.stone>.01||s.giant>.01||s.glacier>.18||s.iceHabitat>=.60
    ||s.scarp>.05||s.cliffLow>-32768||s.fluvialType||s.riftWeight>.15)return;
 double t=ecologyTemperature(s),h=s.H;
 // Rooted warm coastal wetlands use the existing connected sea/estuary plane.
 // No new water is introduced and inland/highland basins cannot qualify.
 if(t>=.62&&h>=.54&&!s.closedBasin&&!s.lake&&s.river==0&&s.mainRiver<.20
    &&s.height>=seaLevelValue-4&&s.height<=seaLevelValue+6
    &&s.water<=seaLevelValue&&s.lava<s.height&&s.coast<.20){
  int gx=int(std::floor(x/128.)),gz=int(std::floor(z/128.));
  double u=smooth(0,1,x/128.-gx),v=smooth(0,1,z/128.-gz);
  auto shore=[&](int X,int Z){auto n=oceanNode(X,Z);return .5*n.broad+n.mouth;};
  double connected=(1-u)*(1-v)*shore(gx,gz)+u*(1-v)*shore(gx+1,gz)
      +(1-u)*v*shore(gx,gz+1)+u*v*shore(gx+1,gz+1);
  if(connected>.16&&biomeNoise.at(x/370.+71.,z/370.+139.)>-.12){
   s.biome=forest_capabilities::Mangrove;s.treeD=.012;s.soilSand=0;return;
  }
 }
 if(s.kind||s.lake||s.river||s.water>=s.height||s.height<seaLevelValue+8||s.height>340)return;
 bool forest=s.biome==9||s.biome==23;
 bool meadow=forest||s.biome==7||s.biome==8||s.biome==14;
 if(meadow&&t>=.38&&t<=.67&&h>=.38&&h<=.76&&s.soilSand<.15
    &&s.height>=seaLevelValue+35&&s.height<=310
    &&(s.upland>.10||s.mount>.05||s.height>=165)
    &&biomeNoise.at(x/610.+217.,z/610.-81.)>.10){
  s.biome=forest_capabilities::Cherry;s.treeD=.009;return;
 }
 bool temperate=forest&&t>=.36&&t<=.70&&h>=.53&&s.soilSand<=.20;
 double stand=temperate?biomeNoise.at(x/510.,z/510.):-1.;
 // Sheltered humid benches/valleys carry rarer pale stands inside the same
 // broad temperate forest region. Coherent noise avoids scattered biome dots.
 bool pale=temperate&&stand>.19&&s.height<295&&s.ridge<.60&&h>.58
     &&biomeNoise.at(x/235.+97.,z/235.-143.)>.13;
 if(pale){s.biome=forest_capabilities::Pale;s.treeD=.012;return;}
 // A new optional forest must not consume the rarer common habitats above.
 if(forest_capabilities::dappled(forestProfile)&&(forest||s.biome==6)
    &&t>=.30&&t<=.62&&h>=.42&&s.soilSand<.15&&s.height<=315
    &&biomeNoise.at(x/490.-157.,z/490.+231.)>.03){
  s.biome=forest_capabilities::Dappled;s.treeD=.012;return;
 }
 if(temperate&&stand>=-.04){s.biome=forest_capabilities::Dark;s.treeD=.016;}
}
