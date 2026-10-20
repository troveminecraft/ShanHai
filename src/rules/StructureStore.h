#pragma once
#include "../persistence/DurableJson.h"

namespace worldgen_rules::structure_store {
using J=nlohmann::json;
using worldgen_persistence::label;
using worldgen_persistence::read;
using worldgen_persistence::atomicWrite;
inline void persist(std::filesystem::path const&dst,J const&value){
 // Both copies contain NEW claims before any entity or loot side effect.
 atomicWrite(dst,value);auto mirror=dst;mirror+=".bak";atomicWrite(mirror,value);
}
inline void validate(J const&j,std::string const&world,J const&seed){
 if(!j.is_object()||j.value("world",std::string{})!=world||!j.contains("terrainSeed")||j.at("terrainSeed")!=seed||!j.contains("entries")||!j.at("entries").is_object()||(j.contains("structureEntries")&&!j.at("structureEntries").is_object()&&!j.at("structureEntries").is_null()))throw std::runtime_error("Entity ledger identity/schema mismatch");
}
struct Result {J data;bool enabled=false,recovered=false;std::string error;};
inline Result load(std::filesystem::path const&dst,std::string const&world,J const&seed){
 auto mirror=dst;mirror+=".bak";Result r;r.data={{"world",world},{"terrainSeed",seed},{"entries",J::object()},{"structureEntries",J::object()}};
 if(!std::filesystem::exists(dst)&&!std::filesystem::exists(mirror)){r.enabled=true;return r;}
 try{auto j=read(dst);validate(j,world,seed);r.data=std::move(j);r.enabled=true;return r;}catch(std::exception const&e){r.error=e.what();}
 try{auto j=read(mirror);validate(j,world,seed);
  if(std::filesystem::exists(dst)){auto bad=dst;bad+=".corrupt-"+std::to_string(worldgen_persistence::diagnosticStamp());std::filesystem::copy_file(dst,bad);}
  atomicWrite(dst,j);r.data=std::move(j);r.enabled=true;r.recovered=true;
 }catch(std::exception const&e){r.error+="; backup: "+std::string(e.what());}
 // Invalid claims never disable terrain or reset the ledger to allow repeats.
 return r;
}
}
