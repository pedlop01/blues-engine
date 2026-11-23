// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace titus_old {
inline std::filesystem::path report_root;
inline std::filesystem::path io_path(const std::filesystem::path& path){
#ifdef _WIN32
    auto s=path.native();for(auto& c:s)if(c==L'/')c=L'\\';
    if(s.rfind(L"\\\\?\\",0)==0)return std::filesystem::path(s);
    if(s.rfind(L"\\\\",0)==0)return std::filesystem::path(L"\\\\?\\UNC\\"+s.substr(2));
    if(s.size()>2&&s[1]==L':'&&s[2]==L'\\')return std::filesystem::path(L"\\\\?\\"+s);
#endif
    return path;
}
inline bool io_file(const std::filesystem::path& path){
#ifdef _WIN32
    DWORD a=GetFileAttributesW(io_path(path).c_str());return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);
#else
    return std::filesystem::is_regular_file(path);
#endif
}
inline void io_mkdir(const std::filesystem::path& path,std::error_code& ec){
#ifdef _WIN32
    if(CreateDirectoryW(io_path(path).c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS)ec.clear();else ec=std::error_code(GetLastError(),std::system_category());
#else
    std::filesystem::create_directories(path,ec);
#endif
}

}
