void connectedCliffRelief(Surface&s,double&height,int wx,int wz){
 auto const&c=parameters.cliffs;
 cliff_transition::Displacement displacement;
 struct Alcove {double low,high,roof;};std::array<Alcove,25> alcoves{};size_t count=0;
 double reach=std::hypot(c.front,std::max(16.,c.halfLength-14.)+136.)+48.;
 int halo=std::max(1,int(std::ceil((reach-c.grid*.5)/c.grid)));
 int gx=int(std::floor(wx/c.grid)),gz=int(std::floor(wz/c.grid));
 for(int iz=gz-halo;iz<=gz+halo;++iz)for(int ix=gx-halo;ix<=gx+halo;++ix){
  if(std::abs(wx-(ix+.5)*c.grid)>reach||std::abs(wz-(iz+.5)*c.grid)>reach)continue;
  auto a=patch(ix,iz);if(!a.valid)continue;
  double dx=wx-a.x,dz=wz-a.z,w=-dx*a.nz+dz*a.nx;
  bool curved=parameters.cliffRevision>=4;
  double u=dx*a.nx+dz*a.nz-(curved?0.:1.3*std::sin(w/13+a.h));
  auto shape=cliff_transition::footprint(c.halfLength,c.front,a.drop);
  double offset=curved?cliff_transition::traceOffset(a.trace,w):0.;
  auto part=parameters.cliffRevision>=5?cliff_transition::bankSample(shape,u,w,a.warp,offset):
   curved?cliff_transition::curvedSample(shape,u,w,a.warp,offset):cliff_transition::sample(shape,u,w,a.warp);
  if(part.weight<=0)continue;
  displacement.add(part,a.nx,a.nz);
  u-=offset;
  if(!C.contains("alcoves"))continue;
  auto const&q=C.at("alcoves");
  if(!q.value("enabled",false)||a.h<num(q,"minHeight")||hash01(ix,iz,terrainSeed^607)>=num(q,"chance"))continue;
  double width=num(q,"halfWidth")*(.85+.3*hash01(ix,iz,terrainSeed^609)),depth=num(q,"depth");
  double cut=1-(w*w)/(width*width)-((u-2)*(u-2))/(depth*depth);
  if(cut>0&&count<alcoves.size()){
   double mid=a.h-num(q,"midDepth"),span=num(q,"halfHeight")*std::sqrt(cut);
   alcoves[count++]={mid-span,mid+span,num(q,"roof")};
  }
 }
 if(displacement.weight>0){
  // Blend horizontal remaps and sample the hillside once. Summing independent
  // height deltas made overlapping scarps overshoot, then hit a flat clamp.
  double scale=displacement.scale();
  height=uncliff(wx+displacement.x*scale,wz+displacement.z*scale);
 }
 for(size_t i=0;i<count;++i){
  auto const&a=alcoves[i];int low=int(std::ceil(a.low)),high=int(std::floor(std::min(a.high,height-a.roof)));
  if(high-low>=2&&low>40){s.cliffLow=low;s.cliffHigh=high;}
 }
}
