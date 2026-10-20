#include "../experiments/vanilla-structure-runtime/FossilJournal.h"
#include <cassert>
#include <iostream>
using namespace fossil_journal;
int main(){
 Json a{{"schema",1},{"world","test"},{"profile","frozen"},{"anchor",{-1064,-793}},{"kind","fossil"},{"blocks",Json::array({Json::array({-17016,21,-12674,"minecraft:bone_block",1})})}};
 auto record=encode(a);auto clean=decode(record,"test","frozen");assert(clean.validBytes==record.size()&&clean.plans.size()==1);
 // Every byte truncation of a second append preserves exactly the first
 // durable plan and yields the correct safe truncation offset.
 for(size_t i=0;i<record.size();++i){auto torn=decode(record+record.substr(0,i),"test","frozen");assert(torn.validBytes==record.size()&&torn.plans==clean.plans);}
 auto duplicate=decode(record+record,"test","frozen");assert(duplicate.plans==clean.plans&&duplicate.validBytes==record.size()*2);
 auto rejects=[](auto fn){bool rejected=false;try{fn();}catch(std::exception const&){rejected=true;}assert(rejected);};
 auto modified=a;modified["blocks"][0][1]=22;
 rejects([&]{decode(record+encode(modified),"test","frozen");});
 rejects([&]{decode(record,"other","frozen");});rejects([&]{decode(record,"test","changed");});
 auto corrupt=Json::parse(record);corrupt["payload"]["blocks"][0][1]=22;
 rejects([&]{decode(corrupt.dump()+"\n","test","frozen");});
 rejects([&]{decode(record+"{broken}\n"+record,"test","frozen");});
 std::cout<<"PASS durable plan round-trip, all "<<record.size()<<" torn-tail offsets, duplicate identity, conflict, world/profile mismatch and interior corruption\n";
}
