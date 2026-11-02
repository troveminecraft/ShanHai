#undef NDEBUG
#include "PaletteAliases.h"
#include <cassert>
#include <iostream>

using J=nlohmann::json;
int main(){
    using terrain_runtime::samePaletteRequest;
    for(auto name:{"dark_oak_leaves","pale_oak_leaves","cherry_leaves"}){
        J explicitLeaf={{"name",std::string("minecraft:")+name},{"states",{{"persistent_bit",true},{"update_bit",false}}}};
        assert(samePaletteRequest(J(name),explicitLeaf));
        assert(samePaletteRequest(explicitLeaf,explicitLeaf));
        auto different=explicitLeaf;different["states"]["update_bit"]=true;
        assert(!samePaletteRequest(J(name),different));
        different=explicitLeaf;different["states"]["invalid_state"]=true;
        assert(!samePaletteRequest(J(name),different));
        different=explicitLeaf;different["extraWater"]=true;
        assert(!samePaletteRequest(J(name),different));
    }
    J x={{"name","minecraft:dark_oak_log"},{"states",{{"pillar_axis","x"}}}};
    auto z=x;z["states"]["pillar_axis"]="z";
    assert(!samePaletteRequest(x,z));
    auto north=J{{"name","minecraft:carved_pumpkin"},{"states",{{"minecraft:cardinal_direction","north"}}}};
    auto east=north;east["states"]["minecraft:cardinal_direction"]="east";
    assert(!samePaletteRequest(north,east));
    assert(!samePaletteRequest(J("dark_oak_leaves"),J("cherry_leaves")));
    std::cout<<"Palette aliases preserve leaf identity, orientation and water-state validation\n";
}
