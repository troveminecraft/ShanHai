-- Supply the constructor used by our owned WorldBlockTarget subclasses.
-- Keep the pinned SDK's native fields and virtual slots. The old native
-- constructor copies WorldGenContext and enables simple writes (byte 0x60).
function shanhai_structure_sdk_overlay(target)
    if get_config("game_line") == "26.20" then return end
    local relative = "mc/world/level/WorldBlockTarget.h"
    local sdk = target:pkg("levilamina"):installdir()
    local source = io.readfile(path.join(sdk, "include", relative))
    local marker = "    WorldBlockTarget();"
    local first, last = source:find(marker, 1, true)
    assert(first and not source:find(marker, last + 1, true), "Unexpected WorldBlockTarget SDK declaration")
    local replacement = marker .. [[

    WorldBlockTarget(BlockSource& source, WorldGenContext const& context)
        : mBlockSource(source), mWorldGenContext(context), mBlockSimpleIsAllowed(true) {}
]]
    source = source:sub(1, first - 1) .. replacement .. source:sub(last + 1)
    local directory = path.join(target:autogendir(), "sdk-overlay-26.51.9")
    os.mkdir(path.directory(path.join(directory, relative)))
    io.writefile(path.join(directory, relative), source)
    -- These native constructors were inlined, but the factories and virtual
    -- layouts remain. Initialize owned feature instances with native values.
    local constructors = {
        RuinedPortalFeature = [[
    RuinedPortalFeature(uint seed, bool nether)
        : StructureFeature(seed, VanillaStructureFeatureType::RuinedPortal()),
          mNether(nether), mOverworldSpacing(40), mOverworldMinSeparation(15),
          mNetherSpacing(25), mNetherMinSeparation(10) {}
]],
        MineshaftFeature = [[
    explicit MineshaftFeature(uint seed)
        : StructureFeature(seed, VanillaStructureFeatureType::Mineshaft()) {}
]]
    }
    for name, body in pairs(constructors) do
        local file = "mc/world/level/levelgen/structure/" .. name .. ".h"
        local header = io.readfile(path.join(sdk, "include", file))
        local at = assert(header:find("public:", 1, true), "Unexpected feature SDK declaration")
        header = '#include "mc/world/level/levelgen/structure/VanillaStructureFeatureType.h"\n' ..
            header:sub(1, at + 6) .. "\n" .. body .. header:sub(at + 7)
        os.mkdir(path.directory(path.join(directory, file)))
        io.writefile(path.join(directory, file), header)
    end
    if target:name() == "karma-worldgen-client" then
    local relative = "mc/client/gui/ProgressHandler.h"
    local sdk = target:pkg("levilamina"):installdir()
    local source = io.readfile(path.join(sdk, "include", relative))
    local field = "::ll::TypedStorage<4, 4, ::ProgressHandlerType const> mType;"
    local first, last = source:find(field, 1, true)
    assert(first and not source:find(field, last + 1, true), "Unexpected ProgressHandler SDK declaration")
    source = source:gsub("virtual ::std::string getTitleText%(%) const;", "virtual ::std::string getTitleText() const { return {}; }")
    local replacement = field:sub(1, -2) .. " = ProgressHandlerType::Generic;"
    source = source:sub(1, first - 1) .. replacement .. source:sub(last + 1)
    local directory = path.join(target:autogendir(), "sdk-overlay-26.51.9")
    os.mkdir(path.directory(path.join(directory, relative)))
    io.writefile(path.join(directory, relative), source)
    end
    target:add("includedirs", directory)
end
