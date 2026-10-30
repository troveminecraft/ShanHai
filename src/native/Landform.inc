// World-coordinate cached 2-D correction. Never samples generated chunks.
// Opposed samples remove the background slope before measuring a narrow crest.
Cache<uint64_t,double> landformRaw{8192},landformNodes{8192};
bool landformEnabled=C.contains("landform")&&C.at("landform").value("enabled",false);
double landformSample(int x,int z){return landformRaw.get(key2(x,z),[&]{return vcont(x*8.,z*8.);});}
double landformNode(int x,int z){return landformNodes.get(key2(x,z),[&]{
 auto const&q=C.at("landform");double h=landformSample(x,z);
 // This factor is zero below the unchanged lowland threshold. Neighbour
 // crests cannot contribute there, so avoid sixteen unused density samples.
 if(h<=52)return 0.;
 std::array<double,8> averages{};double lo=h,hi=h;int i=0;
 for(int r:{1,2})for(auto [dx,dz]:{std::pair{1,0},std::pair{0,1},std::pair{1,1},std::pair{1,-1}}){
  double a=landformSample(x+dx*r,z+dz*r),b=landformSample(x-dx*r,z-dz*r);
  lo=std::min({lo,a,b});hi=std::max({hi,a,b});averages[i++]=(a+b)*.5;
 }
 std::sort(averages.begin(),averages.end());
 double target=(averages[3]+averages[4])*.5,delta=target-h;
 double mask=smooth(52,82,h)*smooth(9,24,hi-lo)*smooth(1.5,5,std::abs(delta));
 if(mask==0)return 0.;
 // Broad mountain maxima have little local curvature; no absolute height cap.
 double cap=delta<0?num(q,"crestLimit"):num(q,"footLimit");
 return mask*cap*std::tanh(delta*num(q,"strength")/cap);
});}
double landformCorrection(double x,double z){
 if(!landformEnabled)return 0;
 double gx=x/8,gz=z/8;int ix=int(std::floor(gx)),iz=int(std::floor(gz));
 double u=smooth(0,1,gx-ix),v=smooth(0,1,gz-iz);
 // Coarse drainage surveys often land exactly on this lattice. Preserve the
 // original interpolation arithmetic, but do not evaluate zero-weight nodes.
 double a=landformNode(ix,iz),b=u==0?0:landformNode(ix+1,iz);
 double c=v==0?0:landformNode(ix,iz+1),d=u==0||v==0?0:landformNode(ix+1,iz+1);
 return (1-u)*(1-v)*a+u*(1-v)*b+(1-u)*v*c+u*v*d;
}
