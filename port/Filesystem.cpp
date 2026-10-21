#include "util/Filesystem.h"
#include <windows.h>
#include <algorithm>
namespace smw::filesystem {
static std::string native(const path& p) {
    auto s=p.string(); std::replace(s.begin(),s.end(), '/', '\\'); return s;
}
bool exists(const path& p) { return GetFileAttributesA(native(p).c_str()) != INVALID_FILE_ATTRIBUTES; }
bool is_directory(const path& p) {
    auto flags=GetFileAttributesA(native(p).c_str());
    return flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_DIRECTORY);
}
std::vector<directory_entry> list_directory(const path& p) {
    std::vector<directory_entry> entries;
    WIN32_FIND_DATAA data;
    HANDLE h=FindFirstFileA(native(p / "*").c_str(), &data);
    if (h==INVALID_HANDLE_VALUE) return entries;
    do {
        const std::string name=data.cFileName;
        if (name!="." && name!="..") entries.emplace_back(p/name, (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0);
    } while (FindNextFileA(h,&data));
    FindClose(h);
    std::sort(entries.begin(), entries.end(), [](const auto& a,const auto& b){return a.path()<b.path();});
    return entries;
}
}
