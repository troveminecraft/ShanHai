#include "TerrainKernel.h"
#include <fstream>
#include <future>
#include <iostream>
using J=terrain_kernel::J;
void require(bool ok,char const*message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("expected native recipe");J recipe;std::ifstream(argv[1])>>recipe;
 auto context=std::make_shared<terrain_kernel::Context>();
 auto make=[&](J const&r,terrain_kernel::ContextPtr c){return std::make_unique<terrain_kernel::Generator>(r,terrain_kernel::Reservations{},false,nullptr,nullptr,std::move(c));};
 auto a=make(recipe,context),b=make(recipe,context);
 auto first=std::async(std::launch::async,[&]{return a->chunk(0,120);});auto second=std::async(std::launch::async,[&]{return b->chunk(0,120);});
 auto expected=first.get();require(second.get()==expected,"shared concurrent trees change output");
 auto before=context->statistics().at("jungleShapes");require(before.at("builds").get<uint64_t>()>0,"fixture has no jungle trees");
 require(a->chunk(0,120)==expected,"same chunk changed after publication");
 auto after=context->statistics().at("jungleShapes");require(after.at("builds")==before.at("builds"),"same world rebuilt resident shapes");
 auto different=recipe;different["config"]["seed"]=recipe.at("config").at("seed").get<uint32_t>()^0x31415926u;
 different["seedManifest"]["runtimeSeed"]=different["config"]["seed"];
 auto mixed=make(different,context),isolated=make(different,std::make_shared<terrain_kernel::Context>());
 auto altered=mixed->chunk(0,120);require(altered==isolated->chunk(0,120),"alternate recipe reused wrong world's trees");require(altered!=expected,"alternate seed fixture was not distinct");
 auto newContext=std::make_shared<terrain_kernel::Context>();auto reopened=make(recipe,newContext);require(newContext->statistics().at("jungleShapes").at("entries")==0,"new world retained old shapes");require(reopened->chunk(0,120)==expected,"new world output changed");
 std::cout<<"PASS shared-worker determinism, same-world reuse, alternate recipe isolation, fresh world\n";
}catch(std::exception const&e){std::cerr<<e.what()<<std::endl;return 1;}}
