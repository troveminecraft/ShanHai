#pragma once
#include <array>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cstdint>

// BDS test UI policy, independent of generation probabilities and recipes.
namespace structure_test {
struct Group { std::string_view key, name; int type, style, quota; };
inline constexpr std::array<Group,20> Groups{{
 {"snowy_village","雪原村庄",12,0,3}, {"taiga_village","针叶林村庄",12,1,3},
 {"plains_village","平原村庄",12,2,3}, {"desert_village","沙漠村庄",12,3,3},
 {"savanna_village","热带草原村庄",12,4,3},
 {"desert_pyramid","沙漠神殿",13,11,2}, {"jungle_pyramid","丛林神庙",13,13,2},
 {"swamp_hut","沼泽小屋",13,10,2}, {"igloo","冰屋",13,5,2},
 {"pillager_outpost","掠夺者前哨站",0,-1,2}, {"ruined_portal","废弃传送门",1,-1,2},
 {"shipwreck","沉船",2,-1,2}, {"ocean_ruin","海底废墟",3,-1,2},
 {"buried_treasure","埋藏的宝藏",4,-1,2}, {"monument","海底神殿",5,-1,2},
 {"stronghold","要塞",6,-1,2}, {"ancient_city","远古城市",8,-1,2},
 {"mineshaft","废弃矿井",9,-1,2}, {"trail_ruins","古迹废墟",10,-1,2},
 {"trial_chambers","试炼密室",11,-1,2}
}};
inline int group(int type,int style){
 for(size_t i=0;i<Groups.size();++i)if(Groups[i].type==type&&(type<12||Groups[i].style==style))return int(i);
 return -1;
}
inline int firstId(int group){int id=1;for(int i=0;i<group;++i)id+=Groups[i].quota;return id;}
inline int expected(){return firstId(int(Groups.size()))-1;}
struct Cell {int x,z;};
inline std::vector<Cell> cells(int lo,int hi){
 std::vector<Cell> result;
 for(int z=lo;z<=hi;++z)for(int x=lo;x<=hi;++x)result.push_back({x,z});
 std::sort(result.begin(),result.end(),[](Cell a,Cell b){
  auto da=int64_t(a.x)*a.x+int64_t(a.z)*a.z,db=int64_t(b.x)*b.x+int64_t(b.z)*b.z;
  return da!=db?da<db:a.z!=b.z?a.z<b.z:a.x<b.x;
 });return result;
}
inline std::vector<Cell> expandingCells(int spacing,int radius){
 auto cell=[&](int block){int cp=block>=0?block/16:(block-15)/16;return cp>=0?cp/spacing:(cp-spacing+1)/spacing;};
 std::vector<Cell> result;int previousLo=1,previousHi=0;
 for(int reach=20000;reach<=radius;reach+=20000){
  int lo=cell(-reach),hi=cell(reach-1);
  for(auto c:cells(lo,hi))if(c.x<previousLo||c.x>previousHi||c.z<previousLo||c.z>previousHi)result.push_back(c);
  previousLo=lo;previousHi=hi;
 }return result;
}
}
