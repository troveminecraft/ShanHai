#include "FailureRecord.h"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    auto value=terrain_runtime::failureRecord("terrain","0.1.2","fill","-9223372036854775808",
        "test",std::array<int,2>{-1,12},"fixture failure","stop before saving incomplete chunk");
    auto path=std::filesystem::path(argv[1])/"fixture-failure.json";
    if(!terrain_runtime::writeFailure(path,value))return 2;
    nlohmann::json read;std::ifstream(path)>>read;
    if(read!=value||read["seed"]!="-9223372036854775808"||read["chunk"][0]!=-1)return 3;
    if(terrain_runtime::writeFailure(path/"missing"/"file",value))return 4;
    std::cout<<"failure context round-trip and unwritable sink checked\n";
}
