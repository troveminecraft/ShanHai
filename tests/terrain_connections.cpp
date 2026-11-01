#undef NDEBUG
#include "native/Field.h"
#include <cassert>
#include <fstream>
#include <iostream>

namespace nativegen {
struct TerrainConnectionRegression {
 static J curved(J data){
  data["config"]["cliffRevision"]=4;Field f(data);int checked=0,bent=0;double largest=0;J sites=J::array();
  for(int gz=96;gz<160&&checked<48;++gz)for(int gx=96;gx<160&&checked<48;++gx){
   auto a=f.patch(gx,gz);if(!a.valid)continue;double deviation=0;
   double left=cliff_transition::traceOffset(a.trace,-96),right=cliff_transition::traceOffset(a.trace,96);
   for(int w=-96;w<=96;++w){double line=left+(right-left)*(w+96)/192.;
    deviation=std::max(deviation,std::abs(cliff_transition::traceOffset(a.trace,w)-line));
   }
   for(auto offset:a.trace)assert(std::isfinite(offset)&&std::abs(offset)<=56.);
   auto shape=cliff_transition::footprint(64,256,a.drop);
   for(int w:{-120,-48,0,48,120}){double offset=cliff_transition::traceOffset(a.trace,w),last=-257;
    for(int i=0;i<5140;++i){double u=-257+i*.1,mapped=u+cliff_transition::curvedSample(shape,u,w,a.warp,offset).displacement;
     if(i)assert(mapped>last);last=mapped;
    }
   }
   int drop=faceDrop(f,a);assert(drop<=320);bent+=deviation>=6;largest=std::max(largest,deviation);++checked;
   sites.push_back({{"x",a.x-32768},{"z",a.z-32768},{"curveDeviation",deviation},{"centerFaceDrop",drop}});
  }
  assert(checked==48&&bent>=36);return {{"checked",checked},{"curvedFaces",bent},{"maximumDeviation",largest},{"passed",true},{"sites",sites}};
 }
 static J waterConnection(J data){
  assert(data.at("seedManifest").at("terrainSeed")=="35");
  data["config"]["cliffRevision"]=3;data["config"]["volcanoRevision"]=5;data["config"]["waterContainment"]=1;
  J next=data;next["config"]["waterContainment"]=2;Field before(data),after(next);
  auto a=before.surface(2560+32768,4785+32768,false),b=before.surface(2560+32768,4786+32768,false);
  auto c=after.surface(2560+32768,4785+32768,false),d=after.surface(2560+32768,4786+32768,false);
  assert(a.water>a.height&&b.water>b.height&&a.height>=b.water);
  assert(c.water>c.height&&d.water>d.height&&c.height<d.water&&d.height<c.water);
  assert(a.water==c.water&&b.water==d.water&&c.height==a.height-1&&b.height==d.height);
  return {{"old",{{a.height,a.water},{b.height,b.water}}},{"new",{{c.height,c.water},{d.height,d.water}}},{"passed",true}};
 }
 static double percentile(std::vector<double> values,double fraction){
  assert(!values.empty());std::sort(values.begin(),values.end());return values[size_t((values.size()-1)*fraction)];
 }
 static void geometry(){
  using namespace cliff_transition;
  for(double drop:{100.,200.,300.})for(double amplitude:{20.,40.,80.,144.}){
   auto f=footprint(64,256,drop);
   assert(f.end-f.core>=72&&f.end<=186);
   for(double w:{0.,f.core,(f.core+f.end)*.5,f.end}){
    double last=-257.;
    for(int i=0;i<=10280;++i){double u=-257.+i*.05;auto s=sample(f,u,w,amplitude);double mapped=u+s.displacement;
     if(i)assert(mapped>last);last=mapped;
     assert(s.weight>=0&&s.weight<=1&&std::abs(s.displacement)<=amplitude);
    }
   }
   assert(sample(f,257,0,amplitude).displacement==0&&sample(f,4,f.end,amplitude).displacement==0);
   assert(std::abs(sample(f,4,f.end-.001,amplitude).displacement)<1.e-6);
   Displacement combined;combined.add(sample(f,4,0,amplitude),1,0);combined.add(sample(f,4,0,amplitude),0,1);
   assert(std::hypot(combined.x,combined.z)*combined.scale()<=amplitude);
  }
 }
 static int faceDrop(Field&f,Cliff const&a){
  int X=roundJS(a.x),Z=roundJS(a.z),drop=0;
  for(int z=Z-4;z<=Z+4;++z)for(int x=X-4;x<=X+4;++x){auto s=f.compute(x,z,false);
   for(auto [dx,dz]:{std::pair{2,0},std::pair{0,2}})drop=std::max(drop,std::abs(s.height-f.compute(x+dx,z+dz,false).height));
  }return drop;
 }
 static J cliffs(J data){
  data["config"]["cliffRevision"]=2;data["config"]["volcanoRevision"]=4;J next=data;next["config"]["cliffRevision"]=3;
  J reference=data;reference["config"]["cliffs"]["chance"]=0.;
  Field old(data),current(next),background(reference);std::vector<double> oldEnds,newEnds,oldFaces,newFaces;J examples=J::array();int checked=0,oldTall=0,newTall=0;
  for(int gz=96;gz<160&&checked<48;++gz)for(int gx=96;gx<160&&checked<48;++gx){
   auto a=old.patch(gx,gz);if(!a.valid)continue;
   auto center=old.compute(roundJS(a.x),roundJS(a.z),false);
   if(center.volcanic>0||center.basinWeight>.01||center.riftWeight>.01||center.stone>0)continue;
   int before=faceDrop(old,a),after=faceDrop(current,a);
   oldFaces.push_back(before);newFaces.push_back(after);oldTall+=before>=100&&before<=300;newTall+=after>=100&&after<=300;
   double oldEdge=0,newEdge=0;
   for(int U:{-4,4}){double lastOld=0,lastNew=0;bool have=false;
    for(int W=-200;W<=200;++W){
     if(std::abs(W)<38){have=false;continue;}
     int x=roundJS(a.x-a.nz*W+a.nx*U),z=roundJS(a.z+a.nx*W+a.nz*U);
     double base=background.compute(x,z,false).height,vOld=old.compute(x,z,false).height-base,vNew=current.compute(x,z,false).height-base;
     if(have){oldEdge=std::max(oldEdge,std::abs(vOld-lastOld));newEdge=std::max(newEdge,std::abs(vNew-lastNew));}
     lastOld=vOld;lastNew=vNew;have=true;
    }
   }
   oldEnds.push_back(oldEdge);newEnds.push_back(newEdge);
   examples.push_back({{"x",a.x-32768},{"z",a.z-32768},{"oldFace",before},{"newFace",after},{"oldEndStep",oldEdge},{"newEndStep",newEdge}});++checked;
  }
  assert(checked>=12);
  double oldMedian=percentile(oldEnds,.5),newMedian=percentile(newEnds,.5);
  return {{"checked",checked},{"oldTallFaces",oldTall},{"newTallFaces",newTall},
   {"oldMedianEndStep",oldMedian},{"newMedianEndStep",newMedian},
   {"oldP90EndStep",percentile(oldEnds,.9)},{"newP90EndStep",percentile(newEnds,.9)},
   {"oldMedianFace",percentile(oldFaces,.5)},{"newMedianFace",percentile(newFaces,.5)},
   {"passed",newMedian<=oldMedian*.70&&newTall>=oldTall*.85},{"examples",examples}};
 }
 static J volcanoes(J data){
  data["config"]["cliffRevision"]=2;data["config"]["volcanoRevision"]=4;J next=data;next["config"]["volcanoRevision"]=5;
  Field old(data),current(next);std::vector<double> oldSlopes,newSlopes,oldSamples,newSamples;J examples=J::array();int checked=0;
  for(int gz=-2;gz<=2;++gz)for(int gx=-2;gx<=2;++gx){
   auto a=old.volcanoRegion(gx,gz),b=current.volcanoRegion(gx,gz);assert(a.valid==b.valid);if(!a.valid)continue;
   assert(a.cone.crater==b.cone.crater&&a.lavaTop==b.lavaTop&&a.cone.height==b.cone.height);
   int X=roundJS(a.cone.x)+32768,Z=roundJS(a.cone.z)+32768;auto oa=old.compute(X,Z,false),na=current.compute(X,Z,false);
   assert(oa.height==na.height&&oa.lava==na.lava&&oa.volcanoMargins==na.volcanoMargins);
   double steepOld=0,steepNew=0;
   for(int direction=0;direction<16;++direction){double angle=direction*6.283185307179586/16,nx=std::cos(angle),nz=std::sin(angle);
    int start=int(std::ceil(old.craterRadius(a.cone,angle)))+4;double lastOld=0,lastNew=0;bool have=false;
    for(int radius=start;radius<start+100;radius+=4){
     int x=X+roundJS(nx*radius),z=Z+roundJS(nz*radius);auto o=old.compute(x,z,false),n=current.compute(x,z,false);
     assert(n.height>=-61&&n.height<512);
     if(have){double os=std::max(0.,(lastOld-o.height)/4.),ns=std::max(0.,(lastNew-n.height)/4.);
      steepOld=std::max(steepOld,os);steepNew=std::max(steepNew,ns);oldSamples.push_back(os);newSamples.push_back(ns);}
     lastOld=o.height;lastNew=n.height;have=true;
    }
   }
   oldSlopes.push_back(steepOld);newSlopes.push_back(steepNew);++checked;
   examples.push_back({{"x",a.cone.x},{"z",a.cone.z},{"oldSteepestUpperFlank",steepOld},{"newSteepestUpperFlank",steepNew}});
  }
  assert(checked>=5);double oldMedian=percentile(oldSlopes,.5),newMedian=percentile(newSlopes,.5);
  return {{"checked",checked},{"oldMedianUpperSlope",oldMedian},{"newMedianUpperSlope",newMedian},
   {"oldP90UpperSlope",percentile(oldSamples,.9)},{"newP90UpperSlope",percentile(newSamples,.9)},
   {"passed",newMedian<=oldMedian&&percentile(newSamples,.9)<=percentile(oldSamples,.9)*.8},
   {"cratersAndMagmaCentersUnchanged",true},{"examples",examples}};
 }
 static J describe(J const& data) {
  Field f(data);J sites=J::array();int X=1338+32768,Z=-1683+32768;
  int gx=int(std::floor(X/256.)),gz=int(std::floor(Z/256.));
  for(int iz=gz-3;iz<=gz+3;++iz)for(int ix=gx-3;ix<=gx+3;++ix){
   auto a=f.patch(ix,iz);if(!a.valid)continue;
   double dx=X-a.x,dz=Z-a.z,w=-dx*a.nz+dz*a.nx,u=dx*a.nx+dz*a.nz-1.3*std::sin(w/13+a.h);
   J row={{"gx",ix},{"gz",iz},{"x",a.x-32768},{"z",a.z-32768},{"normal",{a.nx,a.nz}},{"warp",a.warp},{"h",a.h},{"u",u},{"w",w}};
   row["drop"]=f.uncliff(a.x+a.nx*a.warp,a.z+a.nz*a.warp)-f.uncliff(a.x-a.nx*a.warp,a.z-a.nz*a.warp);
   J ends=J::array();
   for(int W=-220;W<=220;W+=2){
    int x=roundJS(a.x-a.nz*W+a.nx*4),z=roundJS(a.z+a.nx*W+a.nz*4);
    auto s=f.compute(x,z,false);ends.push_back({x-32768,z-32768,W,s.height,f.uncliff(x,z)});
   }
   row["endProfile"]=ends;sites.push_back(row);
  }
  auto s=f.compute(X,Z,false);
  return {{"point",{X-32768,Z-32768}},{"height",s.height},{"uncliff",f.uncliff(X,Z)},{"sites",sites}};
 }
};
}

int main(int argc,char**argv){try{
 if(argc!=3&&argc!=4)return 2;nativegen::J data;std::ifstream(argv[1])>>data;
 using T=nativegen::TerrainConnectionRegression;
 if(argc==4&&std::string(argv[3])=="curved"){auto report=T::curved(data);std::ofstream(argv[2])<<report.dump(2);report.erase("sites");std::cout<<report<<std::endl;return 0;}
 if(argc==4&&std::string(argv[3])=="water-connection"){auto report=T::waterConnection(data);std::ofstream(argv[2])<<report.dump(2);std::cout<<report<<std::endl;return 0;}
 if(argc==4){T::geometry();nativegen::J report={{"cliffs",T::cliffs(data)},{"volcanoes",T::volcanoes(data)}};
  std::ofstream(argv[2])<<report.dump(2);report["cliffs"].erase("examples");report["volcanoes"].erase("examples");std::cout<<report<<std::endl;
  return report["cliffs"]["passed"]==true&&report["volcanoes"]["passed"]==true?0:1;
 }
 auto report=T::describe(data);std::ofstream(argv[2])<<report.dump(2);report.erase("sites");std::cout<<report<<std::endl;
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
