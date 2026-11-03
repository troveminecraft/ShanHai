#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>

using namespace nativegen;
using Point=std::pair<int,int>;
using Columns=std::map<Point,Surface>;
Columns area(Field& f,int X,int Z,int radius){
 Columns out;
 for(int z=Z-radius;z<=Z+radius;++z)for(int x=X-radius;x<=X+radius;++x)
  out.emplace(Point{x,z},f.surface(x+32768,z+32768,false));
 return out;
}
J inspect(Columns const& a){
 int edges=0,step=0,open=0,blocked=0;
 for(auto const&[p,s]:a){
  if(s.water<=s.height)continue;
  for(auto [dx,dz]:{Point{1,0},{-1,0},{0,1},{0,-1}}){
   auto it=a.find({p.first+dx,p.second+dz});if(it==a.end())continue;auto const&b=it->second;
   if(s.lake==1&&b.river>0&&b.lake!=1){++edges;step=std::max(step,std::abs(s.height-b.height));}
   open+=b.water<=b.height&&b.height<s.water;
   blocked+=b.water>b.height&&(b.height>=s.water||s.height>=b.water);
  }
 }
 return {{"mouthEdges",edges},{"maximumBedStep",step},{"dryOpenFaces",open},{"blockedWaterFaces",blocked}};
}
std::vector<uint16_t> decode(std::vector<uint8_t> const& b,int height){
 assert(b.size()>260&&b[0]=='K'&&b[1]=='W'&&b[2]=='2');size_t at=260;
 auto word=[&]{assert(at+2<=b.size());int n=b[at]|(int(b[at+1])<<8);at+=2;return n;};
 std::vector<uint16_t> out(256*height);
 for(int i=0;i<256;++i){int count=word(),y=0;for(int k=0;k<count;++k){int n=word(),v=word();assert(n>0&&y+n<=height);
  std::fill(out.begin()+i*height+y,out.begin()+i*height+y+n,uint16_t(v));y+=n;}assert(y==height);}
 return out;
}
int main(int argc,char**argv){try{
 if(argc!=3)return 2;J data;std::ifstream(argv[1])>>data;J next=data;next["config"]["lakeJunctionRevision"]=1;
 regional::Store store;regional::StoreScope scope(store);Field old(data),now(next);J report=J::array();
 std::set<Point> chunks;int changed=0,unchangedWater=0;
 for(auto [x,z]:{Point{744,-1392},{372,-1561},{296,-1501}}){
  auto a=area(old,x,z,40),b=area(now,x,z,40);auto A=inspect(a),B=inspect(b);
  assert(A["maximumBedStep"].get<int>()>=5&&B["maximumBedStep"].get<int>()<=2);
  assert(B["mouthEdges"]==A["mouthEdges"]&&B["dryOpenFaces"]==0&&B["blockedWaterFaces"]==0);
  for(auto const&[p,s]:a){auto const&t=b.at(p);assert(s.water==t.water);++unchangedWater;
   if(s.height!=t.height){assert(s.lake==1&&t.height<s.height);++changed;}
  }
  for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)chunks.emplace(int(std::floor(x/16.))+dx,int(std::floor(z/16.))+dz);
  report.push_back({{"position",{x,z}},{"before",A},{"after",B}});
 }
 assert(changed>100);
 // Decode the production voxel pipeline, including every later feature pass.
 // Check actual water/ground faces and repeat the complete chunks in reverse.
 Engine engine(next),reverse(next);int height=now.generationHeight(),voxelFaces=0;
 std::map<Point,std::vector<uint8_t>> saved;
 std::map<Point,std::vector<uint16_t>> raw;
 for(auto p:chunks){saved[p]=engine.chunk(p.first,p.second);raw[p]=decode(saved[p],height);}
 for(auto it=chunks.rbegin();it!=chunks.rend();++it)assert(saved.at(*it)==reverse.chunk(it->first,it->second));
 auto block=[&](int x,int y,int z){int cx=int(std::floor(x/16.)),cz=int(std::floor(z/16.));
  auto it=raw.find({cx,cz});assert(it!=raw.end());return it->second[((z-cz*16)*16+x-cx*16)*height+y+64];};
 for(auto [cx,cz]:chunks)for(int z=cz*16;z<cz*16+16;++z)for(int x=cx*16;x<cx*16+16;++x){
  auto s=now.surface(x+32768,z+32768,false);if(s.lake!=1||s.river<=0)continue;
  for(auto [dx,dz]:{Point{1,0},{0,1}}){int X=x+dx,Z=z+dz;
   if(!chunks.contains({int(std::floor(X/16.)),int(std::floor(Z/16.))}))continue;
   auto t=now.surface(X+32768,Z+32768,false);if(t.water<=t.height)continue;
   int lo=std::max(s.height,t.height)+1,hi=std::min(s.water,t.water);assert(lo<=hi);
   bool wet=false;for(int y=lo;y<=hi;++y)wet|=block(x,y,z)==8&&block(X,y,Z)==8;assert(wet);++voxelFaces;
  }
 }
 assert(voxelFaces>100);
 // More lake mouths, including negative coordinates and different plan owners.
 J extra=J::array();
 for(auto [gx,gz]:{Point{0,-1},{-1,-1},{1,0}}){auto plan=now.inspectRegionalDrainage(gx,gz);int checked=0;
  for(auto const&r:plan["routes"]){if(r["receiver"]!="lake"||checked>=4)continue;
   int x=r["join"][0],z=r["join"][1];auto v=inspect(area(now,x,z,32));
   assert(v["dryOpenFaces"]==0&&v["blockedWaterFaces"]==0);
   extra.push_back({{"position",{x,z}},{"faces",v}});++checked;
  }
 }
 J out={{"reportedSites",report},{"changedSubmergedLakeColumns",changed},{"unchangedWaterColumns",unchangedWater},
        {"decodedChunkCount",chunks.size()},{"actualConnectedWaterFaces",voxelFaces},{"orderIndependent",true},{"otherMouths",extra},{"passed",true}};
 std::ofstream(argv[2])<<out.dump(2);std::cout<<out.dump()<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
