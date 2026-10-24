set_targetdir(path.absolute(path.join(get_config("builddir") or "build", "$(plat)/$(arch)/$(mode)"), os.projectdir()))
add_rules("mode.debug", "mode.release")
set_languages("c++20")
set_toolchains("clang-cl")
set_runtimes("MD")
add_requires("nlohmann_json v3.12.0")
add_includedirs("../src")
add_defines("NOMINMAX", "UNICODE")
add_undefines("NDEBUG")
add_cxflags("/EHsc", "/utf-8", "/fp:precise", "/clang:-ffp-contract=off", "/arch:AVX2")
for _, name in ipairs({"terrain_revision", "badlands_regions", "build_height", "structure_height", "vegetation_columns", "mountain_height", "sea_level", "raised_terrain", "test_teleport_policy", "completed_biomes", "regional_waterways", "failure_record", "creation_status", "kernel_determinism", "kernel_bounds", "palette_contract", "structure_stage", "structure_plan_store", "structure_prefill_workers", "hook_transaction", "shared_structure_lifecycle",
                      "view_gap_recovery", "bds_structure_binding",
                      "world_scoped_cache"}) do
    target(name)
        set_kind("binary")
        add_packages("nlohmann_json")
        add_files(name .. ".cpp")
        if name == "build_height" then add_syslinks("bcrypt") end
    target_end()
end
