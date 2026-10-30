#undef NDEBUG
#include "native/Field.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace nativegen;
namespace nativegen {
struct CoastalContinuityTest {
    static void check(J data){
        for(int sea:{40,64,100}){
            data["config"]["seaLevel"]=sea;data["config"]["coastalContinuity"]=1;
            Field field(data);field.waterEnabled=true;
            // Inject one constant coastal cell so only the altitude envelope
            // is measured; no unrelated biome/noise changes can hide a jump.
            for(int z=0;z<2;++z)for(int x=0;x<2;++x)field.coastalNodes.get(key2(x,z),[]{return 1.;});
            double last=1;
            for(int i=0;i<=4400;++i){
                double h=sea+21+i*.01,w=field.coastWeight(8,8,h);
                assert(w>=0&&w<=last+1e-12&&last-w<.0004);last=w;
            }
            assert(last==0&&field.coastWeight(8,8,sea+65.01)==0);
            double cutoff=field.elevationVariants?141:105;
            assert(std::abs(field.coastWeight(8,8,cutoff-.001)-field.coastWeight(8,8,cutoff+.001))<.001);
        }
    }
};
}
int main(int argc,char**argv){
    assert(argc==2);J legacy;std::ifstream(argv[1])>>legacy;
    CoastalContinuityTest::check(legacy);
    auto revised=legacy;revised["config"]["coastalContinuity"]=1;
    Field before(legacy),after(revised);
    assert(before.surface(-211+32768,1473+32768).height==141);
    int last=-1;
    for(int x=-214;x<=-208;++x){
        auto a=before.surface(x+32768,1473+32768),b=after.surface(x+32768,1473+32768);
        if(last>=0)assert(std::abs(b.height-last)<=2);last=b.height;
        std::cout<<x<<" legacy="<<a.height<<" continuous="<<b.height<<'\n';
    }
    assert(after.surface(-211+32768,1473+32768).height>=153);
}
