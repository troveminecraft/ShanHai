// Cached regional stand membership; smooth density/height changes at forest
// edges without changing biome labels, ground heights or the drainage graph.
Cache<uint64_t,int> forestEdgeNodes{4096};
int forestFamily(Surface const&s)const{
 if(field.ecologyTreeWeight(s)==0)return 0;
 if(coniferHabitat(s))return 1;if(broadleafHabitat(s))return 2;
 if(birchHabitat(s))return 3;if(jungleHabitat(s))return 4;
 if(s.biome==10||s.wetland>.2)return 5;if(s.biome==12)return 6;
 return 0;
}
double forestInterior(int x,int z,int family){
 int gx=int(std::floor(x/16.)),gz=int(std::floor(z/16.));
 auto node=[&](int X,int Z){return forestEdgeNodes.get(key2(X,Z),[&]{return forestFamily(field.surface(X*16,Z*16,false));})==family?1.:0.;};
 double u=smooth(0,1,x/16.-gx),v=smooth(0,1,z/16.-gz);
 return (node(gx,gz)*(1-u)+node(gx+1,gz)*u)*(1-v)+(node(gx,gz+1)*(1-u)+node(gx+1,gz+1)*u)*v;
}
