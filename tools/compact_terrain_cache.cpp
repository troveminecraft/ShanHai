#include "DerivedCache.h"
#include <tlhelp32.h>
#include <nlohmann/json.hpp>
#include <iostream>
bool gameRunning(){
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot check Minecraft processes");
 PROCESSENTRY32W p{};p.dwSize=sizeof(p);bool running=false;
 if(!Process32FirstW(snapshot,&p)){CloseHandle(snapshot);throw std::runtime_error("Cannot enumerate processes");}
 do{if(!_wcsicmp(p.szExeFile,L"Minecraft.Windows.exe")||!_wcsicmp(p.szExeFile,L"Minecraft.exe"))running=true;}while(Process32NextW(snapshot,&p));
 CloseHandle(snapshot);return running;
}
int wmain(int argc,wchar_t**argv){try{
 if(argc!=3)throw std::runtime_error("Usage: native-cache-compact <world/karma/cache> <new receipt.json>");
 auto root=std::filesystem::absolute(argv[1]).lexically_normal(),receipt=std::filesystem::path(argv[2]);
 if(root.filename()!=L"cache"||root.parent_path().filename()!=L"karma"||std::filesystem::is_symlink(std::filesystem::symlink_status(root))||!std::filesystem::is_directory(root)||std::filesystem::exists(receipt))throw std::runtime_error("Expected existing terrain cache and new receipt path");
 if(gameRunning())throw std::runtime_error("Close Minecraft before compacting its derived cache");
 nlohmann::json records=nlohmann::json::array();uint64_t before=0,after=0,changed=0,invalid=0;
 for(auto const&dir:std::filesystem::directory_iterator(root)){
  if(!std::filesystem::is_directory(dir.symlink_status()))continue;auto id=dir.path().filename().string();
  if(id.size()!=64||!std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))continue;
  for(auto const&entry:std::filesystem::directory_iterator(dir.path())){
   if(!std::filesystem::is_regular_file(entry.symlink_status())||entry.path().extension()!=L".bin")continue;
   auto name=entry.path().stem().string();if(name.empty()||!std::all_of(name.begin(),name.end(),[](char c){return c>='0'&&c<='9';}))continue;
   if(records.size()%256==0&&gameRunning())throw std::runtime_error("Minecraft started; compaction stopped between files");
   auto size=entry.file_size();auto read=terrain_cache::readDerived(entry.path());before+=size;
   if(read.state!=terrain_cache::ReadState::Valid){after+=size;++invalid;continue;}
   auto digest=terrain_cache::digest(read.bytes);
   if(read.legacy){terrain_cache::writeDerived(entry.path(),read.bytes);auto verified=terrain_cache::readDerived(entry.path());if(verified.state!=terrain_cache::ReadState::Valid||verified.bytes!=read.bytes)throw std::runtime_error("Cache roundtrip mismatch");++changed;}
   auto actual=std::filesystem::file_size(entry.path());after+=actual;
   records.push_back({{"file",id+"/"+entry.path().filename().string()},{"sha256",digest},{"before",size},{"after",actual}});
   if(records.size()%5000==0)std::cout<<records.size()<<" cache files verified\n"<<std::flush;
  }
 }
 nlohmann::json summary={{"beforeBytes",before},{"afterBytes",after},{"verified",records.size()},{"changed",changed},{"invalidUntouched",invalid},{"worldDatabaseModified",false}};
 std::cout<<summary<<std::endl;summary["files"]=std::move(records);std::ofstream out(receipt);out<<summary.dump();if(!out)throw std::runtime_error("Could not write receipt");
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
