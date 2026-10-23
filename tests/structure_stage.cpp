#include "rules/StructureStage.h"
#include <iostream>
#include <thread>
#include <vector>
#include <map>

struct Chunk { int x, z; };
struct View {
    std::map<std::pair<int,int>,Chunk> chunks;
    // A random availability query need not expose a processing chunk.
    Chunk *randomAvailableChunk() const { return nullptr; }
    Chunk *lookup(int x,int z) { auto it=chunks.find({x,z}); return it==chunks.end()?nullptr:&it->second; }
};
using Task = worldgen_rules::DecorationTaskScope<View,Chunk>;
Chunk &ownerFor(View &view) {
    return worldgen_rules::decorationTaskOwner<View,Chunk>(view,
        [](Chunk const &c) { return std::pair{c.x,c.z}; },
        [&](int x,int z) { return view.lookup(x,z); });
}

void check(bool ok, char const *reason) {
    if (!ok)
        throw std::runtime_error(reason);
}
int main() {
    try {
        auto rejected=[](auto action) { try {action();} catch(std::runtime_error const&) {return true;} return false; };
        std::array<uint8_t,13> virtualShort{0x48,0x8b,0x01,0x48,0x8b,0x40,0x50,0x48,0xff,0xe0};
        std::array<uint8_t,13> virtualLong{0x48,0x8b,0x01,0x48,0x8b,0x80,0x68,0x01,0,0,0x48,0xff,0xe0};
        check(worldgen_rules::generatorVirtualSlot(virtualShort)==10,"SDK disp8 virtual thunk decoding failed");
        check(worldgen_rules::generatorVirtualSlot(virtualLong)==45,"SDK disp32 virtual thunk decoding failed");
        std::array<uint8_t,24> debugThunk{0x50,0x48,0x89,0x0c,0x24,0x48,0x8b,0x0c,0x24,
            0x48,0x8b,0x01,0x48,0x8b,0x80,0x68,0x01,0,0,0x41,0x5a,0x48,0xff,0xe0};
        check(worldgen_rules::generatorVirtualSlot(debugThunk)==45,"SDK debug virtual thunk decoding failed");
        debugThunk[20]=0x58;
        check(!worldgen_rules::generatorVirtualSlot(debugThunk),"unknown debug epilogue accepted");
        virtualShort[6]=0x70;
        check(worldgen_rules::generatorVirtualSlot(virtualShort)==14,"SDK load virtual thunk decoding failed");
        virtualShort[6]=0x78;
        check(worldgen_rules::generatorVirtualSlot(virtualShort)==15,"virtual decoder hardcoded known slot");
        virtualShort[6]=0xf8;
        check(!worldgen_rules::generatorVirtualSlot(virtualShort),"negative displacement accepted");
        virtualLong[6]=0x69;
        check(!worldgen_rules::generatorVirtualSlot(virtualLong),"unaligned virtual offset accepted");
        virtualLong[6]=0x68;virtualLong[12]=0xc3;
        check(!worldgen_rules::generatorVirtualSlot(virtualLong),"unrecognized thunk accepted");
        check(!worldgen_rules::secondaryGeneratorHook(0x1000,0x1000),"folded hook installed twice");
        check(worldgen_rules::secondaryGeneratorHook(0x1000,0x2000),"independent hook skipped");
        check(rejected([&]{worldgen_rules::secondaryGeneratorHook(0,0x2000);}),"missing hook target accepted");
        int originalCalls=0,stageCalls=0;
        auto origin=[&]{++originalCalls;return true;};
        auto stage=[&]{++stageCalls;return true;};
        for(int generator=0;generator<2;++generator)
            check(worldgen_rules::afterStructureDecoration(true,false,origin,stage),"generator stage failed");
        check(originalCalls==2&&stageCalls==2,"Flat/Void did not each execute one shared stage");
        check(worldgen_rules::afterStructureDecoration(false,true,origin,stage),"other dimension intercepted");
        check(originalCalls==3&&stageCalls==2,"inactive generator reached stage");
        check(!worldgen_rules::afterStructureDecoration(true,true,origin,stage)&&originalCalls==3,"failed stage retried origin");
        check(!worldgen_rules::afterStructureDecoration(true,false,[]{return false;},stage)&&stageCalls==2,"failed native decoration still placed structures");
        check(!worldgen_rules::afterStructureDecoration(true,false,origin,[]{return false;}),"shared stage failure lost");
        // Identical native bodies still have separate vtable slots; unrelated
        // slots must never be intercepted. Restore on failure and on unload.
        int native,decoration,height,other;
        std::array<void*,3> table{&native,&native,&native};
        std::array<void**,2> slots{&table[0],&table[1]};
        std::array<void*,2> expected{&native,&native},replacement{&decoration,&height};
        auto write=[](void**slot,void*value){*slot=value;};
        worldgen_rules::replaceGeneratorSlots(slots,expected,replacement,write);
        check(table[0]==&decoration&&table[1]==&height&&table[2]==&native,"folded vtable dispatch touched unrelated slot");
        worldgen_rules::restoreGeneratorSlots(slots,expected,replacement,write);
        check(table[0]==&native&&table[1]==&native,"vtable unload failed");
        int writes=0;
        check(rejected([&]{worldgen_rules::replaceGeneratorSlots(slots,expected,replacement,[&](void**slot,void*value){
            if(++writes==2)throw std::runtime_error("write failed");*slot=value;});}),"failed slot write accepted");
        check(table[0]==&native&&table[1]==&native,"partial vtable install not rolled back");
        table[1]=&other;
        check(rejected([&]{worldgen_rules::replaceGeneratorSlots(slots,expected,replacement,write);}),"unknown native slot accepted");
        check(table[0]==&native&&table[1]==&other,"validation mutated native table");
        table[0]=&decoration;
        worldgen_rules::restoreGeneratorSlots(slots,expected,replacement,write);
        check(table[0]==&native&&table[1]==&other,"unload overwrote another owner's slot");
        View view;
        for(int z=-5;z<=-3;++z)for(int x=-7;x<=-5;++x)view.chunks.emplace(std::pair{x,z},Chunk{x,z});
        auto &owner=*view.lookup(-6,-4);
        check(view.randomAvailableChunk()==nullptr,"fixture must reproduce unavailable random probe");
        check(rejected([&]{ownerFor(view);}),"missing task accepted");
        check(Task::ownerIf(view)==nullptr,"missing task leaked folded-call context");
        {
            Task scope(view,owner);
            check(&ownerFor(view)==&owner,"explicit processing owner lost when random probe is empty");
            View other;
            check(Task::ownerIf(view)==&owner&&Task::ownerIf(other)==nullptr,"folded-call view guard failed");
            check(rejected([&]{ownerFor(other);}),"wrong view accepted task context");
            auto nested=[&]{Task inner(other,owner);throw std::runtime_error("unwind");};
            check(rejected(nested),"nested exception missing");
            check(&ownerFor(view)==&owner,"nested exception did not restore task");
            bool isolated=false;
            std::thread worker([&]{isolated=rejected([&]{Task::ownerFor(view);});});worker.join();
            check(isolated,"task context leaked across workers");
            auto saved=view.chunks.extract({-7,-5});
            check(rejected([&]{ownerFor(view);}),"incomplete neighbourhood accepted");
            view.chunks.insert(std::move(saved));
            Chunk impostor{-6,-4};
            {Task wrong(view,impostor);check(rejected([&]{ownerFor(view);}),"wrong owner identity accepted");}
        }
        check(rejected([&]{ownerFor(view);}),"completed task context leaked");
        // Every possible native probe, including negative chunk coordinates.
        for (auto [cx, cz] : {std::pair{0, 0}, {-300, 70}, {1000000, -1000000}}) {
            for (int z = cz - 1; z <= cz + 1; ++z)
                for (int x = cx - 1; x <= cx + 1; ++x)
                    check(worldgen_rules::decorationCenter(x, z,
                                                           [&](int px, int pz) {
                                                               return px >= cx - 1 && px <= cx + 1 &&
                                                                      pz >= cz - 1 && pz <= cz + 1;
                                                           }) == std::pair{cx, cz},
                          "wrong decoration owner");
        }
        for (int missing = 0; missing < 9; ++missing) {
            bool failed = false;
            try {
                worldgen_rules::decorationCenter(0, 0, [&](int x, int z) {
                    return x >= -1 && x <= 1 && z >= -1 && z <= 1 && (z + 1) * 3 + x + 1 != missing;
                });
            } catch (std::runtime_error const &) {
                failed = true;
            }
            check(failed, "incomplete view allowed structure writes");
        }
        worldgen_rules::BoundedHints<std::pair<int, int>, 64> hints;
        for (int i = 0; i < 100000; ++i) {
            hints.insert({i, -i});
            hints.insert({i, -i});
            check(hints.size() <= 64, "hint cache leaked");
            if (i % 3 == 0)
                hints.erase({i, -i});
        }
        hints.clear();
        hints.insert({1, 2});
        hints.erase({1, 2});
        hints.insert({1, 2});
        for (int i = 0; i < 63; ++i)
            hints.insert({3, i});
        check(hints.contains({1, 2}), "stale FIFO entry evicted a newer hint");
        worldgen_rules::StructureStageMutex mutex;
        int counter = 0;
        std::vector<std::thread> workers;
        for (int i = 0; i < 4; ++i)
            workers.emplace_back([&] {
                for (int j = 0; j < 10000; ++j) {
                    std::lock_guard lock(mutex);
                    ++counter;
                }
            });
        for (auto &worker : workers)
            worker.join();
        check(counter == 40000 && mutex.acquisitions == 40000, "native lock lost synchronization");
        std::cout << "structure stage: Flat/Void dispatch, SDK thunk decoding, folded slot isolation/rollback, ownership, bounded hints and lock accounting passed\n";
        return 0;
    } catch (std::exception const &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
