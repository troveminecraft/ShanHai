// Native records contain process-local pointers. Persist only decisions that
// must agree when a blueprint is reconstructed; never persist pointers or
// mark unsaved chunk fragments complete.
void checkpointStructure(StructureStart const &start, std::string const &family, J details) {
    if (auditPlanningStarts)
        return;
    J pieces = J::array();
    for (auto const &piece : *start.pieces)
        pieces.push_back({{"type", int(piece->getType())}, {"bounds", boxJson(*piece->mBoundingBox)}});
    J value = {{"schema", 1},
               {"world", worldIdentity()},
               {"terrainSeed", worldgen_rules::structureLedgerSeed(input)},
               {"rules", input.at("config").value("sharedRules", J::object())},
               {"family", family},
               {"anchor", {int(start.chunkX), int(start.chunkZ)}},
               {"bounds", boxJson(*start.boundingBox)},
               {"pieces", std::move(pieces)},
               {"details", std::move(details)}};
    auto file = worldgen_rules::plan_store::checkpointPath(worldFolder(), family, start.chunkX, start.chunkZ);
    std::filesystem::create_directories(file.parent_path());
    worldgen_rules::plan_store::verifyOrCreate(file, value);
}
