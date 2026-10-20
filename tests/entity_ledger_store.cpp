#include "../experiments/vanilla-structure-runtime/EntityLedgerStore.h"
#include <cassert>
#include <iostream>
int main(){
 namespace fs=std::filesystem;using namespace entity_store;
 auto dir=fs::temp_directory_path()/L"karma-ledger-tests-封闭洞"/std::to_string(GetCurrentProcessId());fs::create_directories(dir);
 auto p=dir/"claims.json";std::string world="封闭洞测试";J seed=288493980;
 auto fresh=load(p,world,seed);assert(fresh.enabled&&!fresh.recovered);
 auto claimed=fresh.data;claimed["structureEntries"]["village:1"]={{"state","claimed"}};
 persist(p,claimed);auto mirror=p;mirror+=".bak";assert(read(p)==claimed&&read(mirror)==claimed);
 {std::ofstream f(p,std::ios::binary|std::ios::trunc);f<<std::string(136083,'\0');}
 auto recovered=load(p,world,seed);assert(recovered.enabled&&recovered.recovered&&recovered.data==claimed&&read(p)==claimed);
 {std::ofstream f(p);f<<"{";}{std::ofstream f(mirror);}
 auto blocked=load(p,world,seed);assert(!blocked.enabled&&!blocked.error.empty());assert(fs::file_size(p)==1&&fs::file_size(mirror)==0);
 persist(p,claimed);assert(!load(p,"wrong-world",seed).enabled);assert(!load(p,world,123).enabled);
 // Failure after primary commit but before mirror commit cannot spawn; restart
 // retains the newly claimed key. This is the dangerous interruption window.
 auto tmp=mirror;tmp+=".tmp";fs::create_directory(tmp);
 auto next=claimed;next["structureEntries"]["village:2"]={{"state","claimed"}};bool failed=false;
 try{persist(p,next);}catch(...){failed=true;}assert(failed);
 auto interrupted=load(p,world,seed);assert(interrupted.enabled&&interrupted.data==next);
 fs::remove(tmp);persist(p,next);assert(read(mirror)==next);
 fs::remove(p);auto missing=load(p,world,seed);assert(missing.enabled&&missing.recovered&&missing.data==next);
 std::cout<<"PASS durable mirrors, NUL corruption, truncation, missing primary, identity mismatch, interrupted claim commit\n";
 // Test-owned files only, leave diagnostics available for inspection.
}
