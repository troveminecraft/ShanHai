// Agent A forest grammar. Included inside Engine; deterministic per site id.
std::shared_ptr<Giant> giant(Site const& site) {
 return giantCache.get(site.id,[&]()->std::shared_ptr<Giant> {
  auto const& q=C.at("giantForest");auto s=field.surface(site.x,site.z,false);
  auto r=[&](int k){return rand(site.id,k);};
  double weight=field.giantWeight(site.x,site.z,s);
  int height=std::min(int(num(q,"minHeight"))+int(r(3)*(num(q,"maxHeight")-num(q,"minHeight")+1)),Height-68-s.height);
  if(r(4)>=num(q,"chance")*weight||s.cliffLow!=-32768||height<num(q,"minHeight")||s.height<=36||blocked(site.x,site.z))return nullptr;
  // Check the whole trunk footprint, not just nine points. Ground range may
  // be large on a hillside; the trunk is rooted separately in every column.
  int radius=int(std::ceil(site.radius)),lo=s.height,hi=s.height;
  for(int dz=-radius;dz<=radius;dz++)for(int dx=-radius;dx<=radius;dx++)if(dx*dx+dz*dz<=site.radius*site.radius+.5){
   auto b=field.surface(site.x+dx,site.z+dz,false);
   if(!treeGroundAllowed(b,site.x+dx,site.z+dz))return nullptr;
   int y=b.height;lo=std::min(lo,y);hi=std::max(hi,y);
  }
  if(hi-lo>num(q,"maxGroundRange"))return nullptr;
  auto a=std::make_shared<Giant>();a->id=site.id;a->x=site.x;a->z=site.z;a->base=s.height;a->h=height;
  a->footLo=lo;a->footHi=hi;a->radius=site.radius;a->angle=r(6)*6.283185307179586;a->lean=.4+r(7)*.8;
  a->shape=int(r(20)*4);a->aspect=.93+r(21)*.12;
  // Tall spire, broad mature crown, wind-shaped crown and sparse layered crown.
  static constexpr double widths[4]={11.,14.5,12.,13.};
  static constexpr double bases[4]={.55,.46,.52,.49};
  static constexpr int layers[4]={8,5,6,5};
  a->crownRadius=widths[a->shape]+r(8)*(a->shape==1?2.5:2.);
  a->crownBase=height*(bases[a->shape]+r(9)*.11);
  if(a->shape==2)a->aspect=.83+r(21)*.1;
  int tiers=layers[a->shape]+int(r(10)*3);double span=height-a->crownBase,falloff=.65+r(11)*.4;
  // Unequal spacing, differing overlap and a consistent off-centre axis give
  // each conifer a silhouette, rather than random holes cut into one cone.
  for(int j=0;j<tiers;j++){
   double t=(j+(j?r(100+j)*.34-.17:0))/tiers;
   double bulge=a->shape==1?1+.16*std::sin(t*3.141592653589793):1;
   double rr=std::min(a->shape==2?14.:17.,a->crownRadius*std::pow(1-t,falloff)*bulge*(.87+r(130+j)*.22));
   double y=a->crownBase+span*t,top=std::min(double(height),y+span*((a->shape==3?.17:.23)+r(160+j)*.10));
   double drift=a->shape==2?2.7*t:2.*std::sin(t*2.4+r(22)*6.283185307179586);
   a->tiers.push_back({y,top,rr,std::cos(a->angle+.8)*drift,std::sin(a->angle+.8)*drift,r(190+j)*6.283185307179586});
  }
  // Selected low limbs are deliberately bare. They sit below the foliage;
  // higher living boughs retain needle sleeves near their tips.
  int bare=2+int(r(23)*3);
  for(int j=0;j<bare;j++)a->limbs.push_back({a->angle+j*2.39996323+(r(230+j)-.5)*.6,std::max(hi-s.height+9.,a->crownBase-14+j*3.2),a->radius+4+r(260+j)*5,-1+r(290+j)*3,true});
  for(int j=0;j<tiers-2;j++)for(int k=0;k<3;k++){
   auto const&t=a->tiers[j];a->limbs.push_back({a->angle+j*1.27+k*2.0943951+(r(320+j*3+k)-.5)*.6,t.base+1,t.radius*(.63+r(360+j*3+k)*.17),2+r(400+j*3+k)*3,false});
  }
  // Conservative bound covers every crown, branch sleeve and root. All
  // feature queries use the same halo, including single-column exclusions.
  a->extent=Halo;return a;
 });
}
std::vector<std::shared_ptr<Giant>> giants(int x,int z,int size) {
 std::vector<std::shared_ptr<Giant>> out;double grid=num(C.at("giantForest"),"grid");
 for(int gz=int(std::floor((z-Halo)/grid));gz<=int(std::floor((z+size+Halo)/grid));gz++)for(int gx=int(std::floor((x-Halo)/grid));gx<=int(std::floor((x+size+Halo)/grid));gx++){
  auto it=sites.find(key2(gx,gz));if(it==sites.end())continue;
  for(auto const&site:it->second){auto a=giant(site);if(a&&a->x+a->extent>=x&&a->z+a->extent>=z&&a->x-a->extent<x+size&&a->z-a->extent<z+size)out.push_back(a);}
 }
 std::sort(out.begin(),out.end(),[](auto&a,auto&b){return a->id<b->id;});return out;
}
void model(Giant& a) {
 if(!a.runs.empty())return;
 std::fill(scratch.begin(),scratch.end(),0);ground.fill(-32768);
 std::array<int,Side*Side> bottom,upper;bottom.fill(Height);upper.fill(-1);
 auto g=[&](int x,int z){int i=(z+Halo)*Side+x+Halo;if(ground[i]==-32768)ground[i]=field.surface(a.x+x,a.z+z,false).height;return ground[i];};
 auto put=[&](double X,double Y,double Z,int color,bool root=false){
  int x=roundJS(X),y=roundJS(Y),z=roundJS(Z);
  if(std::abs(x)>=Halo||std::abs(z)>=Halo)throw std::runtime_error("Giant geometry exceeds query halo");
  if(y<-60||y>Height-65)return;int h=g(x,z);if(root?y<h-2:y<=h+(color==14?12:0))return;
  int col=(z+Halo)*Side+x+Halo;auto&v=scratch[col*Height+y+64];if(color==14&&v&&v!=14)return;
  v=uint8_t(color);bottom[col]=std::min(bottom[col],y+64);upper[col]=std::max(upper[col],y+64);
 };
 auto ball=[&](double x,double y,double z,double r,int color,bool root=false){int n=int(std::ceil(r));for(int dz=-n;dz<=n;dz++)for(int dx=-n;dx<=n;dx++)for(int dy=-n;dy<=n;dy++)if(dx*dx+dz*dz+dy*dy<=r*r+.4)put(x+dx,y+dy,z+dz,color,root);};
 // Below the centre's ground level, extend each footprint column to its own
 // ground. This avoids both floating downhill bases and flattening hills.
 int foot=int(std::ceil(a.radius));
 for(int z=-foot;z<=foot;z++)for(int x=-foot;x<=foot;x++)if(x*x+z*z<=a.radius*a.radius+.5)
  for(int y=g(x,z)-2;y<a.base;y++)put(x,y,z,18,true);
 for(int y=0;y<=a.h;y++){
  double t=double(y)/a.h,r=std::max(1.,a.radius*(1-.22*std::min(t/.6,1.))*(t<=.6?1:1-.92*std::pow((t-.6)/.4,1.5)));
  double cx=std::cos(a.angle)*a.lean*t,cz=std::sin(a.angle)*a.lean*t;int n=int(std::ceil(r));
  for(int z=-n;z<=n;z++)for(int x=-n;x<=n;x++)if(x*x+z*z<=r*r+.5)put(cx+x,a.base+y,cz+z,a.base+y<=g(roundJS(cx+x),roundJS(cz+z))+2?18:17,true);
 }
 // Determine the dry reach before drawing. The last three blocks taper into
 // soil instead of leaving a full-width cut log at the water exclusion edge.
 for(int j=0;j<5;j++){
  double angle=a.angle+j*6.283185307179586/5,len=a.radius+3+rand(a.id,450+j)*2;
  int end=int(std::ceil(len));
  for(int step=int(std::floor(a.radius))+1;step<=end;step++){
   int x=roundJS(std::cos(angle)*step),z=roundJS(std::sin(angle)*step);bool wet=false;
   for(int dz=-2;dz<=2&&!wet;dz++)for(int dx=-2;dx<=2;dx++){
    auto s=field.surface(a.x+x+dx,a.z+z+dz,false);if(s.river>0&&s.water>s.height){wet=true;break;}
   }
   if(wet){end=step-1;break;}
  }
  for(int step=0;step<=end*2;step++){
   double d=step*.5,x=std::cos(angle)*d,z=std::sin(angle)*d;
   double tip=smooth(0,3.,end-d),radius=(d<a.radius?1.7:1.05)*tip;
   double y=g(roundJS(x),roundJS(z))+std::max(0.,3-d*.35)*tip;
   ball(x,y,z,std::max(.4,radius),18,true);
  }
 }
 // Ground contact is per column, including the edge of each rounded root.
 // Cap peripheral roots to one exposed block and fill down into the soil;
 // this prevents a ball centred on the high bank hovering over the next step.
 for(int col=0;col<Side*Side;col++){
  int x=col%Side-Halo,z=col/Side-Halo;
  if(x*x+z*z<=a.radius*a.radius+.5)continue;
  auto data=scratch.data()+col*Height;int high=-1,h=0;
  for(int y=bottom[col];y<=upper[col];y++)if(data[y]==18){high=y;data[y]=0;}
  if(high<0)continue;
  h=g(x,z);high=std::min(high,h+65);
  for(int y=h-1;y<=high-64;y++)put(x,y,z,18,true);
 }
 for(auto const&t:a.tiers)for(int y=int(std::ceil(t.base));y<=t.top;y++){
  double u=(y-t.base)/(t.top-t.base),r=t.radius*std::pow(1-u,a.shape==2?.72:1.05);
  double cx=std::cos(a.angle)*a.lean*y/a.h+t.dx,cz=std::sin(a.angle)*a.lean*y/a.h+t.dz;
  int n=int(std::ceil(r*1.25));
  for(int z=-n;z<=n;z++)for(int x=-n;x<=n;x++){
   double ang=std::atan2(double(z),double(x));
   double edge=r*(1+.11*std::sin((a.shape==1?5:3)*ang+t.phase)+.05*std::sin(7*ang+a.angle));
   double rx=x*std::cos(a.angle)+z*std::sin(a.angle),rz=-x*std::sin(a.angle)+z*std::cos(a.angle);
   if(rx*rx/(a.aspect*a.aspect)+rz*rz*a.aspect*a.aspect<=edge*edge)put(cx+x,a.base+y,cz+z,14);
  }
 }
 for(auto const&b:a.limbs){
  double x0=std::cos(a.angle)*a.lean*b.base/a.h,z0=std::sin(a.angle)*a.lean*b.base/a.h;
  int color=std::abs(std::cos(b.angle))>.7?15:16,steps=int(std::ceil(b.reach*2));
  for(int i=0;i<=steps;i++){
   double t=double(i)/steps,x=x0+std::cos(b.angle)*b.reach*t,z=z0+std::sin(b.angle)*b.reach*t,y=a.base+b.base+b.rise*t-1.6*std::sin(t*3.141592653589793);
   ball(x,y,z,1.65-t*.95,color);
   if(!b.bare&&t>.42)ball(x,y+.7,z,2.8-t*.7,14);
  }
 }
 // Keep the living upper leader covered even when the crown bends sideways.
 // This sleeve applies only to the vertical leader, not the exposed low limbs.
 std::vector<std::array<int,3>> leader;
 for(int col=0;col<Side*Side;col++)for(int y=std::max(bottom[col],a.base+int(std::ceil(a.crownBase))+4+64);y<=upper[col];y++)
  if(scratch[col*Height+y]==17)leader.push_back({col%Side-Halo,y-64,col/Side-Halo});
 for(auto p:leader)for(auto d:std::array<std::array<int,3>,6>{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}})put(p[0]+d[0],p[1]+d[1],p[2]+d[2],14);
 double tipRadius=(a.shape==0?1.15:a.shape==1?2.1:1.5)+rand(a.id,459)*.65;
 ball(std::cos(a.angle)*a.lean,a.base+a.h,std::sin(a.angle)*a.lean,tipRadius,14);
 // Scan only occupied vertical intervals. Never emit or truncate a tree at
 // the current chunk edge: the full model is clipped by Engine::chunk.
 for(int col=0;col<Side*Side;col++)for(int y=bottom[col];y<=upper[col];){
  auto data=scratch.data()+col*Height;int end=y+1;while(end<=upper[col]&&data[end]==data[y])end++;
  if(data[y]){a.voxels+=end-y;a.runs.push_back({int16_t(col%Side-Halo),int16_t(col/Side-Halo),int16_t(y-64),int16_t(end-y),data[y]});}y=end;
 }
 if(a.voxels>60000||a.runs.size()>9000)throw std::runtime_error("Native giant budget exceeded");
}
