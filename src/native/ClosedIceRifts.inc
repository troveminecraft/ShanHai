// Analytic column intervals form a narrow opening with inward-slanting walls.
// No density noise, template placement, adjacent chunk reads or tick scans.
struct ClosedRiftColumn {bool valid=false;int top=0,floor=0;double distance=0,opening=0,undercut=0;};
std::array<ClosedRiftColumn,256> closedRiftColumns{};
Cache<uint64_t,double> closedRiftInteriorNodes{8192};
Cache<uint64_t,double> closedRiftSafetyNodes{8192};
double closedRiftSafetyNode(int gx,int gz){return closedRiftSafetyNodes.get(key2(gx,gz),[&]{
 auto s=field.surface(gx*32+32768,gz*32+32768,false);
 if(s.kind||s.river||s.lake||s.fluvialType||s.wetland>.01||s.water>=s.height||s.scarp>.01||s.cliffLow!=-32768)return 0.;
 return std::min(s.glacier,s.superGlacier);
});}
double closedRiftInteriorNode(int gx,int gz){return closedRiftInteriorNodes.get(key2(gx,gz),[&]{
 double weight=1.;
 // Reuse common 32-block nodes for the collar instead of five different
 // off-grid terrain samples per node. The collar is at least as conservative.
 for(auto [dx,dz]:{std::pair{0,0},std::pair{-1,0},std::pair{1,0},std::pair{0,-1},std::pair{0,1}})
  weight=std::min(weight,closedRiftSafetyNode(gx+dx,gz+dz));
 return smooth(.80,.98,weight);
});}
double closedRiftInterior(int x,int z){
 int gx=int(std::floor(x/32.)),gz=int(std::floor(z/32.));double tx=(x-gx*32)/32.,tz=(z-gz*32)/32.;
 double a=closedRiftInteriorNode(gx,gz)*(1-tx)+closedRiftInteriorNode(gx+1,gz)*tx;
 double b=closedRiftInteriorNode(gx,gz+1)*(1-tx)+closedRiftInteriorNode(gx+1,gz+1)*tx;
 auto s=field.surface(x+32768,z+32768,false);
 return (a*(1-tz)+b*tz)*smooth(.80,.98,std::min(s.glacier,s.superGlacier));
}
ClosedRiftColumn closedRiftGeometry(int x,int z,int top,SnowRiftSample r,double n,double m){
 if(r.cut<=0)return {};
 // Reject columns outside even the maximum lining radius before looking up
 // the terrain collar. The broad influence includes a conservative margin.
 if(bedrockIceRifts&&r.distance>r.width+2.5+1.5*std::abs(n-m)+2)return {};
 double activity=r.end*closedRiftInterior(x,z);if(activity<.035)return {};
 double opening=r.width*std::sqrt(activity),undercut=(2.5+1.5*std::abs(n-m))*activity;
 if(r.distance>opening+undercut+2)return {};
 int floor=bedrockIceRifts?int(std::round(top+(-59-top)*activity)):std::max(-48,top-int(std::round((90+12*n+6*m)*activity)));
 if(top-floor<5)return {};
 return {true,top,floor,r.distance,opening,undercut};
}
void planClosedRift(int i,int x,int z,int top,Surface const&,SnowRiftSample r,double n,double m){
 auto p=closedRiftGeometry(x,z,top,r,n,m);if(p.valid)closedRiftColumns[i]=p;
}
KARMA_NOINLINE void cutClosedRifts(std::vector<uint16_t>&raw){
 if(!closedIceRifts)return;
 for(int i=0;i<256;++i){auto const&p=closedRiftColumns[i];if(!p.valid)continue;auto col=raw.data()+i*Height;
  int floor=p.floor;
  // Dry cave air may join a deep rift. Preserve liquids and a solid seal above
  // them; the old early stop at any air cell incorrectly raised the whole floor.
  if(!bedrockIceRifts)for(int y=p.top;y>=floor-3;--y){int b=col[y+64];if(b==1||b==8||b==43||b==126||b==145){floor=std::max(floor,y+3);break;}}
  else floor=std::max(-59,floor);
  int hi=p.top;
  if(p.distance>p.opening){
   double side=clamp((p.distance-p.opening)/std::max(.001,p.undercut),0.,1.);
   hi=std::min(p.top-3,floor+int(std::floor((p.top-floor)*(1-side))));
  }
  // Two-block lateral lining follows the same analytic wall, including the
  // newly exposed substrate. It also closes old dry cave mouths at the sides.
  double outer=clamp((p.distance-p.opening-2)/std::max(.001,p.undercut),0.,1.);
  int liningTop=std::min(p.top,floor+int(std::floor((p.top-floor)*(1-outer))));
  if(liningTop<=floor)continue;
  int bottom=bedrockIceRifts?std::max(-60,floor-2):floor-2;
  for(int y=bottom;y<=liningTop;++y)col[y+64]=uint16_t((y+384)%18<3?144:143);
  if(hi>floor)std::fill(col+floor+65,col+hi+65,uint16_t(0));
  for(int y=bottom;y<=floor;++y)col[y+64]=uint16_t(y==floor?9:144);
  // Attached ice lining on the underside. The ledge keeps at least 3 blocks
  // of roof and connects sideways to untouched banks rather than floating.
  if(hi<p.top)for(int y=hi+1;y<=std::min(p.top,hi+3);++y)col[y+64]=uint16_t(y==hi+1?9:143);
 }
}
