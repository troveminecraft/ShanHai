#include "../ports/windows-client/WorldStartup.h"
#include <cassert>
#include <iostream>
using J=nlohmann::json;
int main(){
    J valid={{"minecraft:dimension",{{"description",{{"identifier","minecraft:overworld"}}},
        {"components",{{"minecraft:dimension_bounds",{{"min",-64},{"max",512}}},
            {"minecraft:generation",{{"generator_type","void"}}}}}}}};
    client::startup::requireHeightResource(valid.dump(),-64,512);
    auto rejected=[&](std::string const& text){try{client::startup::requireHeightResource(text,-64,512);return false;}catch(std::exception const&){return true;}};
    assert(rejected(""));assert(rejected("{}"));
    for(auto max:{320,513}){auto bad=valid;bad["minecraft:dimension"]["components"]["minecraft:dimension_bounds"]["max"]=max;assert(rejected(bad.dump()));}
    auto bad=valid;bad["minecraft:dimension"]["components"]["minecraft:generation"]["generator_type"]="overworld";assert(rejected(bad.dump()));
    bad=valid;bad["minecraft:dimension"]["description"]["identifier"]="minecraft:nether";assert(rejected(bad.dump()));
    std::cout<<"Native selected-height resource checks passed\n";
}
