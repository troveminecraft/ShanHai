#undef NDEBUG
#include "GlacierWeatherPolicy.h"
#include <cassert>
int main(){
 // Seed13's glacier at (21548,5312) lies beyond the former test square.
 assert(glacier_weather::sampleChunkAllowed(true,1346,332));
 assert(!glacier_weather::sampleChunkAllowed(false,1346,332));
 assert(glacier_weather::sampleChunkAllowed(true,-1251,-1251));
 assert(glacier_weather::sampleChunkAllowed(false,-1250,1249));
 assert(!glacier_weather::sampleChunkAllowed(false,1250,0));
 assert(glacier_weather::sampleChunkAllowed(true,-terrain_bounds::GameChunkLimit,0));
 assert(!glacier_weather::sampleChunkAllowed(true,terrain_bounds::GameChunkLimit,0));
 assert(!glacier_weather::sampleChunkAllowed(true,0,-terrain_bounds::GameChunkLimit-1));
 glacier_weather::Policy p;
 for(int i=0;i<200;++i)assert(p.step(false,false)==0);
 for(int i=0;i<9;++i)assert(p.step(true,true)==0);
 assert(p.step(true,true)==1&&p.snowy);
 assert(p.step(false,true)==0&&p.snowy);
 assert(p.step(false,false)==-1&&!p.snowy);
 int restores=0;
 for(int i=0;i<39;++i){int a=p.step(i%2==0,i%2==0);assert(a<=0);restores+=a<0;}
 assert(restores==5&&!p.snowy);
 for(int i=0;i<9;++i)assert(p.step(true,true)==0);
 assert(p.step(true,true)==1);
 for(int i=0;i<99;++i)assert(p.step(true,true)==0);
 assert(p.step(true,true)==1);
 assert(p.step(false,false)==-1);
}
