#include "titus/process.hpp"
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <signal.h>
#endif
namespace titus {
void verify_profile_lock(const Json& request){auto text=std::getenv("TITUS_PROFILE_LOCK");if(!text||!*text)throw Error("E_PROFILE_LOCK","Runtime must inherit its profile lock");
    auto handle=std::stoll(text);auto profile=fs::u8path(request["paths"]["profile_directory"]["path"].get<std::string>());auto root=profile;for(int i=0;i<5;++i)root=root.parent_path();
    auto group=request["save_compatibility_id"].get<std::string>(),profile_name=profile.filename().u8string();
    if(profile_name!=group&&!(request["kind"]=="inspect_request"&&profile_name.rfind("."+group+".import-",0)==0))throw Error("E_PROFILE_LOCK","Profile compatibility directory mismatch");
    if(profile.parent_path().filename().u8string()!=request["game_id"]||profile.parent_path().parent_path().filename()!="games"||profile.parent_path().parent_path().parent_path().filename().u8string()!=request["profile_id"]||root/"profiles"!=profile.parent_path().parent_path().parent_path().parent_path())throw Error("E_PROFILE_LOCK","Invalid managed profile hierarchy");
    auto expected=root/"profile-locks"/fs::u8path(request["profile_id"].get<std::string>()+"."+request["game_id"].get<std::string>()+"."+request["save_compatibility_id"].get<std::string>()+".lock");
#ifdef _WIN32
    HANDLE native=reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));DWORD count=GetFinalPathNameByHandleW(native,nullptr,0,FILE_NAME_NORMALIZED);if(!count)throw Error("E_PROFILE_LOCK","Invalid inherited profile handle");
    std::wstring name(count,L'\0');auto n=GetFinalPathNameByHandleW(native,name.data(),count,FILE_NAME_NORMALIZED);if(!n||n>=count)throw Error("E_PROFILE_LOCK","Invalid inherited profile handle");name.resize(n);
    if(name.rfind(L"\\\\?\\UNC\\",0)==0)name=L"\\\\"+name.substr(8);else if(name.rfind(L"\\\\?\\",0)==0)name=name.substr(4);
    auto path=normalized_path(expected.parent_path())/expected.filename();if(_wcsicmp(path.c_str(),name.c_str()))throw Error("E_PROFILE_LOCK","Profile handle identity mismatch");
#else
    struct stat actual{},wanted{};if(fstat(static_cast<int>(handle),&actual)||stat(expected.c_str(),&wanted)||actual.st_dev!=wanted.st_dev||actual.st_ino!=wanted.st_ino||!S_ISREG(actual.st_mode))throw Error("E_PROFILE_LOCK","Profile descriptor identity mismatch");
#endif
}

