// Forest identities follow existing climate and landforms. This pass changes
// ecology only: it never reshapes a coast, water surface, mountain or river.
void forestHabitat(Surface& s,int x,int z){
 if(!forest_capabilities::enabled(forestProfile))return;
 if(s.mushroomZone||s.badlands||s.badlandsDesert||s.redSandCoast||s.desert>.05||s.aridType
    ||s.volcanic>.01||s.barren>.01||s.stone>.01||s.giant>.01||s.glacier>.18||s.iceHabitat>=.60
    ||s.scarp>.05||s.cliffLow>-32768||s.fluvialType||s.riftWeight>.15)return;
 if(s.kind||s.lake||s.river||s.water>=s.height||s.height<seaLevelValue+8||s.height>340)return;
 double t=ecologyTemperature(s),h=s.H;
 bool forest=s.biome==9||s.biome==23;
 bool meadow=forest||s.biome==7||s.biome==8||s.biome==14;
 if(meadow&&t>=.38&&t<=.67&&h>=.38&&h<=.76&&s.soilSand<.15
    &&s.height>=seaLevelValue+35&&s.height<=310
    &&(s.upland>.10||s.mount>.05||s.height>=165)
    &&biomeNoise.at(x/610.+217.,z/610.-81.)>.10){
  s.biome=forest_capabilities::Cherry;s.treeD=.009;return;
 }
 if(!forest||t<.36||t>.70||h<.53||s.soilSand>.20)return;
 double stand=biomeNoise.at(x/510.,z/510.);
 if(stand<-.04)return;
 // Sheltered humid benches/valleys carry rarer pale stands inside the same
 // broad temperate forest region. Coherent noise avoids scattered biome dots.
 bool pale=stand>.19&&s.height<295&&s.ridge<.60&&h>.58
     &&biomeNoise.at(x/235.+97.,z/235.-143.)>.13;
 s.biome=pale?forest_capabilities::Pale:forest_capabilities::Dark;
 s.treeD=pale?.012:.016;
}
