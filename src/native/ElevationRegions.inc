bool elevatedLandscape=C.value("landscapeRevision",0)>=1&&generatedMaxYExclusive==512;
struct ElevationNode {double ground,top;};
Cache<uint64_t,ElevationNode> elevationNodes{2048};
double elevationPeak(double wx,double wz){
 return mountainTierPeak(regionalNoise.at((wx-32768)/4096.+241.,(wz-32768)/4096.-193.));
}
ElevationNode elevationNode(int gx,int gz){return elevationNodes.get(key2(gx,gz),[&]{
 double x=gx*256.,z=gz*256.,ground=regionalHeight(x,z,vcont(x,z));
 return ElevationNode{ground,raisedMountainHeight(ground,int(elevationPeak(x,z)))};
});}
double tieredMountainHeight(double wx,double wz,double local){
 int gx=int(std::floor(wx/256.)),gz=int(std::floor(wz/256.));
 double u=smooth(0,1,wx/256.-gx),v=smooth(0,1,wz/256.-gz);
 auto a=elevationNode(gx,gz),b=elevationNode(gx+1,gz),c=elevationNode(gx,gz+1),d=elevationNode(gx+1,gz+1);
 auto blend=[&](double A,double B,double C,double D){return (1-u)*(1-v)*A+u*(1-v)*B+(1-u)*v*C+u*v*D;};
 return softenedMountain(local,blend(a.ground,b.ground,c.ground,d.ground),blend(a.top,b.top,c.top,d.top),elevationPeak(wx,wz));
}
double landscapeAltitude(double height)const{return elevatedLandscape?height-raisedLandOffset(height):height;}
