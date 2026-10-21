#include "native/Engine.h"
#include <fstream>
#include <iostream>

namespace nativegen {
struct EngineBoundaryTest {
    static void require(bool condition, char const* message) {
        if (!condition) throw std::runtime_error(message);
    }
    static void run(J const& data) {
        Engine e(data, {}, true);
        constexpr int height = 384, cells = 256 * height;
        constexpr uint16_t guard = 60000;
        std::array<Surface, 256> surfaces{};
        for (auto& s : surfaces) s.kind = 1;
        auto& s = surfaces[255];
        s = Surface{}; s.height = 317; s.glacier = .7; s.superGlacier = .2;
        int X = 0, Z = 0; bool found = false;
        for (int z = -256; z < 256 && !found; ++z)
            for (int x = -256; x < 256 && !found; ++x) {
                double broad = e.landscapeNoise.at(x/61., z/61.);
                double fine = e.landscapeNoise.at(x/13.+17, z/13.-11);
                double wind = std::sin((x*.83+z*.55)/17.+2*broad);
                if (wind < -.97 && broad > .05 && fine > .05) { X=x; Z=z; found=true; }
            }
        require(found, "No actual noise sample for two-block snow rise");
        // Suppress unrelated sites; the snow-ridge pass and its noise remain real.
        for (int gz=int(std::floor((Z-15-24)/128.)); gz<=int(std::floor((Z-15+39)/128.)); ++gz)
            for (int gx=int(std::floor((X-15-24)/128.)); gx<=int(std::floor((X-15+39)/128.)); ++gx)
                e.detailSites.get(key2(gx,gz), [] { return Engine::DetailSite{}; });
        std::vector<uint16_t> raw(cells+128, 0);
        std::fill(raw.begin()+cells, raw.end(), guard);
        raw[255*height+317+64]=143;
        e.landscapePass(X-15+32768,Z-15+32768,raw,surfaces);
        require(std::all_of(raw.begin()+cells,raw.end(),[](auto b){return b==guard;}),
                "Landscape pass wrote beyond last column");
        require(raw[255*height+383]==146, "Snow cap must remain at y=319");
        // Same operation on an interior column must not overwrite the next bedrock.
        surfaces[254]=surfaces[255]; surfaces[255].kind=1;
        std::fill(raw.begin(),raw.end(),0); raw[255*height]=1;
        raw[254*height+317+64]=143;
        e.landscapePass(X-14+32768,Z-15+32768,raw,surfaces);
        require(raw[255*height]==1, "Landscape pass corrupted adjacent bedrock");
        surfaces[254].kind=1;
        e.fracturedGlaciers=true; e.refinedGlaciers=false; e.snowRifts=false;
        e.closedIceRifts=false; e.gradualIceEcotone=false; e.glacierEcologyEnabled=false;
        e.continuousGlacierBodies=false;
        for (int y : {-64,-48,318,319}) {
            s=Surface{}; s.height=y; s.glacier=1; s.superGlacier=1;
            std::fill(raw.begin(),raw.begin()+cells,143);
            std::fill(raw.begin()+cells,raw.end(),guard);
            auto rifts=e.glacierBody(32768,32768,raw,surfaces);
            require(rifts[255].bodyTop<=319, "Glacier publishes out-of-world height");
            require(std::all_of(raw.begin()+cells,raw.end(),[](auto b){return b==guard;}),
                    "Glacier wrote beyond last column");
        }
        std::array<uint16_t,384> column{};
        s.height=319;
        bool rejected=false;
        try { e.field.materialColumn(s,32768,32768,320,column.data()); }
        catch(std::out_of_range const&) { rejected=true; }
        require(rejected,"Out-of-range material column must be rejected before writing");
    }
};
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("expected generated native input");
        nativegen::J data;std::ifstream(argv[1])>>data;
        nativegen::EngineBoundaryTest::run(data);
        std::cout<<"production snow/glacier passes preserve column bounds and neighbour bedrock\n";
    } catch(std::exception const& e) { std::cerr<<e.what()<<'\n';return 1; }
}
