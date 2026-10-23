#pragma once
#include <array>
#include <string_view>

// Gameplay rules originate in the LLBDS adapter. Windows and Android import
// this policy; platform hooks may translate objects, never fork these decisions.
namespace worldgen_rules {
inline constexpr std::string_view Revision="llbds-structures-v3";
// Both maintained adapters execute this policy, including older seed recipes.
inline constexpr std::string_view StructurePlanRevision="llbds-structures-v3";
inline constexpr int VillageSpacing=32,VillageSeparation=8,VillageSalt=10387312;
inline constexpr int VillageSampleRadius=32,VillageSampleStep=32,VillageMaxSlope=10;
inline constexpr int VillageReach=128,VillagePieceStep=3,VillagePieceMaxOffset=8;
inline constexpr int VillageTreeMargin=12,VillageApronRadius=4,VillageApronMaxShift=1;
inline constexpr int RoadRelaxationPasses=16;
inline constexpr bool MansionHabitatAvailable=false; // No dark-forest biome yet.
inline int villageStyle(int biome){
 switch(biome){case 5:return 0;case 6:return 1;case 7:case 8:return 2;case 11:return 3;case 12:return 4;default:return -1;}
}
template<class S> bool villageSurfaceAllowed(S const&s,int style){
 return style>=0&&villageStyle(s.biome)==style&&s.water<s.height&&!s.lake&&!s.river&&!s.volcanic&&!s.stone&&s.iceHabitat<.01;
}
template<class S> bool scatteredCenterAllowed(S const&s){
 return (s.biome==11||s.biome==13||s.biome==10||s.biome==5)&&s.water<s.height&&!s.lake&&!s.river&&!s.volcanic&&!s.stone&&s.iceHabitat<=.01;
}
template<class S> bool scatteredFootprintAllowed(S const&s,int biome){
 return s.biome==biome&&s.water<s.height&&!s.lake&&!s.river&&!s.volcanic&&!s.stone;
}
inline int scatteredMaxSlope(int biome){return biome==11?6:3;}
struct ExtraSpec {int type,spacing,separation,salt,radius;};
inline constexpr std::array<ExtraSpec,12> ExtraSpecs{{
 {9,32,8,146731021,10},{6,128,32,146731022,16},{0,32,8,146731023,5},
 {3,32,8,146731024,5},{1,40,12,146731025,4},{2,32,8,146731026,4},
 {4,32,8,146731027,1},{5,64,24,146731028,6},{7,80,24,146731029,10},
 {8,64,20,146731030,16},{10,34,8,146731031,8},{11,34,12,146731032,12}}};
inline constexpr std::array<char const*,12> ExtraNames{{"pillager_outpost","ruined_portal","shipwreck","ocean_ruin","buried_treasure","monument","stronghold","mansion","ancient_city","mineshaft","trail_ruins","trial_chambers"}};
inline bool extraSurface(int t){return t==0||t==1||t==7||t==10;}
inline bool extraMarine(int t){return t==2||t==3||t==5;}
inline bool extraUnderground(int t){return t==6||t==8||t==9||t==11;}
inline ExtraSpec const* extraSpec(int type){for(auto const&s:ExtraSpecs)if(s.type==type)return &s;return nullptr;}
// Capture/test surfaces predating the configurable sea retain their Y=31
// datum; live surfaces carry the immutable recipe's actual water plane.
template<class S> int surfaceSeaLevel(S const&s){if constexpr(requires{s.seaLevel;})return s.seaLevel;else return 31;}
template<class S> bool extraAnchorAllowed(int type,S const&s,bool mansionHabitatAvailable){
 int sea=surfaceSeaLevel(s);
 if(!extraSpec(type)||s.volcanic>.01||s.iceHabitat>.01)return false;
 if(extraMarine(type)){if(s.ocean<(type==5?.9:.7)||s.height>sea-(type==5?27:7)||s.height< -40||s.lake||s.river)return false;}
 else if(type==4){if(s.height<sea-6||s.height>sea+11||s.lake||s.river||(s.biome!=3&&s.biome!=4&&s.biome!=20&&s.coast<.01))return false;}
 else if(s.ocean>.1||s.height<40)return false;
 if(type==7&&(!mansionHabitatAvailable||s.biome!=9))return false;
 if(type==10&&s.biome!=6&&s.biome!=9&&s.biome!=13)return false;
 if(type==8&&s.height<90)return false;
 if(type==0&&(villageStyle(s.biome)<0||s.water>=s.height||s.river||s.lake))return false;
 return true;
}
template<class S> bool extraFootprintAllowed(int type,S const&s){
 return !(type==7&&s.biome!=9)&&s.volcanic<=.01&&s.iceHabitat<=.01&&!s.lake&&!s.river&&
  (extraMarine(type)?s.ocean>=.5:type==4||s.water<s.height);
}
inline int extraMaxSlope(int type){return type==5?8:type==4?4:6;}
// The two public mods share this SAME inclusion profile. LLBDS remains the
// upstream for every rule; only finite bounds and server test tools differ.
struct ProductProfile {bool infiniteBounds,testTools;};
inline constexpr ProductProfile LlbdsProfile{false,true},PublicModProfile{true,false};
inline std::string_view scatteredName(int biome){return biome==11?"desert_pyramid":biome==13?"jungle_pyramid":biome==10?"swamp_hut":"igloo";}
inline bool publicStructureIncluded(std::string_view name){
 if(name.starts_with("minecraft:"))name.remove_prefix(10);
 if(name=="village"||name=="desert_pyramid"||name=="jungle_pyramid"||name=="swamp_hut"||name=="igloo")return true;
 for(auto n:ExtraNames)if(n==name)return true;return false;
}
}
