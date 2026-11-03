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
 static Chunks draw(J const&data,bool reverse,bool cherry,bool sloped=false,std::vector<std::array<int,4>> reservations={},int trunk=15,double phase=.7,std::vector<double>* timings=nullptr){
  Engine e(data,reservations);std::array<Surface,256> surfaces;surfaces.fill(ground(forest_capabilities::Pale));
  if(cherry){Field::CherryGrove a{true,512,512,140,trunk,180.,150.,phase};e.field.cherryGroves.get(key2(0,0),[&]{return a;});}
  else {
   e.field.skinCache=Cache<uint64_t,Surface>(16384);
   for(int z=-8;z<72;++z)for(int x=-8;x<72;++x)e.field.skinCache.get(key2(x,z),[&]{auto s=surfaces[0];if(sloped)s.height=136+int(std::floor((x%32)/4.));return s;});
   for(int z=-3;z<=4;++z)for(int x=-3;x<=4;++x){
    e.canopyGroundNodes.get(key2(x,z),[]{return 140;});
    e.canopyTrees.get(key2(x,z),[&]{Engine::CanopyTree a;
     if(x>=0&&x<2&&z>=0&&z<2){a.x=x*32+16;a.z=z*32+16;a.ground=140;a.height=45;a.radius=6;
      a.biome=forest_capabilities::Pale;a.door=x+z*2;a.doorAngle=a.door*1.570796327+.18;a.hollow=true;a.holeHeight=11;a.lamp=true;a.heart=x==0&&z==0;a.phase=.3;}
     return a;
    });
   }
  }
  std::vector<std::pair<int,int>> points;int begin=cherry?432:0,end=cherry?592:48;
  for(int z=begin;z<=end;z+=16)for(int x=begin;x<=end;x+=16)points.emplace_back(x,z);
  if(reverse)std::reverse(points.begin(),points.end());Chunks out;
  for(auto [x,z]:points){std::vector<uint16_t> raw(256*e.Height);for(int i=0;i<256;++i){surfaces[i].height=sloped?136+int(std::floor(((x+i%16)%32)/4.)):140;raw[i*e.Height+surfaces[i].height+64]=4;}
   auto start=std::chrono::steady_clock::now();
   if(cherry)e.centralCherryTree(x,z,raw,surfaces);else e.joinedCanopyForest(x,z,raw,surfaces);
   if(timings)timings->push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
   out.emplace(std::pair{x,z},std::move(raw));
  }return out;
 }
 static J naturalGeometry(J data){
  data["config"]["naturalForestRevision"]=1;using namespace forest_palette;
  auto a=draw(data,false,false,true),b=draw(data,true,false,true);assert(a==b);
  int bark=0,lamps=0,hearts=0,clearFloor=0;
  for(auto const&[key,raw]:a)for(int id:raw){bark+=id==PaleWood;lamps+=id>=HollowLampNorth&&id<=HollowLampWest;hearts+=id==Heart;}
  assert(bark>1000&&lamps==4&&hearts==1);
  assert(block(a,16,143,16)==Heart);
  for(int y=141;y<143;++y)assert(block(a,16,y,16)==PaleLog);
  assert(block(a,16,144,16)==PaleLog);
  for(int z=0;z<64;++z)for(int x=0;x<64;++x){int floor=136+(x%32)/4;assert(block(a,x,floor,z)==4);
   if(x>=18&&x<=23&&z>=15&&z<=17&&block(a,x,floor+1,z)==0)++clearFloor;}
  assert(clearFloor>=8);
  // Geometry stays asymmetric without changing the shared roof or creating
  // floating horizontal rings at the old, tree-centre floor height.
  Engine e(data);Engine::CanopyTree t;t.x=16;t.z=16;t.ground=140;t.height=45;t.radius=6;t.phase=.3;t.hollow=true;t.holeHeight=11;t.doorAngle=.18;
  int asymmetry=0;
  for(int y=137;y<151;++y)for(int x=13;x<25;++x)for(int dz=1;dz<=5;++dz)
   asymmetry+=e.canopyHollow(t,x,y,16+dz)!=e.canopyHollow(t,x,y,16-dz);
  assert(asymmetry>30);
  return {{"barkBlocks",bark},{"lamps",lamps},{"groundedHearts",hearts},{"naturalFloorColumns",clearFloor},{"asymmetricHollowSamples",asymmetry},{"orderIndependentChunks",a.size()},{"passed",true}};
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
 static J branching(J data,std::string const&path,int revision=2,int trunk=15,double phase=.7){
  data["config"]["cherryRevision"]=revision;using namespace forest_palette;
  std::vector<double> timings;
  auto a=draw(data,false,true,false,{},trunk,phase,&timings),b=draw(data,true,true,false,{},trunk,phase);assert(a==b);
  int leaves=0,wood=0,columns=0,top=0,baseSlice=0,lowLeaves=0,centreTop=0,outerTop=0;double leafHeight=0,woodHeight=0;std::set<std::array<int,3>> timber;
  std::ofstream binary(path+".voxels",std::ios::binary);
  for(int z=432;z<=607;++z)for(int x=432;x<=607;++x){bool covered=false;
   for(int y=141;y<=220;++y){uint16_t id=block(a,x,y,z);binary.write((char*)&id,2);
    leaves+=id==CanopyCherryLeaves;covered|=id==CanopyCherryLeaves;
    if(id==CanopyCherryLeaves){leafHeight+=y-140;lowLeaves+=y<170;
     double radius=std::hypot(x-512,z-512);if(radius<=16)centreTop=std::max(centreTop,y-140);if(radius>=40)outerTop=std::max(outerTop,y-140);}
    if(id==CherryLog||id==CherryWood){++wood;woodHeight+=y-140;baseSlice+=y==150;timber.insert({x,y,z});}if(id)top=std::max(top,y);
   }columns+=covered;
  }
  binary.close();
  // A detached ornamented crown must not pass a height-only assertion.
  std::vector<std::array<int,3>> queue;
  for(auto p:timber)if(p[1]==141)queue.push_back(p);
  for(auto p:queue)timber.erase(p);
  for(size_t i=0;i<queue.size();++i){auto p=queue[i];for(auto d:std::array<std::array<int,3>,6>{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}}){
   auto q=std::array{p[0]+d[0],p[1]+d[1],p[2]+d[2]};if(timber.erase(q))queue.push_back(q);}}
  assert(timber.empty());assert(top>=207&&top<=218&&wood>(revision>=3?trunk*trunk*(revision>=4?12:25):10000));
  assert(leaves>15000&&leaves<100000&&columns<8000);
  if(revision>=4)assert(centreTop>=outerTop+6);
  if(revision>=3)for(int variant=0;variant<64;++variant){auto model=worldgen_rules::natural_cherry::build(10+variant%11,variant*.71,
    [&](int salt){return hash01(variant, salt, 89701);},revision>=4);
   assert(model.branches.size()<1024&&model.tufts.size()<512);
   for(auto const&b:model.branches)assert(std::max({std::abs(b.x),std::abs(b.z),std::abs(b.X),std::abs(b.Z)})+std::max(b.r,b.R)<80);
   for(auto const&t:model.tufts)assert(std::max(std::abs(t.x),std::abs(t.z))+std::max(t.rx,t.rz)*1.12<80&&t.y+t.ry*1.1<80);
  }
  std::sort(timings.begin(),timings.end());
  return {{"leaves",leaves},{"wood",wood},{"height",top-140},{"leafColumns",columns},{"trunkRadius",trunk},
    {"baseSliceWood",baseSlice},{"leafMeanHeight",leafHeight/leaves},{"woodMeanHeight",woodHeight/wood},{"visualMeanHeight",(leafHeight+woodHeight)/(leaves+wood)},{"leavesBelow30",lowLeaves},{"centreCrownTop",centreTop},{"outerCrownTop",outerTop},
    {"geometryMs",{{"median",timings[timings.size()/2]},{"p95",timings[timings.size()*95/100]},{"maximum",timings.back()}}},
    {"allWoodGroundConnected",true},{"chunksReordered",a.size()},{"passed",true}};
 }
 static J reservations(J data){
  data["config"]["forestReservationRevision"]=1;data["config"]["naturalForestRevision"]=1;
  // The building touches a root but misses its anchor. The complete tree
  // must be omitted in every neighbouring chunk, not sliced by the box.
  auto full=draw(data,false,false),filtered=draw(data,false,false,false,{{24,14,26,18}});
  auto reverse=draw(data,true,false,false,{{24,14,26,18}});assert(filtered==reverse);
  using namespace forest_palette;int removed=0,kept=0;
  for(int z=0;z<64;++z)for(int x=0;x<64;++x)for(int y=141;y<184;++y){
   int a=block(full,x,y,z),b=block(filtered,x,y,z);
   if(a==PaleWood||a==PaleLog){if(x<32&&z<32){assert(b!=PaleWood&&b!=PaleLog);++removed;}
    else if(b==a)++kept;}
  }
  assert(removed>1000&&kept>3000);
  return {{"removedWholeTreeWood",removed},{"neighbourWoodRetained",kept},{"chunksReordered",filtered.size()},{"passed",true}};
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
 if(argc!=4&&argc!=6)return 2;nativegen::J data;std::ifstream(argv[1])>>data;data=nativegen::CanopyForestRegression::recipe(std::move(data));
 auto start=std::chrono::steady_clock::now();auto mode=std::string(argv[3]);auto result=mode=="reservations"?nativegen::CanopyForestRegression::reservations(data):mode=="branching"||mode=="natural_cherry"||mode=="full_cherry"?nativegen::CanopyForestRegression::branching(data,argv[2],mode=="full_cherry"?4:mode=="natural_cherry"?3:2,argc==6?std::stoi(argv[4]):15,argc==6?std::stod(argv[5]):.7):mode=="sites"?nativegen::CanopyForestRegression::sites(data):mode=="natural"?nativegen::CanopyForestRegression::naturalGeometry(data):nativegen::CanopyForestRegression::geometry(data);
 result["seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::ofstream(argv[2])<<result.dump(2);std::cout<<result<<std::endl;
 }catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
