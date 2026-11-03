#undef NDEBUG
#include "BiomeCatalog.h"
#include <cassert>
#include <set>
#include <iostream>
int main(){
 using namespace terrain_runtime::biome_catalog;
 assert(chinese("zh_CN")&&chinese("zh_TW")&&chinese("zh-Hans")&&chinese("ZH-hant"));
 assert(!chinese("en_US")&&!chinese("ja_JP")&&!chinese("")&&!chinese("zhuang"));
 std::set<std::string> ids;
 for(auto const&e:entries()){
  assert(ids.insert(e.id()).second&&e.id().starts_with("peakarma:"));
  assert(!e.title(false).empty()&&!e.title(true).empty());
  assert(!e.description(false).empty()&&!e.description(true).empty());
 }
 assert(find("minecraft:plains")==-1&&find("peakarma:plains")>=0);
 auto get=[](char const* id)->Entry const& {int i=find(id);assert(i>=0);return entries()[i];};
 assert(available(get("peakarma:roofed_forest"),62,true));
 assert(!available(get("peakarma:dappled_forest"),62,true));
 assert(available(get("peakarma:dappled_forest"),63,true));
 assert(!available(get("peakarma:mangrove_swamp"),58,true));
 assert(!available(get("peakarma:snowy_forest"),29,false));
 assert(!available(get("peakarma:arid_basin"),63,true));
 nativegen::Surface s;s.biome=9;s.snowCover=.9;
 assert(matches(get("peakarma:snowy_forest"),s,true));
 assert(!matches(get("peakarma:forest"),s,true));
 assert(matches(get("peakarma:forest"),s,false));
 assert(!matches(get("peakarma:snowy_desert"),s,true));
 s.biome=11;
 assert(matches(get("peakarma:snowy_desert"),s,true));
 s.superGlacier=.8;s.glacier=.8;
 assert(matches(get("peakarma:super_glacier"),s,true));
 assert(!matches(get("peakarma:glacier"),s,true));
 s.superGlacier=0;
 assert(matches(get("peakarma:glacier"),s,true));
 s.glacier=s.superGlacier=.35;
 assert(matches(get("peakarma:glacier"),s,true));
 assert(!matches(get("peakarma:super_glacier"),s,true));
 s.closedBasin=true;s.basinWeight=.9;s.height=80;
 assert(matches(get("peakarma:inland_basin"),s,true));
 s.height=150;assert(matches(get("peakarma:inland_basin"),s,true));
 s.basinWeight=0;assert(!matches(get("peakarma:inland_basin"),s,true));
 s.volcanic=.7;assert(matches(get("peakarma:volcano"),s,true));
 std::cout<<ids.size()<<" unique namespaced biomes/landforms; locale, host capabilities, snow and landform matching passed\n";
}
