// Secondary channels split and rejoin an existing gentle reach. They reuse
// its exact junction levels, and never request another drainage owner.
template<class Sample> void addRiverBranches(J&plan,int gx,int gz,uint32_t seed,Sample sample,J const&lakes){
 auto&segments=plan["segments"];auto original=segments.size();J branches=J::array();int attempts=0,surveyed=0;
 std::unordered_map<int,std::vector<std::array<int,2>>> lakeRows;
 for(auto const&lake:lakes)for(auto const&r:lake.at("rows"))lakeRows[int(r[0])].push_back({r[1],r[2]});
 auto lakeAt=[&](int x,int z){auto i=lakeRows.find(z);if(i==lakeRows.end())return false;
  for(auto const&r:i->second)if(x>=r[0]&&x<=r[1])return true;return false;};
 for(auto const&route:plan.at("routes")){
  if(branches.size()>=3||attempts>=12)break;
  int first=route.at("firstSegment"),count=route.at("segmentCount");
  if(count<32||hash01(int(segments[first][0]),int(segments[first][1]),seed^63401)>.65)continue;
  for(int trial=0;trial<3&&attempts<12;++trial){
   ++attempts;int start=first+int(hash01(first,trial,seed^63403)*std::max(1,count-32));
   auto const&s=segments[start];double startRun=s[8],target=100+80*hash01(first,trial,seed^63405);
   int end=start;while(end<first+count&&double(segments[end][9])-startRun<target)++end;
   if(end>=first+count||double(s[6])<1.7||double(s[4])-double(segments[end][5])>6||startRun<160)continue;
   bool bad=false;for(int i=start;i<=end;++i)bad|=(segments[i].size()>11&&double(segments[i][11])>0);
   if(bad)continue;
   std::vector<RiverPoint> center,curve;std::vector<double> water,width,run;
   center.push_back({s[0],s[1]});water.push_back(s[4]);width.push_back(std::min(2.8,double(s[6])*.72));run.push_back(startRun);
   for(int i=start;i<=end;++i){auto const&t=segments[i];center.push_back({t[2],t[3]});water.push_back(t[5]);width.push_back(std::min(2.8,double(t[7])*.72));run.push_back(t[9]);}
   double extent=18+16*hash01(first,trial,seed^63407),sign=hash01(first,trial,seed^63409)<.5?-1.:1.;
   for(size_t i=0;i<center.size();++i){auto direction=regional::direction(center[std::min(i+1,center.size()-1)]-center[i?i-1:i]);
    double t=(run[i]-run.front())/(run.back()-run.front()),wave=std::sin(t*3.141592653589793);
    curve.push_back(center[i]+RiverPoint{-direction.z,direction.x}*(sign*extent*wave*wave));
   }
   curve.front()=center.front();curve.back()=center.back();
   // Keep the complete dry rim. Reject a side channel that would be held up
   // by air, cross a closed lake, cut a deep trench or enter volcanic rock.
   for(size_t i=1;i<curve.size()&&!bad;++i){auto a=curve[i-1],b=curve[i],v=b-a;double len2=v.x*v.x+v.z*v.z;
    if(length(v)>12){bad=true;break;}double reach=1.12*std::max(width[i-1],width[i])+1.5;
    for(int z=int(std::floor(std::min(a.z,b.z)-reach));z<=int(std::ceil(std::max(a.z,b.z)+reach))&&!bad;++z)
    for(int x=int(std::floor(std::min(a.x,b.x)-reach));x<=int(std::ceil(std::max(a.x,b.x)+reach));++x){
     double t=clamp(((x-a.x)*v.x+(z-a.z)*v.z)/std::max(1e-9,len2),0.,1.);
     if(length(RiverPoint{double(x),double(z)}-(a+v*t))>reach)continue;
     // Remain in the parent's existing conservative Core + Halo bound.
     if(x<gx*2048-480||x>(gx+1)*2048+480||z<gz*2048-480||z>(gz+1)*2048+480){bad=true;break;}
     auto g=sample(x,z);++surveyed;double level=mix(t,water[i-1],water[i]);
     if(g.stone>.001||g.volcanic>0||g.closedBasin||lakeAt(x,z)||g.height<std::ceil(level)||g.height>level+12){bad=true;break;}
    }
   }
   if(bad)continue;
   size_t begin=segments.size();double distance=run.front();
   for(size_t i=1;i<curve.size();++i){double next=distance+length(curve[i]-curve[i-1]);
    segments.push_back({curve[i-1].x,curve[i-1].z,curve[i].x,curve[i].z,water[i-1],water[i],width[i-1],width[i],distance,next,0.,0.});distance=next;
   }
   branches.push_back({{"source",{curve.front().x,curve.front().z}},{"join",{curve.back().x,curve.back().z}},
    {"receiver","river"},{"firstSegment",begin},{"segmentCount",segments.size()-begin},{"parentFirst",start},{"parentLast",end},
    {"length",distance-run.front()},{"main",false},{"branch",true}});break;
  }
 }
 for(auto const&branch:branches)plan["routes"].push_back(branch);
 plan["branches"]=std::move(branches);plan["branchDiagnostics"]={{"attempts",attempts},{"surveyColumns",surveyed},{"addedSegments",segments.size()-original}};
}
