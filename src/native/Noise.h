#pragma once
#include "Platform.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
namespace nativegen {
using J=nlohmann::json;
inline int terrainRevision(J const&data){
 auto version=data.value("version",std::string{});constexpr std::string_view prefix="karma-terrain-2.";
 return version.starts_with(prefix)?std::stoi(version.substr(prefix.size())):0;
}

inline double clamp(double v,double a,double b){return std::max(a,std::min(b,v));}
inline double mix(double t,double a,double b){return a+t*(b-a);}
inline double smooth(double a,double b,double v){double t=clamp((v-a)/(b-a),0,1);return t*t*(3-2*t);}
inline int roundJS(double v){return int(std::floor(v+.5));}
inline uint64_t key2(int x,int z){return(uint64_t(uint32_t(x))<<32)|uint32_t(z);}
inline uint32_t seedHash(uint32_t seed,uint32_t k){auto s=seed^((k+1)*0x9e3779b1u);s=(s^(s>>16))*0x21f0aaadu;s=(s^(s>>15))*0x735a2d97u;return s^(s>>15);}
inline double hash01(int x,int z,uint32_t s){uint32_t h=s^(uint32_t(x)*0x27d4eb2du)^(uint32_t(z)*0x165667b1u);h=(h^(h>>15))*0x85ebca6bu;h=(h^(h>>13))*0xc2b2ae35u;return double(h^(h>>16))/4294967296.;}
inline double fade(double t){return t*t*t*(t*(t*6-15)+10);}
struct Perlin {
 std::array<uint8_t,512> p{};
 explicit Perlin(uint32_t seed){for(int i=0;i<256;i++)p[i]=uint8_t(i);for(int i=255;i>0;i--){seed+=0x6d2b79f5u;uint32_t t=(seed^(seed>>15))*(1u|seed);t=(t+((t^(t>>7))*(61u|t)))^t;int j=int(double(t^(t>>14))/4294967296.*(i+1));std::swap(p[i],p[j]);}for(int i=256;i<512;i++)p[i]=p[i&255];}
 double at(double x,double y)const{int X=int(std::floor(x))&255,Y=int(std::floor(y))&255;x-=std::floor(x);y-=std::floor(y);auto g=[](int h,double a,double b){static constexpr int v[8][2]={{1,1},{-1,1},{1,-1},{-1,-1},{1,0},{-1,0},{0,1},{0,-1}};return v[h&7][0]*a+v[h&7][1]*b;};return mix(fade(y),mix(fade(x),g(p[p[X]+Y],x,y),g(p[p[X+1]+Y],x-1,y)),mix(fade(x),g(p[p[X]+Y+1],x,y-1),g(p[p[X+1]+Y+1],x-1,y-1)));}
 double fbm(double x,double z,int oct=5,bool ridge=false)const{double s=0,a=1,n=0,f=1;for(int i=0;i<oct;i++){double v=at(x*f,z*f);if(ridge){v=1-std::abs(v);v*=v;}s+=a*v;n+=a;a*=.5;f*=2;}return s/n;}
};
// Permutations and offsets are frozen at package time, never terrain samples.
// Sampling follows deepslate 0.19.0 (MIT); retain its bundled license.
struct Improved {
 std::array<int,256> p{};double xo,yo,zo;
 explicit Improved(J const& j):xo(j.at("xo")),yo(j.at("yo")),zo(j.at("zo")){for(int i=0;i<256;i++)p[i]=j.at("p")[i].get<int>()&255;}
 int P(int i)const{return p[i&255];}
 static double grad(int h,double x,double y,double z){static constexpr int g[16][3]={{1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},{1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},{0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1},{1,1,0},{0,-1,1},{-1,1,0},{0,-1,-1}};auto v=g[h&15];return v[0]*x+v[1]*y+v[2]*z;}
 double at(double x,double y,double z)const{x+=xo;y+=yo;z+=zo;int X=int(std::floor(x)),Y=int(std::floor(y)),Z=int(std::floor(z));x-=std::floor(x);y-=std::floor(y);z-=std::floor(z);int a=P(P(X)+Y),b=P(P(X+1)+Y),c=P(P(X)+Y+1),d=P(P(X+1)+Y+1);double u=fade(x),v=fade(y),w=fade(z);return mix(w,mix(v,mix(u,grad(P(a+Z),x,y,z),grad(P(b+Z),x-1,y,z)),mix(u,grad(P(c+Z),x,y-1,z),grad(P(d+Z),x-1,y-1,z))),mix(v,mix(u,grad(P(a+Z+1),x,y,z-1),grad(P(b+Z+1),x-1,y,z-1)),mix(u,grad(P(c+Z+1),x,y-1,z-1),grad(P(d+Z+1),x-1,y-1,z-1))));}
};
struct Octaves {
 std::vector<std::unique_ptr<Improved>> levels;std::vector<double> amplitudes;double fi,fv;
 explicit Octaves(J const& j):amplitudes(j.at("amplitudes").get<std::vector<double>>()),fi(j.at("lowestFreqInputFactor")),fv(j.at("lowestFreqValueFactor")){for(auto const& l:j.at("noiseLevels"))levels.push_back(l.is_null()?nullptr:std::make_unique<Improved>(l));}
 static double wrap(double v){return v-std::floor(v/33554432.+.5)*33554432.;}
 double at(double x,double y,double z)const{double s=0,f=fi,v=fv;for(size_t i=0;i<levels.size();i++){if(levels[i])s+=amplitudes[i]*v*levels[i]->at(wrap(x*f),wrap(y*f),wrap(z*f));f*=2;v*=.5;}return s;}
};
inline thread_local uint64_t noiseSamples=0;
struct Normal {Octaves a,b;double factor;explicit Normal(J const& j):a(j.at("first")),b(j.at("second")),factor(j.at("valueFactor")){} double at(double x,double y,double z)const{++noiseSamples;constexpr double k=1.0181268882175227;return(a.at(x,y,z)+b.at(x*k,y*k,z*k))*factor;}};
struct Pos {double x,y,z;bool operator==(Pos const&)const=default;};
inline uint64_t mixHash(uint64_t v){v^=v>>30;v*=0xbf58476d1ce4e5b9ULL;v^=v>>27;v*=0x94d049bb133111ebULL;return v^(v>>31);}
struct PosHash {size_t operator()(Pos const&p)const{auto bits=[](double v){return v==0?uint64_t(0):std::bit_cast<uint64_t>(v);};return mixHash(bits(p.x)^std::rotl(bits(p.y),21)^std::rotl(bits(p.z),42));}};
// Fixed-cap FIFO memoization: no wholesale cache clear or string coordinate keys.
template<class K,class V,class H=std::hash<K>>struct Cache {
 size_t cap,cursor=0;std::unordered_map<K,V,H> m;std::vector<K> order;
 explicit Cache(size_t c):cap(c){m.reserve(c);order.reserve(c);}
 template<class F>V get(K k,F f){auto it=m.find(k);if(it!=m.end())return it->second;V v=f();if(m.size()>=cap){m.erase(order[cursor]);order[cursor]=k;cursor=(cursor+1)%cap;}else order.push_back(k);m.emplace(k,v);return v;}
};
class Density {
 using Fn=std::function<double(double,double,double)>;
 J const& data;bool surface;std::unordered_map<std::string,Fn> named;std::unordered_map<std::string,std::unique_ptr<Normal>> noises;
 Fn memo(Fn f){auto last=std::make_shared<Pos>(Pos{NAN,NAN,NAN});auto value=std::make_shared<double>();return[f,last,value](double x,double y,double z){Pos p{x,y,z};if(!(*last==p)){*value=f(x,y,z);*last=p;}return *value;};}
 Fn spline(J const& s){if(s.is_number()){double v=s;return[v](double,double,double){return v;};}auto co=compile(s.at("coordinate"));struct Pt{double x,d;Fn f;};std::vector<Pt> pts;for(auto const&p:s.at("points"))pts.push_back({p.at("location"),p.at("derivative"),spline(p.at("value"))});bool mode=surface;return[co,pts,mode](double x,double y,double z){double t=co(x,y,z);auto a=&pts.front(),b=&pts.back();if(t<=a->x)return a->f(x,y,z)+(t-a->x)*a->d;if(t>=b->x)return b->f(x,y,z)+(t-b->x)*b->d;for(size_t i=1;i<pts.size();i++)if(t<pts[i].x){a=&pts[i-1];b=&pts[i];break;}double d=b->x-a->x,u=(t-a->x)/d,av=a->f(x,y,z),bv=b->f(x,y,z);if(mode){double u2=u*u,u3=u2*u;return(2*u3-3*u2+1)*av+(u3-2*u2+u)*d*a->d+(-2*u3+3*u2)*bv+(u3-u2)*d*b->d;}return mix(u,av,bv)+u*(1-u)*mix(u,a->d*d-(bv-av),-b->d*d+(bv-av));};}
public:
 Density(J const& d,bool mode):data(d),surface(mode){}
 Normal& noise(std::string const& id){auto it=noises.find(id);if(it==noises.end())it=noises.emplace(id,std::make_unique<Normal>(data.at("noises").at(id))).first;return *it->second;}
 Fn ref(std::string const&id){auto it=named.find(id);if(it!=named.end())return it->second;Fn f;if(surface&&(id=="minecraft:shift_x"||id=="minecraft:shift_z")){auto n=&noise("minecraft:offset");f=id=="minecraft:shift_x"?Fn([n](double x,double,double z){return n->at(x*.25,0,z*.25)*4;}):Fn([n](double x,double,double z){return n->at(z*.25,x*.25,0)*4;});}else f=compile(data.at("functions").at(id));if(surface)f=memo(f);named.emplace(id,f);return f;}
 Fn compile(J const& v){
  if(v.is_number()){double n=v;return[n](double,double,double){return n;};}if(v.is_string())return ref(v.get<std::string>());
  std::string t=v.at("type");if(t.starts_with("minecraft:"))t=t.substr(10);
  if(t=="constant")return compile(v.at("argument"));if(t=="blend_alpha")return compile(1);if(t=="blend_offset"||t=="beardifier")return compile(0);
  if(t=="blend_density")return compile(v.at("argument"));
  if(t=="cache_2d"||t=="cache_once"||t=="cache_all_in_cell")return surface?compile(v.at("argument")):memo(compile(v.at("argument")));
  if(t=="flat_cache"){auto f=compile(v.at("argument"));if(surface)return f;auto cache=std::make_shared<Cache<Pos,double,PosHash>>(16384);return[f,cache](double x,double,double z){Pos p{std::floor(x/4)*4,0,std::floor(z/4)*4};return cache->get(p,[&]{return f(p.x,0,p.z);});};}
  if(t=="interpolated"){auto f=compile(v.at("argument"));double w=data.at("settings").at("noise").at("size_horizontal").get<double>()*4,h=data.at("settings").at("noise").at("size_vertical").get<double>()*4;auto cache=std::make_shared<Cache<Pos,double,PosHash>>(32768);auto last=std::make_shared<Pos>(Pos{NAN,NAN,NAN});auto corners=std::make_shared<std::array<double,8>>();return[f,w,h,cache,last,corners](double x,double y,double z){double bx=std::floor(x/w)*w,by=std::floor(y/h)*h,bz=std::floor(z/w)*w;auto& a=*corners;Pos cell{bx,by,bz};if(!(*last==cell)){int i=0;for(int dz=0;dz<2;dz++)for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){Pos p{bx+dx*w,by+dy*h,bz+dz*w};a[i++]=cache->get(p,[&]{return f(p.x,p.y,p.z);});}*last=cell;}double u=(x-bx)/w,v=(y-by)/h,q=(z-bz)/w;return mix(q,mix(v,mix(u,a[0],a[1]),mix(u,a[2],a[3])),mix(v,mix(u,a[4],a[5]),mix(u,a[6],a[7])));};}
  if(t=="spline")return spline(v.at("spline"));
  if(t=="add"||t=="mul"||t=="min"||t=="max"){auto a=compile(v.at("argument1")),b=compile(v.at("argument2"));if(t=="add")return[a,b](double x,double y,double z){return a(x,y,z)+b(x,y,z);};if(t=="mul")return[a,b](double x,double y,double z){double q=a(x,y,z);return q==0?0:q*b(x,y,z);};if(t=="min")return[a,b](double x,double y,double z){return std::min(a(x,y,z),b(x,y,z));};return[a,b](double x,double y,double z){return std::max(a(x,y,z),b(x,y,z));};}
  if(t=="abs"||t=="square"||t=="cube"||t=="half_negative"||t=="quarter_negative"||t=="squeeze"){auto a=compile(v.at("argument"));if(t=="abs")return[a](double x,double y,double z){return std::abs(a(x,y,z));};if(t=="square")return[a](double x,double y,double z){double q=a(x,y,z);return q*q;};if(t=="cube")return[a](double x,double y,double z){double q=a(x,y,z);return q*q*q;};if(t=="half_negative")return[a](double x,double y,double z){double q=a(x,y,z);return q<0?q*.5:q;};if(t=="quarter_negative")return[a](double x,double y,double z){double q=a(x,y,z);return q<0?q*.25:q;};return[a](double x,double y,double z){double q=clamp(a(x,y,z),-1,1);return q/2-q*q*q/24;};}
  if(t=="clamp"){auto a=compile(v.at("input"));double lo=v.at("min"),hi=v.at("max");return[a,lo,hi](double x,double y,double z){return clamp(a(x,y,z),lo,hi);};}
  if(t=="y_clamped_gradient"){double a=v.at("from_y"),b=v.at("to_y"),lo=v.at("from_value"),hi=v.at("to_value");return[a,b,lo,hi](double,double y,double){return mix(clamp((y-a)/(b-a),0,1),lo,hi);};}
  if(t=="range_choice"){auto a=compile(v.at("input")),b=compile(v.at("when_in_range")),c=compile(v.at("when_out_of_range"));double lo=v.at("min_inclusive"),hi=v.at("max_exclusive");return[a,b,c,lo,hi](double x,double y,double z){double q=a(x,y,z);return(q>=lo&&q<hi?b:c)(x,y,z);};}
  if(t=="noise"||t=="shifted_noise"){auto n=&noise(v.at("noise"));double s=v.at("xz_scale"),sy=surface?0.:v.at("y_scale").get<double>();auto ax=compile(v.value("shift_x",J(0))),ay=compile(v.value("shift_y",J(0))),az=compile(v.value("shift_z",J(0)));bool mode=surface;return[n,s,sy,ax,ay,az,mode](double x,double y,double z){return n->at(x*s+ax(x,y,z),mode?0:y*sy+ay(x,y,z),z*s+az(x,y,z));};}
  if(t=="shift"||t=="shift_a"||t=="shift_b"){auto n=&noise(v.at("argument"));return[n,t](double x,double y,double z){return(t=="shift_a"?n->at(x*.25,0,z*.25):t=="shift_b"?n->at(z*.25,x*.25,0):n->at(x*.25,y*.25,z*.25))*4;};}
  if(t=="weird_scaled_sampler"){auto a=compile(v.at("input"));auto n=&noise(v.at("noise"));bool one=v.at("rarity_value_mapper")=="type_1";return[a,n,one](double x,double y,double z){double q=a(x,y,z),r=one?(q<-.5?.75:q<0?1:q<.5?1.5:2):(q<-.75?.5:q<-.5?.75:q<.5?1:q<.75?2:3);return r*std::abs(n->at(x/r,y/r,z/r));};}
  throw std::runtime_error("Unsupported native density: "+t);
 }
};
}
