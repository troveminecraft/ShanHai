#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#include <chrono>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace worldgen_persistence {
using Json=nlohmann::json;
inline std::string label(std::filesystem::path const&p){auto s=p.u8string();return {s.begin(),s.end()};}
inline Json read(std::filesystem::path const&p){
 std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot open JSON: "+label(p));
 try{return Json::parse(f);}catch(std::exception const&e){throw std::runtime_error("Invalid JSON: "+label(p)+": "+e.what());}
}
// Callers serialize writes to a destination. Flush the file and its rename
// before a caller is allowed to perform a non-repeatable game side effect.
inline void atomicWrite(std::filesystem::path const&dst,Json const&value){
 auto tmp=dst;tmp+=".tmp";auto text=value.dump(2);
#ifdef _WIN32
 HANDLE h=CreateFileW(tmp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot write ledger: "+label(tmp));
 DWORD n=0;bool ok=text.size()<=MAXDWORD&&WriteFile(h,text.data(),DWORD(text.size()),&n,nullptr)&&n==text.size()&&FlushFileBuffers(h);
 CloseHandle(h);if(!ok)throw std::runtime_error("Cannot flush ledger: "+label(tmp));
 if(!MoveFileExW(tmp.c_str(),dst.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit ledger: "+label(dst));
#else
 int fd=::open(tmp.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
 if(fd<0)throw std::runtime_error("Cannot write ledger: "+label(tmp));
 bool ok=true;size_t offset=0;
 while(offset<text.size()){auto n=::write(fd,text.data()+offset,text.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}offset+=size_t(n);}
 if(ok)ok=::fsync(fd)==0;int closed=::close(fd);if(!ok||closed)throw std::runtime_error("Cannot flush ledger: "+label(tmp));
 if(::rename(tmp.c_str(),dst.c_str()))throw std::runtime_error("Cannot commit ledger: "+label(dst));
 auto parent=dst.parent_path();if(parent.empty())parent=".";
 int directory=::open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(directory<0)throw std::runtime_error("Cannot open ledger directory: "+label(parent));
 ok=::fsync(directory)==0;closed=::close(directory);if(!ok||closed)throw std::runtime_error("Cannot flush ledger directory: "+label(parent));
#endif
}
inline uint64_t diagnosticStamp(){return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
}
