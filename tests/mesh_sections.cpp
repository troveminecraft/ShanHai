#undef NDEBUG
#include "MeshSectionTrace.h"
#include <cassert>
int main(){terrain_runtime::MeshSectionTrace t;
 t.record(-1,128,17,false,0);t.record(-1,176,17,true,3,1200);
 auto a=t.snapshot()["columns"][0];assert(a["chunk"]==nlohmann::json::array({-1,1}));
 assert(a["seenMask"].get<uint64_t>()==((1ull<<12)|(1ull<<15)));
 assert(a["builderReturnedMask"]==(1ull<<15));
 t.record(-1,128,17,true,0,500);a=t.snapshot()["columns"][0];
 assert(a["builderReturnedMask"]==a["seenMask"]&&a["nativeCanRenderMask"]==(1ull<<15));
 for(int x=0;x<10000;++x)t.record(x*16,64,x*32,true,3,1);
 assert(t.snapshot()["columns"].size()==64);
}
