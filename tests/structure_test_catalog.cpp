#include "../src/StructureTestCatalog.h"
#include <cassert>
#include <set>
int main(){
 using namespace structure_test;
 assert(expected()==45);
 std::set<int> ids;
 for(int g=0;g<int(Groups.size());++g){
  assert(group(Groups[g].type,Groups[g].style)==g);
  for(int i=0;i<Groups[g].quota;++i)assert(ids.insert(firstId(g)+i).second);
 }
 assert(ids.size()==45&&*ids.begin()==1&&*ids.rbegin()==45);
 assert(group(7,-1)==-1); // Mansion remains disabled.
 assert(group(12,-1)==-1&&group(13,7)==-1);
 auto all=cells(-40,39);assert(all.size()==6400);
 std::set<std::pair<int,int>> positions;int64_t distance=-1;
 for(auto c:all){assert(c.x>=-40&&c.x<=39&&c.z>=-40&&c.z<=39);assert(positions.emplace(c.x,c.z).second);auto d=int64_t(c.x)*c.x+int64_t(c.z)*c.z;assert(d>=distance);distance=d;}
 auto larger=expandingCells(32,40000);assert(larger.size()>all.size());
 for(size_t i=0;i<all.size();++i)assert(larger[i].x==all[i].x&&larger[i].z==all[i].z);
 positions.clear();for(auto c:larger)assert(positions.emplace(c.x,c.z).second);
}
