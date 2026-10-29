#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <mutex>
#include <atomic>
namespace terrain_runtime {
class DeferredDiagnosticBatch {
public:
    inline static thread_local DeferredDiagnosticBatch* current=nullptr;
    inline static std::mutex outputMutex;
    inline static std::atomic_uint64_t failures=0,dropped=0;
    DeferredDiagnosticBatch* previous=current;
    std::vector<std::pair<std::filesystem::path,std::string>> rows;
    size_t bytes=0;
    DeferredDiagnosticBatch(){current=this;}
    DeferredDiagnosticBatch(DeferredDiagnosticBatch const&)=delete;
    ~DeferredDiagnosticBatch()noexcept {
        current=previous;
        for(auto& [path,text]:rows)write(path,text);
    }
    static void write(std::filesystem::path const&path,std::string const&text)noexcept {
        try {std::lock_guard guard(outputMutex);std::ofstream out(path,std::ios::app);out<<text;if(!out)++failures;}
        catch(...){++failures;}
    }
    static void append(std::filesystem::path path,std::string text)noexcept {
        try {
            if(auto* batch=current){
                if(batch->rows.size()>=128||batch->bytes+text.size()>1024*1024){++dropped;return;}
                batch->bytes+=text.size();batch->rows.emplace_back(std::move(path),std::move(text));
            }else write(path,text);
        }catch(...){++failures;}
    }
};
class DeferredDiagnosticLine:public std::ostringstream {
    std::filesystem::path path;
public:
    explicit DeferredDiagnosticLine(std::filesystem::path value):path(std::move(value)){}
    ~DeferredDiagnosticLine()noexcept {try{DeferredDiagnosticBatch::append(std::move(path),str());}catch(...){++DeferredDiagnosticBatch::failures;}}
};
}
