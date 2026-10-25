#pragma once
#include "Field.h"
#include "../rules/StructurePlan.h"
#include <map>
#include <memory>
#include <mutex>

namespace nativegen {
// Prefill and decoration can ask for candidates before the native structure
// stage is initialized, and can arrive on different threads. Own the immutable
// recipe and its mutable Field caches together; never borrow lab::field/input.
// No native registry or structure-stage lock is acquired by this planner.
class VillageCandidatePlanner {
 struct State {
  J recipe;
  Field field;
  std::map<std::pair<int,int>,worldgen_rules::ChunkPosition> candidates;
  explicit State(J data):recipe(std::move(data)),field(recipe){}
 };
 std::mutex mutex;
 std::unique_ptr<State> state;
 std::string identity;
 uint32_t seed=0;
public:
 template<class LoadRecipe>
 worldgen_rules::ChunkPosition get(std::string const&world,uint32_t worldSeed,int x,int z,LoadRecipe load){
  std::lock_guard lock(mutex);
  if(!state||identity!=world||seed!=worldSeed){
   // Publish only a fully initialized state. A failed read/constructor leaves
   // the previous world intact and the next request can retry safely.
   auto next=std::make_unique<State>(load());
   state=std::move(next);identity=world;seed=worldSeed;
  }
  auto key=std::pair{x,z};auto it=state->candidates.find(key);
  if(it!=state->candidates.end())return it->second;
  auto candidate=worldgen_rules::villageCandidateForTerrain(x,z,seed,[&](int X,int Z){return state->field.surface(X+32768,Z+32768);});
  if(state->candidates.size()>=512)state->candidates.clear();
  state->candidates.emplace(key,candidate);return candidate;
 }
 void clear(){std::lock_guard lock(mutex);state.reset();identity.clear();}
};
}
