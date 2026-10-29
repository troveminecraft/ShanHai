#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include <limits>
namespace nativegen::regional {
struct LakeBoundary { int level=0; std::vector<std::array<int,3>> rows; size_t samples=0; };
// Coarse candidates are never a final water mask. Verify only the connected
// flooded component and its shoreline, reusing heights when lowering a spill.
template<class Sample, class Eligible>
LakeBoundary closedLake(int x0,int z0,int side,int sx,int sz,int maximum,int minimum,
                        Sample sample,Eligible eligible,size_t minimumArea=256) {
    LakeBoundary result;
    if(side<3||side>257||sx<=0||sz<=0||sx>=side-1||sz>=side-1)return result;
    constexpr int Unknown=std::numeric_limits<int>::min();
    std::vector<int> heights(side*side,Unknown);
    std::vector<unsigned char> usable(side*side),wet(side*side);
    auto height=[&](int i){
        if(heights[i]==Unknown){auto s=sample(x0+i%side,z0+i/side);heights[i]=s.height;usable[i]=eligible(s);++result.samples;}
        return heights[i];
    };
    std::vector<int> pending,component;
    for(int level=maximum;level>=minimum;--level){
        int start=sz*side+sx;
        if(height(start)>=level)continue;
        std::fill(wet.begin(),wet.end(),0);pending={start};component.clear();wet[start]=1;bool spill=false;
        while(!pending.empty()){
            int i=pending.back();pending.pop_back();int x=i%side,z=i/side;
            if(x==0||z==0||x==side-1||z==side-1||!usable[i]){spill=true;break;}
            component.push_back(i);
            for(int n:{i-1,i+1,i-side,i+side})if(!wet[n]&&height(n)<level){wet[n]=1;pending.push_back(n);}
        }
        if(spill||component.size()<minimumArea)continue;
        std::sort(component.begin(),component.end());result.level=level;
        for(size_t n=0;n<component.size();){int first=component[n],last=first;
            while(++n<component.size()&&component[n]==last+1&&component[n]/side==first/side)last=component[n];
            result.rows.push_back({z0+first/side,x0+first%side,x0+last%side});
        }
        return result;
    }
    return result;
}
}
