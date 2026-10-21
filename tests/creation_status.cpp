#include "../ports/windows-client/NewWorldStatus.h"
#include <iostream>
int main() {
    using nlohmann::json;
    auto result=creation_status::failure(json{{"state","error"},{"phase","准备失败"},{"detail","Node.js missing"}});
    if(!result||result->find("Node.js missing")==std::string::npos)return 1;
    if(creation_status::failure(json{{"state","running"},{"detail","old error"}}))return 2;
    if(!creation_status::failure(json{{"state","error"}}))return 3;
    std::cout<<"creation errors retain worker detail and do not classify running status as failure\n";
}
