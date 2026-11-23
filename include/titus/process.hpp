#pragma once
#include "titus/catalog.hpp"
#include <memory>
#include <map>
namespace titus {
void verify_profile_lock(const Json& request);
class ProfileLock {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    explicit ProfileLock(const fs::path& file);
    ~ProfileLock();
    intptr_t native_handle() const;
};
class Process {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    Process();~Process();
    Process(const Process&)=delete;Process& operator=(const Process&)=delete;
    void start(const fs::path& executable,const std::vector<std::string>& args,
        const fs::path& cwd,const std::map<std::string,std::string>& env,
        const fs::path& log_directory,const ProfileLock& lock);
    bool poll(); // true once the actual OS process has exited
    void terminate(); // Explicit caller decision only; never invoked by destruction.
    int exit_code() const;
    bool started() const;
};
std::string read_document(const fs::path& file,size_t limit=1024*1024);
void write_document(const fs::path& file,const Json& value);
}
