#pragma once
#include "BuildHeight.h"
#include "HookTransaction.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
class Level;
namespace ll::mod { class NativeMod; }
namespace ll::io { class Logger; }
// Windows orchestration talks to host adapters, never their mutable globals.
namespace terrain {
ll::io::Logger& log();
ll::mod::NativeMod& clientMod();
nlohmann::json jsonFile(std::filesystem::path const& path);
void clientPrepare(std::filesystem::path const& root, std::string const& name,
                   std::string const& seed, nlohmann::json const& marker);
void clientActivate();
void clientRelease();
void clientTick(Level& level);
void clientHooks(terrain_runtime::HookTransaction& hooks);
void clientUnhook();
terrain_runtime::BuildHeight clientHeight();
nlohmann::json clientStatus();
}
namespace lab {
void clientPrepare(std::filesystem::path const& root, std::string const& name,
                   std::string const& identity);
void clientActivate();
void clientRelease(Level& level);
void clientWarmTemplates(Level& level);
void clientTick(Level& level);
void clientHooks(terrain_runtime::HookTransaction& hooks);
void clientUnhook();
void clientVanillaPath(std::filesystem::path path);
nlohmann::json clientStatus();
}
