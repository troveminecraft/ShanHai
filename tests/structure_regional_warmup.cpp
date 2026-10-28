#undef NDEBUG
#include "TerrainKernel.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <future>
#include <mutex>
using namespace terrain_kernel;
using Clock=std::chrono::steady_clock;
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("recipe required");
    J recipe;std::ifstream(argv[1])>>recipe;
    std::vector<std::pair<int,int>> points{{0,0},{-224,-3488},{2048,-2048},{-2049,2048}};
    auto run=[&](bool warm){
        auto context=std::make_shared<Context>();Field planning(recipe,context),worker(recipe,context);
        std::vector<std::array<int,5>> values;double outside=0,inside=0;
        for(auto [x,z]:points){
            auto start=Clock::now();
            if(warm)for(int dz:{-192,0,192})for(int dx:{-192,0,192})worker.prepareRegions(x+32768+dx,z+32768+dz);
            outside+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
            start=Clock::now();
            for(int dz=-128;dz<=128;dz+=32)for(int dx=-128;dx<=128;dx+=32){
                auto s=planning.surface(x+32768+dx,z+32768+dz);
                values.push_back({s.height,s.water,s.biome,s.lava,int(s.surf)});
            }
            inside+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        }
        return std::pair{values,J{{"outsideMs",outside},{"insideMs",inside},{"plans",context->statistics()}}};
    };
    auto cold=run(false),warm=run(true);assert(cold.first==warm.first);
    // Also exercise the actual encoded terrain after another worker prepared
    // regions in reverse order, not just the sampled surface values.
    auto context=std::make_shared<Context>();Field worker(recipe,context);
    for(auto i=points.rbegin();i!=points.rend();++i)worker.prepareRegions(i->first+32768,i->second+32768);
    Generator generator(recipe,{},false,nullptr,nullptr,context),reference(recipe);
    for(auto [x,z]:points)assert(generator.chunk(x/16,z/16)==reference.chunk(x/16,z/16));
    std::cout<<J{{"cold",cold.second},{"warmed",warm.second},{"surfaceEqual",true},{"encodedChunksEqual",true},{"gameplayTested",false}}.dump(2)<<'\n';
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
