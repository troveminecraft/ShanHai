#pragma once
#include <map>
#include <tuple>
namespace terrain_runtime {
// Immutable terrain reads only, scoped to one native decoration callback.
// Bound memory without failing placement when an unusually large piece reads more.
template<class Surface,class Value> class SeededColumnCache {
    std::map<std::pair<int,int>,Surface> columns;
    std::map<std::tuple<int,int,int>,Value> blocks;
public:
    template<class Sample,class Material> Value get(int x,int y,int z,Sample sample,Material material){
        auto point=std::tuple{x,y,z};if(auto b=blocks.find(point);b!=blocks.end())return b->second;
        auto key=std::pair{x,z};auto c=columns.find(key);
        Surface surface;
        if(c!=columns.end())surface=c->second;
        else {surface=sample(x,z);if(columns.size()<4096)columns.emplace(key,surface);}
        Value value=material(surface,x,y,z);if(blocks.size()<32768)blocks.emplace(point,value);return value;
    }
};
}
