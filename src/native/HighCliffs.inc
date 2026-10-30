Cliff highCliffPatch(int gx,int gz){
 Cliff result;auto const&q=parameters.cliffs;auto const&stone=C.at("stoneForest");
 if(hash01(gx,gz,terrainSeed^int(q.seed))>=q.chance)return result;
 double cx=(gx+.5)*q.grid,cz=(gz+.5)*q.grid;
 if(largeStoneForestEnabled()&&std::hypot(cx-32768-num(stone,"centerX"),cz-32768-num(stone,"centerZ"))<=num(stone,"outerRadius")+num(stone,"edgeWarp")+q.grid)return result;
 double best=-INFINITY,target=120.+155.*hash01(gx,gz,terrainSeed^611);
 for(int dz:{-32,0,32})for(int dx:{-32,0,32}){
  double x=cx+dx,z=cz+dz,h=uncliff(x,z);if(h<q.minHeight||h>q.maxHeight)continue;
  double ex=(uncliff(x+8,z)-uncliff(x-8,z))/16,ez=(uncliff(x,z+8)-uncliff(x,z-8))/16,sl=std::hypot(ex,ez);
  if(sl<q.minSlope||sl>q.maxSlope)continue;
  Cliff a{true,x,z,h,sl,ex/sl,ez/sl,h,std::min(144.,target/(2*sl))};
  double low=uncliff(x-a.nx*a.warp,z-a.nz*a.warp),high=uncliff(x+a.nx*a.warp,z+a.nz*a.warp);
  if(high-low<100||high-low>300||low<seaLevelValue+4||high>470)continue;
  // Prefer a measured concave hillside when available. Selecting only the
  // steepest crest puts almost every fall on an isolated ridge spring.
  double valley=(uncliff(x-a.nz*32,z+a.nx*32)+uncliff(x+a.nz*32,z-a.nx*32)-2*h)/16;
  double score=sl+clamp(valley,-2.,2.);if(score<=best)continue;
  best=score;a.crest=high;result=a;
 }return result;
}
