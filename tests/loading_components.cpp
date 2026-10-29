#undef NDEBUG
#include "SeededColumnCache.h"
#include "WorkerPool.h"
#include <future>
#include "DeferredDiagnosticLog.h"
#include <cassert>
#include <iostream>
#include <mutex>
struct Surface {int height;};
int main(int argc,char**argv){
 assert(argc==2);std::filesystem::path file=argv[1];assert(!std::filesystem::exists(file));
 terrain_runtime::WorkerPool<4> pool;pool.configure(2);
 std::vector<std::future<void>> workers;
 for(int i=0;i<8;++i)workers.push_back(std::async(std::launch::async,[&]{for(int n=0;n<64;++n){auto lease=pool.acquire();assert(lease.i<2);}}));
 for(auto&worker:workers)worker.get();pool.configure(4);
 {auto lease=pool.acquire();bool rejected=false;try{pool.configure(2);}catch(std::logic_error const&){rejected=true;}assert(rejected);}
 terrain_runtime::SeededColumnCache<Surface,int> cache;int samples=0,materials=0;
 auto sample=[&](int x,int z){++samples;return Surface{x+z};};
 auto material=[&](Surface s,int,int y,int){++materials;return s.height+y;};
 for(int repeat=0;repeat<4;++repeat)for(int y=0;y<200;++y)for(int x=0;x<8;++x)
  assert(cache.get(x,y,3,sample,material)==x+y+3);
 assert(samples==8&&materials==1600);
 for(int x=0;x<40000;++x)assert(cache.get(x,1,0,sample,material)==x+1); // bounds do not abort native placement
 std::mutex registry;
 {terrain_runtime::DeferredDiagnosticBatch batch;
  {std::lock_guard guard(registry);terrain_runtime::DeferredDiagnosticLine(file)<<"first\n";assert(!std::filesystem::exists(file));}
  assert(!std::filesystem::exists(file));
 }
 std::ifstream in(file);std::string row;std::getline(in,row);assert(row=="first");
 try {terrain_runtime::DeferredDiagnosticBatch batch;std::lock_guard guard(registry);terrain_runtime::DeferredDiagnosticLine(file)<<"unwind\n";throw 1;}catch(int){}
 assert(terrain_runtime::DeferredDiagnosticBatch::current==nullptr);
 assert(terrain_runtime::DeferredDiagnosticBatch::failures==0);
 std::cout<<"PASS bounded immutable column reuse and deferred logging including unwind\n";
}