std::string read_document(const fs::path& file,size_t limit){
    std::ifstream in(filesystem_path(file),std::ios::binary);if(!in)throw Error("E_READ","Cannot read "+file.u8string());
    std::string bytes;char buffer[8192];while(in){in.read(buffer,sizeof buffer);bytes.append(buffer,static_cast<size_t>(in.gcount()));if(bytes.size()>limit)throw Error("E_LIMIT","Document size limit exceeded");}
    if(!in.eof())throw Error("E_READ","Failed reading "+file.u8string());return bytes;
}
void write_document(const fs::path& file,const Json& value){
    auto temporary=file;temporary+=fs::u8path(".tmp."+uuid());auto bytes=value.dump(2)+"\n";
    try{std::ofstream out(filesystem_path(temporary),std::ios::binary);out.write(bytes.data(),bytes.size());out.flush();if(!out)throw Error("E_WRITE","Cannot write "+file.u8string());out.close();
#ifdef _WIN32
        if(!MoveFileExW(filesystem_path(temporary).c_str(),filesystem_path(file).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw Error("E_WRITE","Cannot replace "+file.u8string());
#else
        fs::rename(temporary,file);
#endif
    }catch(...){std::error_code ec;fs::remove(temporary,ec);throw;}
}
struct ProfileLock::Impl {
#ifdef _WIN32
    HANDLE handle=INVALID_HANDLE_VALUE;
#else
    int handle=-1;
#endif
};
ProfileLock::ProfileLock(const fs::path& file):impl(new Impl){ensure_directory(file.parent_path());
#ifdef _WIN32
    SECURITY_ATTRIBUTES attr{sizeof attr,nullptr,TRUE};impl->handle=CreateFileW(filesystem_path(file).c_str(),GENERIC_READ|GENERIC_WRITE,0,&attr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(impl->handle==INVALID_HANDLE_VALUE)throw Error("E_PROFILE_BUSY","Este perfil ya está en uso o no se puede bloquear.");
#else
    impl->handle=open(file.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(impl->handle<0)throw Error("E_PROFILE_BUSY","Cannot open profile lock");
    if(flock(impl->handle,LOCK_EX|LOCK_NB)!=0){close(impl->handle);impl->handle=-1;throw Error("E_PROFILE_BUSY","Este perfil ya está en uso.");}
#endif
}
ProfileLock::~ProfileLock(){
#ifdef _WIN32
    if(impl->handle!=INVALID_HANDLE_VALUE)CloseHandle(impl->handle);
#else
    if(impl->handle>=0)close(impl->handle);
#endif
}
intptr_t ProfileLock::native_handle()const{
#ifdef _WIN32
 return reinterpret_cast<intptr_t>(impl->handle);
#else
 return static_cast<intptr_t>(impl->handle);
#endif
}
struct Process::Impl {
#ifdef _WIN32
    HANDLE process=nullptr;
#else
    pid_t pid=-1;
#endif
    bool started=false,exited=false;int code=-1;
};
Process::Process():impl(new Impl){}Process::~Process(){
#ifdef _WIN32
    if(impl->process)CloseHandle(impl->process);
#else
    if(impl->started&&!impl->exited){int status;waitpid(impl->pid,&status,WNOHANG);}
#endif
}
bool Process::started()const{return impl->started;}int Process::exit_code()const{return impl->code;}
#ifdef _WIN32
static std::wstring quote(const std::wstring& arg){std::wstring result=L"\"";size_t slashes=0;
    for(auto c:arg){if(c==L'\\'){++slashes;continue;}if(c==L'"')result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');slashes=0;result+=c;}
    result.append(slashes*2,L'\\');return result+L'"';}
#endif
void Process::start(const fs::path& executable,const std::vector<std::string>& args,const fs::path& cwd,
 const std::map<std::string,std::string>& overrides,const fs::path& logs,const ProfileLock& lock){
    if(impl->started)throw Error("E_PROCESS","Process already started");ensure_directory(logs);
#ifdef _WIN32
    std::wstring command=quote(executable.native());for(const auto& a:args)command+=L" "+quote(fs::u8path(a).native());
    struct Less{bool operator()(const std::wstring&a,const std::wstring&b)const{return _wcsicmp(a.c_str(),b.c_str())<0;}};
    std::map<std::wstring,std::wstring,Less> environment;auto block=GetEnvironmentStringsW();if(!block)throw Error("E_PROCESS","Cannot read environment");
    for(auto p=block;*p;p+=wcslen(p)+1){std::wstring item(p);auto equal=item.find(L'=',item[0]==L'='?1:0);if(equal!=std::wstring::npos)environment[item.substr(0,equal)]=item.substr(equal+1);}FreeEnvironmentStringsW(block);
    for(const auto& pair:overrides)environment[fs::u8path(pair.first).native()]=fs::u8path(pair.second).native();
    environment[L"TITUS_PROFILE_LOCK"]=std::to_wstring(lock.native_handle());
    std::vector<wchar_t> variables;for(const auto& p:environment){auto item=p.first+L"="+p.second;variables.insert(variables.end(),item.begin(),item.end());variables.push_back(0);}variables.push_back(0);
    SECURITY_ATTRIBUTES attr{sizeof attr,nullptr,TRUE};
    HANDLE output=CreateFileW(filesystem_path(logs/"stdout.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&attr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    HANDLE error=CreateFileW(filesystem_path(logs/"stderr.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&attr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&attr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(output==INVALID_HANDLE_VALUE||error==INVALID_HANDLE_VALUE||input==INVALID_HANDLE_VALUE){if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);if(error!=INVALID_HANDLE_VALUE)CloseHandle(error);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);throw Error("E_PROCESS","Cannot open process logs");}
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof startup;startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;startup.StartupInfo.hStdOutput=output;startup.StartupInfo.hStdError=error;startup.StartupInfo.hStdInput=input;
    SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);std::vector<char> attributes(size);startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    bool initialized=InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&size);HANDLE inherited[]={output,error,input,reinterpret_cast<HANDLE>(lock.native_handle())};
    bool updated=initialized&&UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof inherited,nullptr,nullptr);
    PROCESS_INFORMATION info{};BOOL ok=updated&&CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_UNICODE_ENVIRONMENT|CREATE_NO_WINDOW,variables.data(),cwd.c_str(),&startup.StartupInfo,&info);
    if(initialized)DeleteProcThreadAttributeList(startup.lpAttributeList);CloseHandle(input);CloseHandle(output);CloseHandle(error);
    if(!ok)throw Error("E_PROCESS","Cannot start installed runtime");CloseHandle(info.hThread);impl->process=info.hProcess;
#else
    // Build argv/env before fork; the child only performs async-signal-safe calls.
    std::map<std::string,std::string> environment;
    for(char** p=::environ;*p;++p){std::string item(*p);auto equal=item.find('=');if(equal!=std::string::npos)environment[item.substr(0,equal)]=item.substr(equal+1);}
    for(const auto& pair:overrides)environment[pair.first]=pair.second;environment["TITUS_PROFILE_LOCK"]="198";
    std::vector<std::string> values;for(const auto& p:environment)values.push_back(p.first+"="+p.second);
    std::vector<char*> env;for(auto& v:values)env.push_back(v.data());env.push_back(nullptr);
    std::vector<std::string> arguments{executable.string()};arguments.insert(arguments.end(),args.begin(),args.end());std::vector<char*> argv;for(auto& a:arguments)argv.push_back(a.data());argv.push_back(nullptr);
    int output=open((logs/"stdout.log").c_str(),O_CREAT|O_TRUNC|O_WRONLY|O_CLOEXEC,0600),error=open((logs/"stderr.log").c_str(),O_CREAT|O_TRUNC|O_WRONLY|O_CLOEXEC,0600);
    if(output<0||error<0){if(output>=0)close(output);if(error>=0)close(error);throw Error("E_PROCESS","Cannot open process logs");}
    auto pid=fork();if(pid==0){if(dup2(output,STDOUT_FILENO)<0||dup2(error,STDERR_FILENO)<0||dup2(static_cast<int>(lock.native_handle()),198)<0||chdir(cwd.c_str())!=0)_exit(126);execve(executable.c_str(),argv.data(),env.data());_exit(127);}
    close(output);close(error);if(pid<0)throw Error("E_PROCESS","Cannot fork runtime");impl->pid=pid;
#endif
    impl->started=true;
}
void Process::terminate(){if(!impl->started||poll())return;
#ifdef _WIN32
    if(!TerminateProcess(impl->process,137))throw Error("E_PROCESS","Cannot terminate runtime");
#else
    if(kill(impl->pid,SIGKILL)!=0&&errno!=ESRCH)throw Error("E_PROCESS","Cannot terminate runtime");
#endif
}
bool Process::poll(){if(!impl->started)return false;if(impl->exited)return true;
#ifdef _WIN32
    if(WaitForSingleObject(impl->process,0)!=WAIT_OBJECT_0)return false;DWORD code;GetExitCodeProcess(impl->process,&code);impl->code=static_cast<int>(code);
#else
    int status;auto result=waitpid(impl->pid,&status,WNOHANG);if(result==0)return false;if(result<0){if(errno==EINTR)return false;impl->code=-1;}else impl->code=WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);
#endif
    impl->exited=true;return true;
}
}
