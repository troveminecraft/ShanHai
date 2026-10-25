#undef NDEBUG
#include "native/Field.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("new recipe/report required");J data;std::ifstream(argv[1])>>data;Field f(data);assert(f.seaLevel()==64);
 J old=data;old["config"].erase("landscapeRevision");Field priorSnow(old);
 int water=0,snow=0;for(int z=-4096;z<=4096;z+=512)for(int x=-4096;x<=4096;x+=512){int X=x+32768,Z=z+32768;auto s=f.surface(X,Z);
  if(s.height<64&&s.water==64&&!s.lake&&!s.river&&!s.stone&&!s.volcanic){int m=f.material(s,X,64,Z);assert(m==8||m==9);assert(f.material(s,X,65,Z)==0);++water;}
  Surface mountain;mountain.height=340;mountain.T=.05;double a=priorSnow.surfaceTemperature(mountain,X,Z),b=f.surfaceTemperature(mountain,X,Z);assert(b>=a);snow+=b>a;
 }
 assert(water>10&&snow>10);int routes=0,segments=0;
 for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x){auto p=f.inspectRegionalDrainage(x,z);auto const&ss=p.at("segments");
  for(auto const&s:ss){assert(double(s[4])>=double(s[5])&&double(s[5])>=64);++segments;}
  for(auto const&r:p.at("routes")){size_t first=r.at("firstSegment"),count=r.at("segmentCount");assert(count>0);++routes;
   for(size_t i=first+1;i<first+count;++i){auto const&a=ss[i-1];auto const&b=ss[i];assert(a[2]==b[0]&&a[3]==b[1]&&a[5]==b[4]);}
   if(r.at("receiver")=="ocean")assert(ss[first+count-1][5]==64);
  }
 }assert(routes>0&&segments>0);
 auto v=f.inspectVolcanoes(-8192,8192).at(0);int cx=v.at("x"),cz=v.at("z");J cross=J::array();
 for(int x=-180;x<=180;x+=2){auto s=f.surface(cx+x+32768,cz+32768,false);J column=J::array();for(int y=-64;y<=400;y+=2)column.push_back(f.material(s,cx+x+32768,y,cz+32768));cross.push_back(column);}
 J report{{"seaLevel",64},{"oceanColumns",water},{"warmerMountainSnowClimateColumns",snow},{"routes",routes},{"segments",segments},{"volcano",v},{"crossSection",cross},{"gameplayTested",false}};
 std::ofstream(argv[2])<<report.dump();report.erase("crossSection");std::cout<<report.dump()<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
