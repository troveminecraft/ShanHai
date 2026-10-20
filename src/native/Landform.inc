// World-coordinate cached 2-D correction. Never samples generated chunks.
// Opposed samples remove the background slope before measuring a narrow crest.
Cache<uint64_t,double> landformRaw{8192},landformNodes{8192};
double landformSample(int x,int z){return landformRaw.get(key2(x,z),[&]{return vcont(x*8.,z*8.);});}
double landformNode(int x,int z){return landformNodes.get(key2(x,z),[&]{
 auto const&q=C.at("landform");double h=landformSample(x,z);
 std::array<double,8> averages{};double lo=h,hi=h;int i=0;
 for(int r:{1,2})for(auto [dx,dz]:{std::pair{1,0},std::pair{0,1},std::pair{1,1},std::pair{1,-1}}){
  double a=landformSample(x+dx*r,z+dz*r),b=landformSample(x-dx*r,z-dz*r);
  lo=std::min({lo,a,b});hi=std::max({hi,a,b});averages[i++]=(a+b)*.5;
 }
 std::sort(averages.begin(),averages.end());
 double target=(averages[3]+averages[4])*.5,delta=target-h;
 double mask=smooth(52,82,h)*smooth(9,24,hi-lo)*smooth(1.5,5,std::abs(delta));
 // Broad mountain maxima have little local curvature; no absolute height cap.
 double cap=delta<0?num(q,"crestLimit"):num(q,"footLimit");
 return mask*cap*std::tanh(delta*num(q,"strength")/cap);
});}
double landformCorrection(double x,double z){
 if(!C.contains("landform")||!C.at("landform").value("enabled",false))return 0;
 double gx=x/8,gz=z/8;int ix=int(std::floor(gx)),iz=int(std::floor(gz));
 double u=smooth(0,1,gx-ix),v=smooth(0,1,gz-iz);
 return (1-u)*(1-v)*landformNode(ix,iz)+u*(1-v)*landformNode(ix+1,iz)+(1-u)*v*landformNode(ix,iz+1)+u*v*landformNode(ix+1,iz+1);
}
