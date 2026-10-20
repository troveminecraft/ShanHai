#pragma once
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <tuple>
#include <stdexcept>

namespace fossil_journal {
using Json=nlohmann::json;
using Key=std::tuple<int,int,std::string>;
inline uint64_t checksum(std::string const&text){uint64_t h=1469598103934665603ull;for(unsigned char c:text)h=(h^c)*1099511628211ull;return h;}
inline std::string encode(Json const&payload){return Json{{"payload",payload},{"checksum",std::to_string(checksum(payload.dump()))}}.dump()+"\n";}
struct Records {std::map<Key,Json> plans;size_t validBytes=0;};
inline Records decode(std::string const&data,std::string const&world,std::string const&profile){
 Records result;
 while(result.validBytes<data.size()){
  auto next=data.find('\n',result.validBytes);if(next==std::string::npos)break;
  auto row=Json::parse(data.substr(result.validBytes,next-result.validBytes),nullptr,false);
  if(row.is_discarded()||!row.is_object()||!row.contains("payload")||row.value("checksum",std::string{})!=std::to_string(checksum(row["payload"].dump())))throw std::runtime_error("Corrupt complete fossil journal record; refusing placement");
  auto const&p=row["payload"];if(p.at("world")!=world||p.at("profile")!=profile||p.at("schema")!=1)throw std::runtime_error("Fossil journal world/profile/schema mismatch");
  auto a=p.at("anchor");auto key=Key{a.at(0).get<int>(),a.at(1).get<int>(),p.at("kind").get<std::string>()};
  auto [it,inserted]=result.plans.emplace(key,p);if(!inserted&&it->second!=p)throw std::runtime_error("Conflicting fossil plan identity");result.validBytes=next+1;
 }
 return result;
}
}
