#undef NDEBUG
#include "native/Engine.h"
#include "BuildHeight.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace nativegen {
struct IceSnowRegression {
 static std::vector<uint8_t> encode(std::vector<uint16_t>const&raw,std::vector<terrain_snow::Plant>const&plants){
  std::vector<uint8_t>out(260);out[0]=75;out[1]=87;out[2]=50;
  auto word=[&](int n){out.push_back(uint8_t(n));out.push_back(uint8_t(n>>8));};
  for(int c=0;c<256;++c){size_t begin=out.size();word(0);int count=0;
   for(int y=0;y<576;){int e=y+1;while(e<576&&raw[c*576+y]==raw[c*576+e])++e;
    word(e-y);word(raw[c*576+y]);++count;y=e;}
   out[begin]=uint8_t(count);out[begin+1]=uint8_t(count>>8);
  }terrain_snow::append(out,plants);return out;
 }
 static void cliffs(){
  auto surface=[](int x,int z){Surface s;s.height=x>=16?100:300;s.glacier=1;s.superGlacier=1;return s;};
  auto generate=[&](int cx,bool icy){std::array<Surface,256>s;std::vector<uint16_t>raw(256*576);
   for(int i=0;i<256;++i){s[i]=surface(cx*16+i%16,i/16);if(!icy)s[i].superGlacier=0;
    for(int y=-64;y<=s[i].height;++y)raw[i*576+y+64]=y<-60?1:2;
    if(cx==0&&i%16==15){raw[i*576+264]=0;raw[i*576+265]=8;raw[i*576+266]=104;}
   }
   auto before=raw;coatGlacierCliffs(576,raw.data(),s,[&](int x,int z){return surface(cx*16+x,z);});
   if(!icy)assert(raw==before);
   for(int i=0;i<256;++i)for(int y=-64;y<512;++y){auto p=i*576+y+64;
    bool replace=icy&&cx==0&&i%16==15&&y>100&&y<=300&&before[p]==2;
    if(replace)assert(raw[p]==143||raw[p]==144);else assert(raw[p]==before[p]);
   }return raw;
  };
  auto a=generate(0,true),b=generate(1,true);assert(b==generate(1,true)&&a==generate(0,true));generate(0,false);
 }
 static void snow(J data,std::filesystem::path const&out){
  data["config"]["snowPlantLayers"]=1;Field f(data);Engine e(data,{},false,&f);
  std::array<Surface,256>s;std::vector<uint16_t>raw(256*576);
  for(int i=0;i<256;++i){s[i].height=200;s[i].snowCover=1;raw[i*576+264]=3;
   int b=std::array{36,37,38,40,71,36,0,36}[i%8];raw[i*576+265]=b;
   if(b==38||b==40)raw[i*576+266]=b+1;
   if(i%8==5)raw[i*576+264]=0; // unsupported grass must not acquire snow
   if(i%8==7)s[i].snowCover=0;
  }
  auto original=raw;e.variantSnowCover(32768,32768,raw,s);assert(e.snowPlants.size()==128);
  for(int i=0;i<256;++i){assert(raw[i*576+265]==(i%8<4||i%8==6?146:original[i*576+265]));
   assert(raw[i*576+266]==original[i*576+266]);}
  auto encoded=encode(raw,e.snowPlants);
  auto c=terrain_runtime::decodeChunk(encoded,256,255,576);assert(c.snowPlants.size()==128);
  std::ofstream bin(out/"snow-plants.kw3",std::ios::binary);bin.write((char*)encoded.data(),encoded.size());bin.close();
  // Validate the same primary volume through both Bedrock index layouts.
  std::vector<int>palette(256);for(int i=0;i<256;++i)palette[i]=i;
  terrain_runtime::BuildHeight bounds{512,512};
  for(bool vertical:{false,true}){std::vector<int>volume(bounds.blocks());
   auto index=[&](int x,int y,int z){return vertical?(x*16+z)*576+y:(x*16+z)+y*256;};
   terrain_runtime::fillGeneratedBlockVolume<int>(volume,palette,c,bounds,index);
   for(auto p:c.snowPlants)assert(volume[index(p.column%16,p.y,p.column/16)]==146);
  }
  auto bad=[&](std::vector<uint8_t>const&bytes){bool rejected=false;try{terrain_runtime::decodeChunk(bytes,256,255,576);}catch(std::runtime_error const&){rejected=true;}assert(rejected);};
  size_t footer=encoded.size()-2-128*5;auto b=encoded;b.pop_back();bad(b);
  b=encoded;b[footer]=0;b[footer+1]=0;bad(b);
  b=encoded;b[footer+2+5]=b[footer+2];bad(b); // duplicate column
  b=encoded;b[footer+3]=0;b[footer+4]=0;bad(b);
  b=encoded;b[footer+5]=2;bad(b); // not a plant
  auto changed=raw;changed[265]=0;bad(encode(changed,e.snowPlants));
  changed=raw;changed[2*576+266]=0;bad(encode(changed,e.snowPlants));
  auto old=data;old["config"].erase("snowPlantLayers");Engine legacy(old,{},false);auto unlayered=original;
  legacy.variantSnowCover(32768,32768,unlayered,s);assert(legacy.snowPlants.empty());
  for(int i=0;i<256;++i)if(i%8<4)assert(unlayered[i*576+265]==original[i*576+265]);
  auto v2=encode(unlayered,{});assert(v2[2]==50&&terrain_runtime::decodeChunk(v2,256,255,576).snowPlants.empty());
 }
 static J real(J data,std::filesystem::path const&out){
  J next=data;next["config"]["glacierCliffSkin"]=1;next["config"]["snowPlantLayers"]=1;
  Field f(next);Engine e(next,{},false,&f),old(data,{},false);
  auto regions=f.inspectIceRegions();J result=J::array();std::set<std::pair<int,int>> chosen;
  int covered=0,plants=0;double oldMs=0,newMs=0;
  auto run=[&](int cx,int cz){
   if(chosen.size()>=24||!chosen.emplace(cx,cz).second)return;
   auto t=std::chrono::steady_clock::now();auto before=old.chunk(cx,cz);oldMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
   t=std::chrono::steady_clock::now();auto bytes=e.chunk(cx,cz);newMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
   auto decoded=terrain_runtime::decodeChunk(bytes,256,255,576),previous=terrain_runtime::decodeChunk(before,256,255,576);
   std::vector<int> pal(256),a(576*256),b(a.size());for(int i=0;i<256;++i)pal[i]=i;
   auto index=[](int x,int y,int z){return (z*16+x)*576+y;};terrain_runtime::BuildHeight bounds{512,512};
   terrain_runtime::fillGeneratedBlockVolume<int>(a,pal,decoded,bounds,index);terrain_runtime::fillGeneratedBlockVolume<int>(b,pal,previous,bounds,index);
   int ice=0;for(size_t i=0;i<a.size();++i)if(a[i]!=b[i]){
    if(a[i]==143||a[i]==144){assert(b[i]==2||(b[i]>=95&&b[i]<=99));++ice;}
    else assert(a[i]==146&&terrain_snow::plant(b[i]));
   }
   covered+=ice;plants+=int(decoded.snowPlants.size());assert(bytes==e.chunk(cx,cz));
   result.push_back({{"chunk",{cx,cz}},{"ice",ice},{"snowPlants",decoded.snowPlants.size()},{"bytes",bytes.size()}});
   if(decoded.snowPlants.size()){auto path=out/("real-snow-"+std::to_string(cx)+"-"+std::to_string(cz)+".kw3");std::ofstream bin(path,std::ios::binary);bin.write((char*)bytes.data(),bytes.size());}
  };
  for(auto const&r:regions){
   int gx=int(std::floor((r.at("x").get<double>()+32768)/256)),gz=int(std::floor((r.at("z").get<double>()+32768)/256));
   for(int dz=-6;dz<=6&&chosen.size()<12;++dz)for(int dx=-6;dx<=6&&chosen.size()<12;++dx){
    auto p=f.patch(gx+dx,gz+dz);if(!p.valid)continue;
    double offset=cliff_transition::traceOffset(p.trace,0);
    int x=roundJS(p.x+p.nx*offset),z=roundJS(p.z+p.nz*offset);
    auto s=f.surface(x,z);if(s.superGlacier<=.5||s.glacier<=.5)continue;
    int cx=int(std::floor((x-32768)/16.)),cz=int(std::floor((z-32768)/16.));
    run(cx,cz);run(cx+1,cz);run(cx-1,cz);run(cx,cz+1);run(cx,cz-1);
   }if(chosen.size()>=12)break;
  }
  // Sample the glacier fringe where vegetation remains and snow cover begins.
  for(auto const&r:regions){double rx=r.at("radiusX"),rz=r.at("radiusZ");
   for(int i=0;i<48&&chosen.size()<24&&plants<24;++i){double angle=i*6.283185307179586/48;
    int x=roundJS(r.at("x").get<double>()+std::cos(angle)*(rx*1.08+300)),z=roundJS(r.at("z").get<double>()+std::sin(angle)*(rz*1.08+300));
    auto s=f.surface(x+32768,z+32768);if(s.snowCover<.6||s.kind||s.water>=s.height||s.iceHabitat>.05||s.rock||s.barren>.01||s.stone>.01||s.height>330||s.glacierVegetation<.3)continue;
    run(int(std::floor(x/16.)),int(std::floor(z/16.)));
   }if(chosen.size()>=24||plants>=24)break;
  }
  assert(covered>0&&plants>0);return {{"samples",result},{"iceBlocks",covered},{"snowPlants",plants},{"oldMs",oldMs},{"newMs",newMs},{"regions",regions.size()}};
 }
};
}
int main(int argc,char**argv){try{if(argc!=3)return 2;nativegen::J data;std::ifstream(argv[1])>>data;std::filesystem::path out(argv[2]);std::filesystem::create_directories(out);
 nativegen::regional::Store store;nativegen::regional::StoreScope scope(store);nativegen::regional::storage(data,out/"regional-cache");
 nativegen::IceSnowRegression::cliffs();nativegen::IceSnowRegression::snow(data,out);
 auto result=nativegen::IceSnowRegression::real(data,out);result["passed"]=true;
 std::ofstream(out/"ice-snow-regression.json")<<result.dump(2);std::cout<<result<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
