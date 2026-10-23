// Void's methods are ICF stubs shared with unrelated classes/signatures. Patch
// the two virtual slots, never the shared machine-code bodies.
std::array<void**,2> voidGeneratorSlots{};
std::array<void*,2> voidGeneratorOriginal{};
bool voidGeneratorBound=false, baseLocateHook=false;
std::mutex voidGeneratorBindingMutex;
J generatorHookReport;
using VoidDecorationFunction=bool(*)(WorldGenerator*,ChunkViewSource&);
using VoidHeightFunction=void(*)(WorldGenerator*,BlockVolume&,ChunkPos const&,std::vector<short>*,bool);
bool voidDecorationDispatch(WorldGenerator *generator,ChunkViewSource &view) {
    auto origin=reinterpret_cast<VoidDecorationFunction>(voidGeneratorOriginal[0]);
    return worldgen_rules::afterStructureDecoration(automaticStructures && decorationGeneratorMatches(generator,view),
        structureStageFailed,[&]{return origin(generator,view);},[&]{return runAutomaticStage(*generator->mLevel,view);});
}
void voidHeightDispatch(WorldGenerator *generator,BlockVolume &volume,ChunkPos const &cp,std::vector<short> *heights,bool flag) {
    reinterpret_cast<VoidHeightFunction>(voidGeneratorOriginal[1])(generator,volume,cp,heights,flag);
    if(adaptHeightMap && heights && structureGeneratorMatches(generator)) *heights=terrainHeightMap(cp);
}
std::array<void*,2> voidGeneratorReplacements() {
    return {ll::memory::toFuncPtr(&voidDecorationDispatch),ll::memory::toFuncPtr(&voidHeightDispatch)};
}
void writeGeneratorSlot(void **slot,void *value) {
    ll::memory::modify(slot,sizeof(*slot),[&]{std::atomic_ref<void*>(*slot).store(value,std::memory_order_release);});
}
void saveGeneratorHookReport() {
    if(!folder.empty()) std::ofstream(folder/"generator-hooks.json")<<generatorHookReport.dump(2)<<'\n';
}
std::array<unsigned,2> voidGeneratorSlotIndices() {
    auto decode=[](auto member) {
        auto code=static_cast<uint8_t const*>(ll::memory::toFuncPtr(member));
        return worldgen_rules::generatorVirtualSlot(std::span<uint8_t const>(code,32));
    };
    auto decoration=decode(&VoidGenerator::decorationPostProcessChunk);
    auto heights=decode(&VoidGenerator::prepareHeights);
    if(!decoration || !heights || *decoration==*heights || *decoration>=128 || *heights>=128)
        throw std::runtime_error("Void generator virtual slot decoding failed");
    return {*decoration,*heights};
}
void bindVoidGenerator(WorldGenerator &generator) {
    std::lock_guard lock(voidGeneratorBindingMutex);
    auto indices=voidGeneratorSlotIndices();
    auto table=*reinterpret_cast<void***>(&generator);
    std::array<void**,2> slots{table+indices[0],table+indices[1]};
    if(voidGeneratorBound) {
        if(slots!=voidGeneratorSlots) throw std::runtime_error("Multiple incompatible Void generator vtables");
        return;
    }
    // This entry point is called only after VoidGenerator's native constructor
    // returned; the actual vtable supplies original native bodies, not import stubs.
    voidGeneratorOriginal={*slots[0],*slots[1]};
    worldgen_rules::replaceGeneratorSlots(slots,voidGeneratorOriginal,voidGeneratorReplacements(),writeGeneratorSlot);
    voidGeneratorSlots=slots;
    voidGeneratorBound=true;
    generatorHookReport["voidVtable"]={{"bound",true},{"decorationSlot",indices[0]},{"heightSlot",indices[1]},
        {"decorationOriginal",fmt::format("0x{:x}",reinterpret_cast<uintptr_t>(voidGeneratorOriginal[0]))},
        {"heightOriginal",fmt::format("0x{:x}",reinterpret_cast<uintptr_t>(voidGeneratorOriginal[1]))}};
    saveGeneratorHookReport();
    ll::mod::NativeMod::current()->getLogger().info("Void generator vtable slots bound: {}",generatorHookReport["voidVtable"].dump());
}
void restoreVoidGeneratorSlots() {
    std::lock_guard lock(voidGeneratorBindingMutex);
    if(voidGeneratorBound) worldgen_rules::restoreGeneratorSlots(voidGeneratorSlots,voidGeneratorOriginal,
        voidGeneratorReplacements(),writeGeneratorSlot);
    voidGeneratorBound=false;
    voidGeneratorSlots={};
}
LL_TYPE_INSTANCE_HOOK(VoidGeneratorConstruction,ll::memory::HookPriority::Normal,VoidGenerator,
                      &VoidGenerator::$ctor,void*,Dimension &dimension) {
    auto result=origin(dimension);
    bindVoidGenerator(*this);
    return result;
}
void installStructureGeneratorHooks(terrain_runtime::HookTransaction &hooks) {
    // Full startup installs this observer before any generator is constructed.
    // Import thunks can already point into other hooks during hot enable, and
    // Bedrock has no usable RTTI here. Require a restart instead of guessing.
    if(auto level=ll::service::getLevel()) if(auto dim=level->getDimension(0).lock())
        if(dim->getWorldGenerator())
            throw std::runtime_error("Structure generator already exists: fully restart before enabling ShanHai; unsafe hot enable is unsupported");
    auto flatLocate=ll::memory::unchecked(&FlatWorldGenerator::$findNearestStructureFeature);
    auto baseLocate=ll::memory::unchecked(&WorldGenerator::$findNearestStructureFeature);
    baseLocateHook=worldgen_rules::secondaryGeneratorHook(flatLocate,baseLocate);
    auto indices=voidGeneratorSlotIndices();
    generatorHookReport={{"scope","active-world primary-dimension exact decoration task"},{"sharedStage",true},
        {"addressNote","import thunks may point to hook chains; only bound vtable originals are native table entries"},
        {"flatDecorationImport",fmt::format("0x{:x}",ll::memory::unchecked(&FlatWorldGenerator::$decorationPostProcessChunk))},
        {"flatHeightImport",fmt::format("0x{:x}",ll::memory::unchecked(&FlatWorldGenerator::$prepareHeights))},
        {"locate",{{"flatImport",fmt::format("0x{:x}",flatLocate)},{"baseImport",fmt::format("0x{:x}",baseLocate)},{"baseHook",baseLocateHook}}},
        {"voidVtable",{{"bound",false},{"constructorHook",true},{"decorationSlot",indices[0]},{"heightSlot",indices[1]},
            {"decorationImport",fmt::format("0x{:x}",ll::memory::unchecked(&VoidGenerator::$decorationPostProcessChunk))},
            {"heightImport",fmt::format("0x{:x}",ll::memory::unchecked(&VoidGenerator::$prepareHeights))}}}};
    hooks.add<VillageHeightProbe>("VillageHeightProbe");
    hooks.add<AutoStage>("AutoStage");
    hooks.add<NativeLocate>("NativeLocate");
    if(baseLocateHook) hooks.add<BaseNativeLocate>("BaseNativeLocate");
    hooks.onRollback(restoreVoidGeneratorSlots);
    hooks.add<VoidGeneratorConstruction>("VoidGeneratorConstruction");
    saveGeneratorHookReport();
    ll::mod::NativeMod::current()->getLogger().info("Structure generator hook targets: {}",generatorHookReport.dump());
}
void uninstallStructureGeneratorHooks() {
    VoidGeneratorConstruction::unhook();
    restoreVoidGeneratorSlots();
    if(baseLocateHook) BaseNativeLocate::unhook();
    NativeLocate::unhook();
    AutoStage::unhook();
    VillageHeightProbe::unhook();
    baseLocateHook=false;
}
