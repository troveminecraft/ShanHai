#include "PaletteContract.h"
#include "ChunkCodec.h"
#include <iostream>
int main() {
    using namespace terrain_runtime;
    try {
        validatePaletteContract(paletteContract());
        auto legacy=paletteContract();legacy["version"]="karma-terrain-2.195.0-native-agent-a";
        legacy["palette"].erase(legacy["palette"].begin()+168,legacy["palette"].end());
        legacy["biomeNames"].erase(legacy["biomeNames"].begin()+25,legacy["biomeNames"].end());
        validatePaletteContract(legacy);
        legacy["version"]="karma-terrain-2.196.0-native-agent-a";
        bool truncated=false;try{validatePaletteContract(legacy);}catch(...){truncated=true;}
        if(!truncated)throw std::runtime_error("New world accepted legacy truncated palette");
        validateColumnRun(0,384,167,168);
        validateColumnRun(383,1,0,168);
        validateColumnRun(319,65,1,168);
        for(auto run: {std::array{-1,1,0},std::array{384,1,0},std::array{383,2,0},std::array{0,384,168}}){
            bool rejected=false;
            try{validateColumnRun(run[0],run[1],run[2],168);}catch(std::exception const&){rejected=true;}
            if(!rejected)throw std::runtime_error("Invalid fill run accepted");
        }
        auto reject=[](auto data) {
            try {validatePaletteContract(data);}
            catch(std::exception const&) {return;}
            throw std::runtime_error("Changed numeric terrain contract was accepted");
        };
        auto data=paletteContract();std::swap(data["palette"][8],data["palette"][9]);reject(data);
        data=paletteContract();data["palette"].erase(0);reject(data);
        data=paletteContract();std::swap(data["biomeNames"][0],data["biomeNames"][1]);reject(data);
        data=paletteContract();data["palette"][146]="snow_layer";reject(data);
        // Exercise the real decoder's invalid runs, palette and biome checks.
        std::vector<uint8_t> bytes{75,87,50,0};bytes.resize(260);
        for(int i=0;i<256;++i)bytes.insert(bytes.end(),{1,0,128,1,167,0});
        decodeChunk(bytes,168,25);
        for(int mode=0;mode<4;++mode) {
            auto invalid=bytes;
            if(mode==0)invalid[4]=25;
            if(mode==1)invalid[264]=168;
            if(mode==2)invalid[262]=129;
            if(mode==3)invalid.pop_back();
            bool rejected=false;
            try {decodeChunk(invalid,168,25);}catch(std::exception const&){rejected=true;}
            if(!rejected)throw std::runtime_error("Invalid encoded column accepted");
        }
        std::cout<<"palette names/states/order and malformed chunk contracts checked\n";
    }catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}
}
