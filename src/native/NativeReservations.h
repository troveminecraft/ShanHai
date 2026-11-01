#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <utility>

namespace nativegen {
class NativeReservations {
 std::vector<std::array<int,4>> areas;
 std::unordered_map<uint64_t,std::vector<size_t>> bins;
 bool indexed=false;
 static int cell(int value){return int(std::floor(value/64.));}
 static uint64_t key(int x,int z){return (uint64_t(uint32_t(x))<<32)|uint32_t(z);}
public:
 explicit NativeReservations(std::vector<std::array<int,4>> value={}):areas(std::move(value)){
  if(areas.size()<=8)return;size_t entries=0;
  for(auto const&r:areas){if(r[0]>r[2]||r[1]>r[3])continue;
   uint64_t count=uint64_t(int64_t(cell(r[2]))-cell(r[0])+1)*(int64_t(cell(r[3]))-cell(r[1])+1);
   if(count>65536||entries+count>65536)return;entries+=size_t(count);
  }
  for(size_t i=0;i<areas.size();++i){auto const&r=areas[i];if(r[0]>r[2]||r[1]>r[3])continue;
   for(int z=cell(r[1]);z<=cell(r[3]);++z)for(int x=cell(r[0]);x<=cell(r[2]);++x)bins[key(x,z)].push_back(i);
  }indexed=true;
 }
 bool contains(int x,int z)const{
  auto hit=[&](auto const&r){return x>=r[0]&&x<=r[2]&&z>=r[1]&&z<=r[3];};
  if(!indexed){for(auto const&r:areas)if(hit(r))return true;return false;}
  auto i=bins.find(key(cell(x),cell(z)));if(i==bins.end())return false;
  for(auto id:i->second)if(hit(areas[id]))return true;return false;
 }
};
}
