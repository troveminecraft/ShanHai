#pragma once
#include <cmath>
#include "native/Surface.h"
#include <string>
#include <string_view>
#include <vector>

namespace terrain_runtime::biome_catalog {
struct Entry {
    std::string key, en, zh, descriptionEn, descriptionZh;
    int biome; // -1..-5 are landforms, independent of the native climate palette.
    bool snow=false;
    std::string id() const {return "peakarma:"+key;}
    std::string title(bool chinese) const {return chinese?zh:en;}
    std::string description(bool chinese) const {return chinese?descriptionZh:descriptionEn;}
};
inline bool chinese(std::string_view language) {
    return language.size()>=2&&(language[0]=='z'||language[0]=='Z')&&(language[1]=='h'||language[1]=='H')
        &&(language.size()==2||language[2]=='_'||language[2]=='-');
}
inline std::vector<Entry> const& entries() {
    static const auto all=[] {
        std::vector<Entry> out{
            {"deep_ocean","Deep Ocean","深海","Deep offshore waters and seabeds.","远离海岸的深水海域与海床。",0},
            {"ocean","Ocean","海洋","Open sea with underwater terrain.","开阔海域与海底地形。",1},
            {"frozen_ocean","Frozen Ocean","冻洋","Cold ocean with frozen surface waters.","海面冻结的寒冷海域。",2},
            {"beach","Beach","沙滩","Sandy shores between land and sea.","陆地与海洋相接的沙质海岸。",3},
            {"cold_beach","Cold Beach","寒冷沙滩","Cold sandy coastal terrain.","寒冷地区的沙质海岸。",4},
            {"ice_plains","Snowy Plains","雪原","Open, snow-covered cold terrain.","寒冷、开阔的覆雪地带。",5},
            {"taiga","Taiga","针叶林","Cool forests dominated by conifers.","以针叶树为主的凉冷森林。",6},
            {"plains","Plains","平原","Open grassland with sparse trees.","树木稀疏的开阔草地。",7},
            {"meadow","Meadow","草甸","Temperate grassy terrain with scattered trees.","温带草地与零散树木。",8},
            {"forest","Forest","森林","Temperate woodland with varied vegetation.","植被多样的温带森林。",9},
            {"swampland","Swamp","沼泽","Wet ground, shallow water and swamp trees.","湿润地表、浅水与沼泽树木。",10},
            {"desert","Desert","沙漠","Large dry regions with sandy ground.","沙质地表为主的大片干旱地区。",11},
            {"savanna","Savanna","热带草原","Warm grassland with acacia trees.","生长金合欢的温暖草原。",12},
            {"jungle","Jungle","丛林","Humid forest with tall jungle trees.","高大丛林树组成的湿润森林。",13},
            {"extreme_hills","Mountains","山地","High mountain terrain above the uplands.","高原之上的高山地带。",14},
            {"mesa","Badlands","恶地","Dry terracotta terrain and red sand.","陶瓦与红沙组成的干旱地形。",15},
            {"arid_basin","Arid Basin","干旱盆地","Dry depressions in legacy terrain recipes.","旧地形配方中的干旱洼地。",16},
            {"giant_forest","Giant Forest","巨木森林","Regions containing exceptionally large trees.","生长巨型树木的森林地区。",17},
            {"river","River","河流","Flowing channels, including larger waterways.","流动的水道，包含大型江流。",18},
            {"frozen_river","Frozen River","冰河","Frozen channels and cold lake waters.","冻结的水道与寒冷湖水。",19},
            {"stone_beach","Rocky Ground","岩石地带","Exposed stone on shores and barren terrain.","海岸和贫瘠地区的裸露岩石地表。",20},
            {"warm_ocean","Warm Ocean","暖海","Warm marine waters and seabeds.","温暖的海域与海床。",21},
            {"cold_ocean","Cold Ocean","冷海","Cold, open marine waters.","寒冷的开阔海域。",22},
            {"birch_forest","Birch Forest","白桦林","Temperate woodland dominated by birches.","以白桦为主的温带林地。",23},
            {"ice_plains_spikes","Ice Spikes","冰刺平原","Snowy terrain with ice formations.","分布冰刺的覆雪地带。",24},
            {"mushroom_island","Mushroom Island","蘑菇岛","Offshore islands with mycelium and mushrooms.","海中生长菌丝与蘑菇的岛屿。",25},
            {"mesa_bryce","Eroded Badlands","侵蚀恶地","Eroded terracotta towers and ridges.","受到侵蚀的陶瓦石柱与山脊。",26},
            {"mesa_plateau","Badlands Plateau","恶地高原","Elevated terracotta plateaus and cliffs.","海拔较高的陶瓦高原与悬崖。",27},
            {"mesa_plateau_stone","Wooded Badlands Plateau","繁茂恶地高原","Elevated badlands with scattered vegetation.","分布零散植被的恶地高原。",28}
        };
        for(int i=0;i<29;++i) {
            auto e=out[i];e.key="snowy_"+e.key;e.en="Snow-covered "+e.en;e.zh="覆雪"+e.zh;
            e.descriptionEn="Snow-covered variant. "+e.descriptionEn;
            e.descriptionZh="覆雪变种。"+e.descriptionZh;e.snow=true;out.push_back(std::move(e));
        }
        for(auto e:std::vector<Entry>{
            {"roofed_forest","Dark Forest","黑森林","Thick trunks beneath a shared canopy, with occasional hollows.","粗壮树干支撑连片树冠，部分树木带有树洞。",58},
            {"pale_garden","Pale Forest","苍白森林","Pale woodland with moss and occasional hearts inside tree hollows.","附着苔藓的苍白林地，部分树洞内藏有嘎吱之心。",59},
            {"cherry_grove","Cherry Grove","樱花林","Continuous cherry woodland surrounding a giant central tree.","围绕中心巨型樱花树的连片樱花林。",60},
            {"mangrove_swamp","Mangrove Swamp","红树林沼泽","Basin wetlands with shallow water and dense mangrove roots.","浅水盆地湿地与密集的红树林根系。",61},
            {"dappled_forest","Dappled Forest","斑驳森林","Colorful poplar woodland with shelf mushrooms.","彩色杨树林与附生层状蘑菇。",62},
            {"glacier","Glacier","冰川","Glacial terrain outside the super-glacier core.","超级冰川核心以外的冰川地带。",-1},
            {"super_glacier","Super Glacier","超级冰川","Extensive ice-covered terrain with large ice spires.","大片覆冰地形与大型冰刺。",-2},
            {"volcano","Volcano","火山","Volcanic slopes, craters and underground magma.","火山山坡、火山口与地下岩浆。",-3},
            {"rolling_plateau","Plateau","高原","Broad, gently rolling uplands.","宽阔、缓起伏的高海拔地区。",-4},
            {"inland_basin","Inland Basin","内陆盆地","Enclosed inland lowlands between Y60 and Y100.","海拔 Y60 至 Y100 的封闭内陆低地。",-5}
        })out.push_back(std::move(e));
        return out;
    }();
    return all;
}
inline int find(std::string_view id) {
    if(!id.starts_with("peakarma:"))return -1;
    id.remove_prefix(9);
    auto const& all=entries();
    for(size_t i=0;i<all.size();++i)if(all[i].key==id)return int(i);
    return -1;
}
inline bool available(Entry const&e,size_t paletteSize,bool elevation) {
    return e.biome<0 || (size_t(e.biome+(e.snow?29:0))<paletteSize
        &&(!e.snow||elevation)&&!(e.biome==16&&elevation));
}
inline bool matches(Entry const&e,nativegen::Surface const&s,bool elevation) {
    switch(e.biome) {
    case -1:return s.glacier>.5&&s.superGlacier<.1;
    case -2:return s.superGlacier>.5;
    case -3:return s.volcanic>.5;
    case -4:return s.upland>.55&&s.height>=150&&s.water<s.height&&s.volcanic<.1;
    case -5:return s.closedBasin&&s.basinWeight>.8&&s.height>=60&&s.height<=100;
    }
    bool snow=elevation&&s.snowCover>.70&&s.biome<29;
    if(e.snow!=snow)return false;
    if(e.biome==14&&elevation)return s.altitudeBand==2&&s.height>=300&&s.water<s.height;
    return s.biome==e.biome;
}
}
