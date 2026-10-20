add_rules("mode.debug", "mode.release")
set_languages("c++20")
set_toolchains("clang-cl")
set_runtimes("MD")
add_requires("nlohmann_json v3.12.0")
add_includedirs("../src")
add_defines("NOMINMAX", "UNICODE")
add_undefines("NDEBUG")
add_cxflags("/EHsc", "/utf-8")
for _, name in ipairs({"structure_stage", "shared_structure_lifecycle",
                      "view_gap_recovery", "bds_structure_binding",
                      "world_scoped_cache"}) do
    target(name)
        set_kind("binary")
        add_packages("nlohmann_json")
        add_files(name .. ".cpp")
    target_end()
end
