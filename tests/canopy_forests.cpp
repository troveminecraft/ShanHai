#undef NDEBUG
#include "native/Engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <chrono>

namespace nativegen {
struct CanopyForestRegression {
 using Chunks=std::map<std::pair<int,int>,std::vector<uint16_t>>;
 static Surface ground(int biome){Surface s;s.height=140;s.water=s.lava=-32768;s.biome=biome;s.H=.7;s.T=.55;return s;}
 static J recipe(J data){data.erase("regionalGeneration");data["version"]="karma-terrain-2.219.0-native-agent-a";
  data["config"]["forestEcologyRevision"]=3;data["capabilityProfile"]=forest_capabilities::CommonCanopy;return data;
 }
 static Chunks draw(J const&data,bool reverse,bool cherry){
  Engine e(data);std::array<Surface,256> surfaces;surfaces.fill(ground(forest_capabilities::Pale));
  if(cherry){Field::CherryGrove a{true,512,512,140,15,180.,150.,.7};e.field.cherryGroves.get(key2(0,0),[&]{return a;});}
  else {
   e.field.skinCache=Cache<uint64_t,Surface>(16384);
   for(int z=-8;z<72;++z)for(int x=-8;x<72;++x)e.field.skinCache.get(key2(x,z),[&]{return surfaces[0];});
   for(int z=-3;z<=4;++z)for(int x=-3;x<=4;++x){
    e.canopyGroundNodes.get(key2(x,z),[]{return 140;});
    e.canopyTrees.get(key2(x,z),[&]{Engine::CanopyTree a;
     if(x>=0&&x<2&&z>=0&&z<2){a.x=x*32+16;a.z=z*32+16;a.ground=140;a.height=45;a.radius=6;
      a.biome=forest_capabilities::Pale;a.door=x+z*2;a.hollow=true;a.holeHeight=11;a.lamp=true;a.heart=x==0&&z==0;a.phase=.3;}
     return a;
    });
   }
  }
  std::vector<std::pair<int,int>> points;int begin=cherry?432:0,end=cherry?592:48;
  for(int z=begin;z<=end;z+=16)for(int x=begin;x<=end;x+=16)points.emplace_back(x,z);
  if(reverse)std::reverse(points.begin(),points.end());Chunks out;
  for(auto [x,z]:points){std::vector<uint16_t> raw(256*e.Height);for(int i=0;i<256;++i)raw[i*e.Height+204]=4;
   if(cherry)e.centralCherryTree(x,z,raw,surfaces);else e.joinedCanopyForest(x,z,raw,surfaces);
   out.emplace(std::pair{x,z},std::move(raw));
  }return out;
 }
 static int block(Chunks const&chunks,int x,int y,int z){int X=int(std::floor(x/16.))*16,Z=int(std::floor(z/16.))*16;
  auto i=chunks.find({X,Z});if(i==chunks.end())return 0;int height=int(i->second.size()/256);
  return y< -64||y>=height-64?0:i->second[((z-Z)*16+x-X)*height+y+64];
 }
 static J geometry(J const&data){
  using namespace forest_palette;auto a=draw(data,false,false),b=draw(data,true,false);assert(a==b);
  int leaves=0,wood=0,lichen=0,lamps=0,hearts=0;
  for(auto const&[key,raw]:a)for(int id:raw){leaves+=id==CanopyPaleLeaves;wood+=id==PaleLog||id==PaleWood;
   lichen+=id>=RootLichenNorth&&id<=RootLichenDown;lamps+=id>=HollowLampNorth&&id<=HollowLampWest;hearts+=id==Heart;}
  assert(leaves>18000&&wood>10000&&lichen>100&&lamps==4&&hearts==1);
  assert(block(a,16,148,16)==Heart&&block(a,16,147,16)==PaleLog&&block(a,16,149,16)==PaleLog);
  for(int y=149;y<=153;++y)assert(block(a,16,y,16)==PaleLog);
  assert(block(a,16,148,15)==0||block(a,17,148,16)==0);
  constexpr int dx[4]={0,1,0,-1},dz[4]={-1,0,1,0};
  for(int z=0;z<64;++z)for(int x=0;x<64;++x){int lo=10000,hi=-10000;
   for(int y=141;y<195;++y){int id=block(a,x,y,z);if(id==CanopyPaleLeaves){lo=std::min(lo,y);hi=y;}
    if(id>=RootLichenNorth&&id<=RootLichenWest){int direction=id-RootLichenNorth,support=block(a,x+dx[direction],y,z+dz[direction]);assert(support==PaleLog||support==PaleWood);}
   }
   assert(lo<=hi);
   for(auto [X,Z]:{std::pair{x+1,z},std::pair{x,z+1}})if(X<64&&Z<64){bool connected=false;
    for(int y=lo;y<=hi;++y)connected|=block(a,X,y,Z)==CanopyPaleLeaves;assert(connected);}
  }
  auto c=draw(data,false,true),d=draw(data,true,true);assert(c==d);int cherryLeaves=0,cherryWood=0,top=0;
  for(int z=432;z<=607;++z)for(int x=432;x<=607;++x)for(int y=141;y<=215;++y){int id=block(c,x,y,z);
   cherryLeaves+=id==CanopyCherryLeaves;cherryWood+=id==CherryLog||id==CherryWood;if(id)top=std::max(top,y);
  }
  assert(cherryLeaves>150000&&cherryWood>25000&&top>=209&&top<=212);
  for(int x=500;x<=524;++x)assert(block(c,x,148,512)==CherryLog);
  return {{"canopyLeaves",leaves},{"trunkAndBranchBlocks",wood},{"supportedLichen",lichen},{"lamps",lamps},{"hearts",hearts},
   {"cherryLeaves",cherryLeaves},{"cherryWood",cherryWood},{"cherryHeight",top-140},{"orderIndependentChunks",a.size()+c.size()},{"passed",true}};
 }
 static J sites(J const&data){
  Engine e(data);J groves=J::array(),trees=J::array();int dark=0,pale=0,hollows=0,lamps=0,hearts=0;
  for(int gz=512;gz<1536;gz+=7)for(int gx=512;gx<1536;gx+=7){auto a=e.canopyTree(gx,gz);if(!a.height)continue;
   assert(a.radius>=5&&a.radius<=7&&a.height>=40&&a.height<=50);dark+=a.biome==forest_capabilities::Dark;pale+=a.biome==forest_capabilities::Pale;
   hollows+=a.hollow;lamps+=a.lamp;hearts+=a.heart;if(trees.size()<12)trees.push_back({a.x-32768,a.ground,a.z-32768,a.biome,a.radius,a.height});
  }
  for(int gz=0;gz<64&&groves.size()<4;++gz)for(int gx=0;gx<64&&groves.size()<4;++gx){auto a=e.field.cherryGroveSite(gx,gz);if(!a.valid)continue;
   auto center=e.field.surface(a.x,a.z,false);assert(center.biome==forest_capabilities::Cherry&&center.water<center.height);
   int low=a.x,high=a.x;for(int x=a.x-300;x<=a.x+300;++x)if(e.field.cherryGroveRadius(a,x,a.z)<=1){low=std::min(low,x);high=std::max(high,x);}
   assert(high-low+1>=160&&high-low+1<=480);groves.push_back({{"center",{a.x-32768,a.ground,a.z-32768}},{"span",high-low+1},{"trunkRadius",a.trunkRadius}});
  }
  assert(dark>=8&&pale>=1&&groves.size()>=1);
  return {{"darkTrees",dark},{"paleTrees",pale},{"hollows",hollows},{"lamps",lamps},{"hearts",hearts},{"trees",trees},{"groves",groves},{"passed",true}};
 }
};}
int main(int argc,char**argv){try{
 if(argc!=4)return 2;nativegen::J data;std::ifstream(argv[1])>>data;data=nativegen::CanopyForestRegression::recipe(std::move(data));
 auto start=std::chrono::steady_clock::now();auto result=std::string(argv[3])=="sites"?nativegen::CanopyForestRegression::sites(data):nativegen::CanopyForestRegression::geometry(data);
 result["seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::ofstream(argv[2])<<result.dump(2);std::cout<<result<<std::endl;
 }catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
