
int verifyStructureVegetation(nativegen::J const& data,terrain_kernel::ContextPtr context){
 using Clock=std::chrono::steady_clock;using J=nativegen::J;
 terrain_kernel::Field heightField(data,context);int height=heightField.generationHeight();
 auto decode=[&](std::vector<uint8_t>const&b){std::vector<uint16_t>r(256*height);size_t p=260;auto word=[&](){int v=b.at(p)|(b.at(p+1)<<8);p+=2;return v;};for(int c=0;c<256;++c){int n=word(),y=0;for(int k=0;k<n;++k){int len=word(),id=word();if(len<=0||len>height-y)throw std::runtime_error("Vegetation verification column overflow");std::fill_n(r.begin()+c*height+y,len,id);y+=len;}if(y!=height)throw std::runtime_error("Vegetation verification column incomplete");}if(p!=b.size())throw std::runtime_error("Vegetation verification trailing data");return r;};
 auto plant=[](int b){return(b>=10&&b<=17)||b==25||b==26||b==37||b==40||b==41||b==61||b==65||b==66||b==69||b==70||(b>=75&&b<=80)||(b>=82&&b<=84)||(b>=89&&b<=93)||b==120||(b>=149&&b<=167);};
 J rows=J::array();unsigned mismatches=0,changes=0;
 for(auto b:{std::array{-2960,2256,-2949,2270},std::array{-3024,1280,-3013,1294},std::array{-192,2288,-186,2296}}){
  std::vector<std::array<int,4>> reserved{{b[0]+32768-4,b[1]+32768-4,b[2]+32768+4,b[3]+32768+4}};
  terrain_kernel::Field shared(data,context);terrain_kernel::Generator a(data,{},false,nullptr,nullptr,context),c(data,reserved,false,nullptr,nullptr,context),fastA(data,{},true,&shared),fastC(data,reserved,true,&shared);
  double fullMs=0,fastMs=0;unsigned bad=0,n=0;
  for(int z=b[1]/16-2;z<=b[3]/16+2;++z)for(int x=b[0]/16-2;x<=b[2]/16+2;++x){
   auto t=Clock::now();auto before=decode(a.chunk(x,z)),after=decode(c.chunk(x,z));fullMs+=std::chrono::duration<double,std::milli>(Clock::now()-t).count();
   t=Clock::now();auto f=fastA.vegetationChunk(x,z),g=fastC.vegetationChunk(x,z);fastMs+=std::chrono::duration<double,std::milli>(Clock::now()-t).count();
   for(size_t i=0;i<f.size();++i){bool old=plant(before[i])&&before[i]!=after[i],now=plant(f[i])&&f[i]!=g[i];if(old)++changes;if(old!=now){++bad;if(mismatches+bad<6)std::cerr<<x<<","<<z<<" index "<<i<<" old "<<before[i]<<","<<after[i]<<" fast "<<f[i]<<","<<g[i]<<std::endl;}}++n;
  }
  mismatches+=bad;rows.push_back(J{{"anchor",{b[0],b[1]}},{"chunks",n},{"fullMs",fullMs},{"vegetationMs",fastMs},{"maskMismatches",bad}});
 }
 std::cout<<J{{"rows",rows},{"changedVoxels",changes},{"mismatches",mismatches},{"passed",mismatches==0}}.dump()<<std::endl;return mismatches?1:0;
}
