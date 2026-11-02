#undef NDEBUG
#include "BiomeSearch.h"
#include <cassert>
#include <set>
#include <iostream>
int main(){
 using namespace terrain_runtime;
 assert(biomeSearchCanUseHints({biome_catalog::find("peakarma:forest")},true));
 assert(!biomeSearchCanUseHints({biome_catalog::find("peakarma:snowy_forest")},true));
 assert(!biomeSearchCanUseHints({biome_catalog::find("peakarma:river")},true));
 assert(!biomeSearchCanUseHints({biome_catalog::find("peakarma:super_glacier")},true));
 std::set<std::pair<int,int>> seen;int calls=0;
 auto sample=[&](int x,int z){++calls;assert(seen.emplace(x,z).second);terrain_kernel::Surface s;
  s.biome=(x==-193&&z==-1)?61:9;s.height=72;s.water=s.biome==61?76:-32768;return s;};
 auto valid=[](int x,int z){return x>=-256&&x<=256&&z>=-256&&z<=256;};
 auto result=searchBiomes(-1,-1,std::vector{61},sample,valid,[]{return false;},{256,64,200});
 assert(result.found&&(*result.found==std::array{-193,77,-1,61}));
 assert(!result.interrupted&&result.samples==calls);
 int cancelledCalls=0;
 auto stopped=searchBiomes(0,0,std::vector{61},[&](int,int){++cancelledCalls;return terrain_kernel::Surface{};},valid,[]{return true;});
 assert(stopped.interrupted&&!stopped.found&&cancelledCalls==0);
 auto budget=searchBiomes(0,0,std::vector{61},[](int,int){return terrain_kernel::Surface{};},valid,[]{return false;},{256,64,10});
 assert(budget.interrupted&&budget.samples==10&&!budget.found);
 auto unavailable=searchBiomes(0,0,std::vector<int>{},sample,valid,[]{return false;});
 assert(!unavailable.found&&unavailable.samples==0);
 seen.clear();calls=0;
 auto narrow=searchBiomesAdaptive(-1,-1,std::vector{61},sample,valid,[]{return false;},{256,64,200});
 assert(narrow.found&&(*narrow.found==std::array{-193,77,-1,61}));
 std::set<std::pair<int,int>> adaptiveSeen;
 auto remote=[&](int x,int z){assert(adaptiveSeen.emplace(x,z).second);terrain_kernel::Surface s;
  s.biome=(x>=2000&&x<=3000&&z>=1500&&z<=2600)?61:9;s.height=120;return s;};
 auto accelerated=searchBiomesAdaptive(0,0,std::vector{61},remote,[](int,int){return true;},[]{return false;});
 assert(accelerated.found&&accelerated.samples<800);
 auto distantNarrow=searchBiomesAdaptive(0,0,std::vector{61},[](int x,int z){terrain_kernel::Surface s;
  s.biome=x==-832&&z==576?61:9;return s;},[](int,int){return true;},[]{return false;},{1024,64,2000});
 assert(distantNarrow.found&&(*distantNarrow.found)[0]==-832&&(*distantNarrow.found)[2]==576);
 auto limited=searchBiomesAdaptive(0,0,std::vector{61},[](int,int){return terrain_kernel::Surface{};},valid,[]{return false;},{256,64,10});
 assert(limited.interrupted&&limited.samples==10);
 std::cout<<"biome search: exact recipe samples, water Y, negative bounds, cancellation and budgets passed\n";
}
