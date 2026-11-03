KARMA_NOINLINE void glacierCliffSkin(int x0,int z0,std::vector<uint16_t>&raw,std::array<Surface,256>const&surfaces){
 if(C.value("glacierCliffSkin",0)<1)return;
 bool any=false;for(auto const&s:surfaces)any|=s.superGlacier>.5&&s.glacier>.5;if(!any)return;
 std::array<Surface,64> halo;std::array<bool,64> known{};
 auto neighbour=[&](int x,int z){if(x>=0&&x<16&&z>=0&&z<16)return surfaces[z*16+x];
  int index=x<0?z:x>=16?16+z:z<0?32+x:48+x;
  if(!known[index]){halo[index]=field.surface(x0+x,z0+z);known[index]=true;}return halo[index];};
 coatGlacierCliffs(Height,raw.data(),surfaces,neighbour);
}
