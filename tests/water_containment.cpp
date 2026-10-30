#undef NDEBUG
#include "native/Field.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <chrono>
namespace nativegen {
struct WaterContainmentRegression {
 static J shores(Field&f,int X,int Z,int radius){
  int dry=0,eroded=0,lakeLeaks=0,wet=0,river=0,connections=0,disconnected=0;
  for(int z=Z-radius;z<=Z+radius;++z)for(int x=X-radius;x<=X+radius;++x){
   auto a=f.surface(x+32768,z+32768,false);
   if(a.water<=a.height||(a.river<=0&&a.lake!=1))continue;
   ++wet;river+=a.river>0;
   for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){
    auto b=f.surface(x+dx+32768,z+dz+32768,false);
    if(b.height<a.water&&b.water<a.water){
     lakeLeaks+=a.lake==1;
     if(b.water<=b.height){++dry;eroded+=f.compute(x+dx+32768,z+dz+32768,false).height>=a.water;}
    }
    if(a.river>0&&b.water>b.height){++connections;disconnected+=a.height>=b.water||b.height>=a.water;}
   }
  }
  return {{"wetColumns",wet},{"riverColumns",river},{"dryOpenFaces",dry},{"erodedBanks",eroded},{"lakeOpenFaces",lakeLeaks},{"connections",connections},{"disconnected",disconnected}};
 }
 static J feedback(J old){
  J next=old;next["config"]["waterContainment"]=1;
  Field before(old),after(next);J out;
  for(auto [label,p]:{std::pair{"river",std::pair{3749,4530}},std::pair{"lake",std::pair{3878,4753}}}){
   auto a=shores(before,p.first,p.second,48),b=shores(after,p.first,p.second,48);
   std::cout<<label<<" before "<<a<<" after "<<b<<std::endl;
   assert(a["dryOpenFaces"].get<int>()>0&&b["dryOpenFaces"]==0&&b["lakeOpenFaces"]==0);
   assert(b["wetColumns"].get<int>()>100&&b["riverColumns"].get<int>()>50&&b["disconnected"]==0);
   out[label]={{"before",a},{"after",b}};
  }
  auto p=after.inspectRegionalDrainage(1,2);int checked=0;
  for(auto const&l:p["lakes"]){
   if(l["center"]!=J::array({3886,4752}))continue;
   int level=l["level"],columns=0,edges=0;
   for(auto const&r:l["rows"])for(int x=r[1];x<=r[2];++x){int z=r[0];auto a=after.surface(x+32768,z+32768,false);assert(a.lake==1&&a.water==level);++columns;
    for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){auto b=after.surface(x+dx+32768,z+dz+32768,false);assert(b.height>=level||b.water>=level);edges+=b.lake!=1;}
   }assert(columns==4102&&edges>0);out["wholeReportedLake"]={{"columns",columns},{"edges",edges},{"level",level}};++checked;
  }assert(checked==1);return out;
 }
 static J cliffs(J data){
  Field f(data);J sites=J::array(),drops=J::array();int candidates=0;
  for(int gz=224;gz<288;++gz)for(int gx=224;gx<288;++gx){
   auto p=f.patch(gx,gz);if(!p.valid)continue;++candidates;
   if(sites.size()>=12)continue;
   int X=roundJS(p.x),Z=roundJS(p.z),drop=0;J pair;
   for(int z=Z-4;z<=Z+4;++z)for(int x=X-4;x<=X+4;++x){
    auto a=f.compute(x,z,false);
    for(auto [dx,dz]:{std::pair{2,0},std::pair{0,2}}){auto b=f.compute(x+dx,z+dz,false);int d=std::abs(a.height-b.height);
     if(d>drop){drop=d;pair={{x-32768,a.height,z-32768},{x+dx-32768,b.height,z+dz-32768}};}
    }
   }drops.push_back(drop);if(drop>=60&&drop<=100)sites.push_back({{"dropWithinTwoBlocks",drop},{"columns",pair}});
  }std::cout<<"cliff candidates "<<candidates<<" drops "<<drops<<std::endl;assert(sites.size()==12);return {{"candidates",candidates},{"sites",sites}};
 }
 static J waterfalls(){
  using namespace regional;J out=J::array();
  for(int height:{30,60,100}){
   auto valley=[&](int x,int z){return Ground{int(96+(z+512)*.05+height*smooth(885.,886.,double(z))+std::abs(x-900)*.002),.7,.5,0,0};};
   J lakes={{"owned",J::array()},{"nearby",J::array()}};
   auto p=drainage(0,0,7,valley,true,100,511,true,true,true,true,&lakes);double maxDrop=0;
   for(auto const&s:p["segments"])if(double(s[11])>0)maxDrop=std::max(maxDrop,double(s[4])-double(s[5]));
   std::cout<<"synthetic ledge "<<height<<" profile "<<maxDrop<<std::endl;
   assert(maxDrop>=height-5&&maxDrop<=100);out.push_back({height,maxDrop,p["routes"].size()});
  }return out;
 }
 static J audit(J data){
  Field f(data);J out=J::array();
  for(auto [gx,gz]:{std::pair{0,0},{1,2},{2,2},{-1,0}}){
   auto plan=f.inspectRegionalDrainage(gx,gz);int lakes=0,edges=0,riverSamples=0,falls=0;J failures=J::array();
   for(auto const&l:plan["lakes"]){
    std::set<std::pair<int,int>> wet;for(auto const&r:l["rows"])for(int x=r[1];x<=r[2];++x)wet.emplace(x,r[0]);
    for(auto [x,z]:wet)for(auto [dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){
     if(wet.contains({x+dx,z+dz}))continue;++edges;
     auto a=f.surface(x+32768,z+32768,false),b=f.surface(x+dx+32768,z+dz+32768,false);
     if(a.lake!=1||b.height<a.water&&b.water<a.water)failures.push_back({x,z,a.water,b.height,b.water});
    }++lakes;
   }
   for(size_t i=0;i<plan["segments"].size();i+=4){auto const&s=plan["segments"][i];
    int x=roundJS((double(s[0])+double(s[2]))*.5),z=roundJS((double(s[1])+double(s[3]))*.5);
    auto check=shores(f,x,z,3);riverSamples+=check["riverColumns"].get<int>();
    if(check["dryOpenFaces"]!=0||check["lakeOpenFaces"]!=0||check["disconnected"]!=0)failures.push_back({x,z,check});
    falls+=s.size()>11&&double(s[11])>0&&double(s[4])-double(s[5])>=30;
   }
   J row={{"region",{gx,gz}},{"lakes",lakes},{"lakeEdges",edges},{"riverSamples",riverSamples},{"falls30to100",falls},{"failures",failures}};out.push_back(row);std::cout<<row.dump()<<std::endl;
  }
  return out;
 }
};}
int main(int argc,char**argv){try{
 if(argc!=4&&argc!=5)return 2;nativegen::J old,next;std::ifstream(argv[1])>>old;std::ifstream(argv[2])>>next;
 nativegen::regional::Store cache;nativegen::regional::StoreScope scope(cache);
 auto start=std::chrono::steady_clock::now();using T=nativegen::WaterContainmentRegression;nativegen::J out;
 if(argc==5){out=T::audit(next);std::ofstream(argv[3])<<out.dump(2);for(auto const&r:out)assert(r["failures"].empty());return 0;}
 out["feedback"]=T::feedback(old);std::cout<<out.dump()<<std::endl;
 out["cliffs"]=T::cliffs(next);out["waterfalls"]=T::waterfalls();
 out["seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::ofstream(argv[3])<<out.dump(2);std::cout<<out.dump()<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
