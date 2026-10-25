// World-coordinate patches on supported, exposed ground. Footprint mode emits
// conservative plant candidates for native structure clearance; full generation
// requires an actual decorated lush cave below an azalea.
void surfaceFlora(int x0,int z0,std::vector<uint16_t>& raw,std::array<Surface,256>const& surfaces,bool footprint=false){
 if(C.value("surfaceMoss",0)!=2)return;
 for(int z=0;z<16;++z)for(int x=0;x<16;++x){
  auto const&s=surfaces[z*16+x];int X=x0+x,Z=z0+z,h=s.height;
  if(h<-39||h>=Height-66||s.water>=h||s.lava>=h||s.iceHabitat>.05||s.desert>.1||s.badlands||s.mushroom||s.volcanic>.1||nativeReserved(X,Z))continue;
  auto col=raw.data()+(z*16+x)*Height;int ground=col[h+64];
  if((ground!=3&&ground!=4&&ground!=32&&ground!=33&&ground!=80)||col[h+65]!=0)continue;
  double plant=hash01(X,Z,terrainSeed^63817);
  bool lush=false;
  if(plant<.018){
   if(footprint)lush=true;
   else for(int y=std::max(-39,h-160);y<h-12;++y)if(col[y+64]==80&&(col[y+65]==0||col[y+65]==120||col[y+65]==121||col[y+65]==122)){lush=true;break;}
  }
  if(lush){col[h+65]=plant<.006?122:121;continue;}
  double cover=understoryNoise.at(X/23.+81,Z/23.-93)+.25*understoryNoise.at(X/7.,Z/7.);
  if(cover>.25&&plant<.45){col[h+64]=80;if(plant<.24)col[h+65]=120;}
 }
}
