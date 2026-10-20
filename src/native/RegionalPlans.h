#pragma once
#include "Noise.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <sstream>
#include <deque>

namespace nativegen::regional {
inline constexpr int Core=2048,Halo=512,Pitch=64,Count=(Core+Halo*2)/Pitch+1;
inline constexpr char Algorithm[]="regional-drainage-v1";
inline uint64_t digest(std::string const&s){uint64_t h=1469598103934665603ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
inline std::string identity(J const&data){
 J key{{"algorithm",Algorithm},{"seed",data.at("seedManifest").at("terrainSeed")},
       {"config",data.at("config")},{"template",data.at("seedManifest").at("templateSha256")}};
 return std::to_string(digest(key.dump()));
}
inline std::recursive_mutex mutex;
inline std::unordered_map<std::string,std::shared_ptr<J const>> plans;
inline std::deque<std::string> residentOrder;
inline std::unordered_map<std::string,std::filesystem::path> folders;
inline void storage(J const&data,std::filesystem::path const&folder){
 if(!data.contains("regionalGeneration"))return;
 std::lock_guard lock(mutex);folders[identity(data)]=folder;
}
template<class F> std::shared_ptr<J const> get(std::string const&id,char const*kind,int gx,int gz,F make){
 std::lock_guard lock(mutex);
 auto name=std::string(kind)+"-"+std::to_string(gx)+"-"+std::to_string(gz);
 auto key=id+":"+name;
 if(auto i=plans.find(key);i!=plans.end())return i->second;
 std::filesystem::path path;
 if(auto dir=folders.find(id);dir!=folders.end())path=dir->second/id/(name+".json");
 J value;
 if(!path.empty()&&std::filesystem::exists(path))try{
  std::ifstream in(path);J saved;in>>saved;
  if(saved.at("identity")==id&&saved.at("algorithm")==Algorithm&&saved.at("checksum")==std::to_string(digest(saved.at("plan").dump())))value=saved.at("plan");
 }catch(std::exception const&){} // Regenerate an incomplete cache from the same recipe.
 if(value.is_null()){
  value=make();
  if(!path.empty())try{
   std::filesystem::create_directories(path.parent_path());auto pending=path;pending+=".tmp";
   {std::ofstream out(pending);out<<J{{"identity",id},{"algorithm",Algorithm},{"checksum",std::to_string(digest(value.dump()))},{"plan",value}}.dump();if(!out)throw std::runtime_error("Regional cache write failed");}
   std::error_code ec;std::filesystem::rename(pending,path,ec);
   if(ec)std::filesystem::remove(pending,ec); // The deterministic recipe remains usable without disk cache.
  }catch(std::exception const&){}
 }
 auto result=std::make_shared<J const>(std::move(value));plans.emplace(key,result);residentOrder.push_back(key);
 if(residentOrder.size()>384){plans.erase(residentOrder.front());residentOrder.pop_front();}return result;
}
struct Ground{int height;double humidity,temperature,stone,volcanic;};
template<class Sample> J drainage(int gx,int gz,uint32_t seed,Sample sample){
 constexpr int N=Count,Total=N*N;
 int originX=gx*Core-Halo,originZ=gz*Core-Halo;
 std::array<Ground,Total> ground;
 auto point=[&](int i){return std::pair{originX+(i%N)*Pitch,originZ+(i/N)*Pitch};};
 auto eligible=[](Ground const&s){return s.stone<=.001&&s.volcanic<=0;};
 for(int i=0;i<Total;++i){auto[x,z]=point(i);ground[i]=sample(x,z);}
 J lakes=J::array(),segments=J::array();
 std::array<double,Total> sink;sink.fill(-1);
 for(int i=0;i<Total;++i)if(ground[i].height<31&&eligible(ground[i]))sink[i]=31;
 // Closed basins are tested against their actual surrounding terrain. A lake
 // owns a small bounded footprint wholly inside its canonical region.
 for(int z=10;z<N-10&&lakes.size()<3;++z)for(int x=10;x<N-10&&lakes.size()<3;++x){
  int i=z*N+x;auto const&s=ground[i];auto[X,Z]=point(i);
  if(s.height<40||s.height>235||s.humidity<.40||!eligible(s)||hash01(X,Z,seed^63101)>.35)continue;
  bool minimum=true;for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)if(dx||dz)minimum&=s.height<ground[(z+dz)*N+x+dx].height;
  if(!minimum)continue;
  bool overlap=false;for(auto const&lake:lakes){double ax=lake.at("center").at(0),az=lake.at("center").at(1);overlap|=std::hypot(X-ax,Z-az)<192;}if(overlap)continue;
  constexpr int Side=33,Step=4;std::array<Ground,Side*Side> fine;int rim=320;
  for(int zz=0;zz<Side;++zz)for(int xx=0;xx<Side;++xx){auto q=sample(X+(xx-16)*Step,Z+(zz-16)*Step);fine[zz*Side+xx]=q;if(xx==0||zz==0||xx==Side-1||zz==Side-1)rim=std::min(rim,q.height);}
  int level=std::min(s.height+4,rim-1);if(level<s.height+2)continue;
  std::array<bool,Side*Side> wet{};std::vector<int> queue{16*Side+16};wet[queue[0]]=true;bool open=false;
  for(size_t k=0;k<queue.size();++k){int j=queue[k],xx=j%Side,zz=j/Side;
   if(xx==0||zz==0||xx==Side-1||zz==Side-1||!eligible(fine[j])){open=true;break;}
   for(auto[dx,dz]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){int a=(zz+dz)*Side+xx+dx;if(!wet[a]&&fine[a].height<level){wet[a]=true;queue.push_back(a);}}
  }
  if(open||queue.size()<32)continue;
  J rows=J::array();
  for(int zz=0;zz<Side;++zz)for(int xx=0;xx<Side;){if(!wet[zz*Side+xx]){++xx;continue;}int begin=xx;while(xx+1<Side&&wet[zz*Side+xx+1])++xx;
   for(int dz=0;dz<Step;++dz)rows.push_back({Z+(zz-16)*Step-2+dz,X+(begin-16)*Step-2,X+(xx-16)*Step+1});++xx;
  }
  lakes.push_back({{"level",level},{"center",{X,Z}},{"rows",rows},{"frozen",s.temperature<.42}});sink[i]=level;
 }
 std::array<double,Total> distance;distance.fill(INFINITY);
 std::array<int,Total> parent,root;parent.fill(-1);root.fill(-1);
 using Entry=std::pair<double,int>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> heap;
 for(int i=0;i<Total;++i)if(sink[i]>=0){distance[i]=0;root[i]=i;heap.push({0,i});}
 std::vector<int> order;
 while(!heap.empty()){
  auto[cost,i]=heap.top();heap.pop();if(cost!=distance[i])continue;order.push_back(i);int x=i%N,z=i/N;
  for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){if((!dx&&!dz)||x+dx<0||x+dx>=N||z+dz<0||z+dz>=N)continue;
   int k=(z+dz)*N+x+dx;if(!eligible(ground[k]))continue;
   double uphill=std::max(0,ground[i].height-ground[k].height),next=cost+Pitch*std::hypot(double(dx),double(dz))+35*uphill*uphill;
   if(next<distance[k]){distance[k]=next;parent[k]=i;root[k]=root[i];heap.push({next,k});}
  }
 }
 std::array<int,Total> flow;flow.fill(1);for(auto i=order.rbegin();i!=order.rend();++i)if(parent[*i]>=0)flow[parent[*i]]+=flow[*i];
 std::vector<int> sources;
 for(int i:order){auto[x,z]=point(i);auto const&s=ground[i];
  if(x<gx*Core+128||x>=(gx+1)*Core-128||z<gz*Core+128||z>=(gz+1)*Core-128||s.height<90||s.height>245||s.humidity<.45||flow[i]<6||distance[i]<400||parent[i]<0)continue;
  if(hash01(x,z,seed^63107)>.35)continue;sources.push_back(i);
 }
 std::sort(sources.begin(),sources.end(),[&](int a,int b){double A=ground[a].height*std::pow(flow[a],.18),B=ground[b].height*std::pow(flow[b],.18);return A==B?a<b:A>B;});
 std::vector<std::pair<int,int>> acceptedSources;std::set<int> occupied;
 for(int source:sources){if(acceptedSources.size()>=5)break;auto[SX,SZ]=point(source);bool close=false;
  for(auto[x,z]:acceptedSources)close|=std::hypot(double(SX-x),double(SZ-z))<400;if(close)continue;
  std::vector<int> path;int end=source;while(parent[end]>=0&&path.size()<96&&!occupied.contains(end)){path.push_back(end);end=parent[end];}
  if(path.size()<8||path.size()>=96||occupied.contains(end)||sink[end]<31)continue;
  path.push_back(end);double receiver=sink[end],water=ground[source].height-1,age=0;J route=J::array();bool bad=false;
  bool main=distance[source]>1200&&flow[source]>=12;double width=main?8.:3.;
  std::array<double,5> previous{};bool first=true;
  for(size_t k=0;k+1<path.size()&&!bad;++k){auto[ax,az]=point(path[k]);auto[bx,bz]=point(path[k+1]);int steps=int(std::ceil(std::hypot(double(bx-ax),double(bz-az))/24));
   for(int t=0;t<=steps;++t){if(k&&t==0)continue;double u=double(t)/steps,X=mix(u,ax,bx),Z=mix(u,az,bz);auto s=sample(roundJS(X),roundJS(Z));
    double radius=.85+(width-.85)*smooth(0.,192.,age),bank=std::min(sample(roundJS(X)+roundJS(radius+3),roundJS(Z)).height,sample(roundJS(X)-roundJS(radius+3),roundJS(Z)).height);
    water=std::max(receiver,std::min({water,double(s.height-1),bank-1}));if(!eligible(s)||s.height-water>12||water-s.height>2){bad=true;break;}
    if(!first){double length=std::hypot(X-previous[0],Z-previous[1]);route.push_back({previous[0],previous[1],X,Z,previous[2],water,previous[3],radius,previous[4],age,main?1.:0.});age+=length;}
    previous={X,Z,water,radius,age};first=false;
   }
  }
  if(bad||route.empty()||previous[2]-receiver>2)continue;
  for(auto const&s:route)segments.push_back(s);for(int i:path)occupied.insert(i);acceptedSources.push_back({SX,SZ});
 }
 return {{"region",{gx,gz}},{"lakes",lakes},{"segments",segments},{"sources",acceptedSources},{"samples",Total}};
}
}
