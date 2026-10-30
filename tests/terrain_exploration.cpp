#include "TerrainKernel.h"
#include "WorkerPool.h"
#include <Windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <numeric>

using J=terrain_kernel::J;
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
std::string hash(std::vector<uint8_t> const& v){
    std::array<unsigned char,32> h{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(v.data()),ULONG(v.size()),h.data(),ULONG(h.size()))<0)
        throw std::runtime_error("SHA256 failed");
    std::string out;for(auto c:h){out+="0123456789abcdef"[c>>4];out+="0123456789abcdef"[c&15];}return out;
}
J distribution(std::vector<double> values){
    std::sort(values.begin(),values.end());
    if(values.empty())return nullptr;
    return {{"count",values.size()},{"p50",values[(values.size()-1)/2]},
            {"p95",values[(values.size()-1)*95/100]},{"p99",values[(values.size()-1)*99/100]},
            {"max",values.back()},{"sum",std::accumulate(values.begin(),values.end(),0.)}};
}
int main(int argc,char**argv){try{
    if(argc!=6)throw std::runtime_error("usage: terrain_exploration recipe.json route.json output.json cache-directory workers");
    J recipe,route;std::ifstream(argv[1])>>recipe;std::ifstream(argv[2])>>route;
    if(!route.is_array()||route.empty()||route.size()>8192)throw std::runtime_error("route must contain 1..8192 chunk coordinate pairs");
    for(auto const& cp:route)if(!cp.is_array()||cp.size()!=2||!cp[0].is_number_integer()||!cp[1].is_number_integer())
        throw std::runtime_error("route entries must be [chunkX, chunkZ] integer pairs");
    if(std::filesystem::exists(argv[3]))throw std::runtime_error("refusing to overwrite an earlier result");
    unsigned workers=std::stoul(argv[5]);terrain_runtime::WorkerPool<4> pool;pool.configure(workers);
    auto setup=Clock::now();auto ctx=std::make_shared<terrain_kernel::Context>();ctx->storage(recipe,argv[4]);
    std::vector<std::unique_ptr<terrain_kernel::Generator>> engines;
    for(unsigned w=0;w<workers;++w)engines.push_back(std::make_unique<terrain_kernel::Generator>(recipe,terrain_kernel::Reservations{},false,nullptr,nullptr,ctx));
    double setupMs=ms(setup,Clock::now());
    // Intentionally no prepareRegions: first use and future region crossings are timed.
    // Eight bounded callers exercise the host's FIFO calculator pool and shared Context.
    std::vector<J> rows(route.size());std::atomic_size_t next=0;std::vector<std::future<void>> jobs;
    auto begin=Clock::now();
    for(unsigned caller=0;caller<8;++caller)jobs.push_back(std::async(std::launch::async,[&]{
        for(size_t i=next++;i<route.size();i=next++){
            int x=route[i].at(0),z=route[i].at(1);auto admitted=Clock::now();
            Clock::time_point acquired,completed;std::vector<uint8_t> bytes;terrain_kernel::Metrics metrics{};
            {auto lease=pool.acquire();acquired=Clock::now();auto& engine=*engines[lease.i];
             bytes=engine.chunk(x,z);completed=Clock::now();metrics=engine.metrics();}
            std::map<int,unsigned> biomes;for(size_t b=4;b<260;++b)++biomes[bytes.at(b)];
            int dominant=std::max_element(biomes.begin(),biomes.end(),[](auto a,auto b){return a.second<b.second;})->first;
            rows[i]={{"i",i},{"chunk",{x,z}},{"dominantBiome",dominant},{"centreBiome",bytes.at(4+8*16+8)},
                {"requestAtMs",ms(begin,admitted)},{"acquiredAtMs",ms(begin,acquired)},{"completionMs",ms(begin,completed)},
                {"waitMs",ms(admitted,acquired)},{"computeMs",ms(acquired,completed)},{"requestMs",ms(admitted,completed)},
                {"surfaceMs",metrics.surfaceMs},{"columnMs",metrics.columnMs},{"featureMs",metrics.featureMs},
                {"undergroundMs",metrics.undergroundMs},{"forestMs",metrics.forestMs},{"hash",hash(bytes)}};
        }
    }));
    for(auto& job:jobs)job.get();double totalMs=ms(begin,Clock::now());
    std::vector<double> compute,wait,requests,completions;std::map<int,unsigned> biomeCounts;
    for(auto const& r:rows){compute.push_back(r["computeMs"]);wait.push_back(r["waitMs"]);requests.push_back(r["requestMs"]);completions.push_back(r["completionMs"]);++biomeCounts[r["dominantBiome"].get<int>()];}
    std::sort(completions.begin(),completions.end());double worstGap=0,at=0,previous=0,explorationGap=0,explorationAt=0;
    for(double t:completions){if(t-previous>worstGap){worstGap=t-previous;at=previous;}
        if(previous>0&&t-previous>explorationGap){explorationGap=t-previous;explorationAt=previous;}previous=t;}
    J result={{"scope","offline production terrain kernel plus bounded FIFO calculator callers; excludes native structures, task dependencies, network and rendering"},
        {"seed",recipe.at("seedManifest").at("terrainSeed")},{"workers",workers},{"callers",8},{"chunks",rows.size()},
        {"setupMs",setupMs},{"wallMs",totalMs},{"computeMs",distribution(compute)},{"queueMs",distribution(wait)},
        {"requestMs",distribution(requests)},{"maxNoCompletionMs",worstGap},{"noCompletionStartedAtMs",at},
        {"firstCompletedMs",completions.front()},{"maxNoCompletionAfterFirstMs",explorationGap},{"explorationGapStartedAtMs",explorationAt},
        {"regional",ctx->statistics()},{"biomeCounts",biomeCounts},{"rows",rows}};
    result["forestStageOrder"]={"conifers","broadleaves","birches","jungles"};
    std::ofstream out(argv[3]);out<<result.dump(2);out.close();if(!out)throw std::runtime_error("result write failed");
    result.erase("rows");std::cout<<result.dump()<<std::endl;
}catch(std::exception const& e){std::cerr<<e.what()<<std::endl;return 1;}}
