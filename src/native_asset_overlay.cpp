#include "native_asset_overlay.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace scarlett::overlay {
namespace {
constexpr std::size_t MaxPathBytes=240;
constexpr std::size_t MaxComponentBytes=80;
char lowercase(char c) noexcept {return c>='A'&&c<='Z'?static_cast<char>(c+('a'-'A')):c;}
bool reserved_device(std::string_view word) noexcept {
    const auto period=word.find('.');
    if(period!=std::string_view::npos)word=word.substr(0,period);
    if(word=="con"||word=="nul"||word=="aux"||word=="prn")return true;
    return word.size()==4 &&
        ((word.substr(0,3)=="com")||(word.substr(0,3)=="lpt")) &&
        word[3]>='1' && word[3]<='9';
}
bool accepted_char(char c) noexcept {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
           (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c==' ';
}
// No implicit path normalization of dot segments or absolute/drive paths:
// rejecting them prevents aliases and traversal. ASCII-only first milestone.
Status inspect_path(std::string_view value,std::string& clean,std::string& key) {
    if(value.empty()||value.size()>MaxPathBytes)return Status::InvalidPath;
    clean.clear();key.clear();
    std::size_t begin=0;
    while(begin<value.size()) {
        std::size_t end=begin;
        while(end<value.size() && value[end]!='/' && value[end]!='\\')++end;
        const auto segment=value.substr(begin,end-begin);
        if(segment.empty()||segment.size()>MaxComponentBytes||
           segment=="."||segment==".."||segment.front()==' '||
           segment.back()=='.'||segment.back()==' ')return Status::InvalidPath;
        std::string folded;
        folded.reserve(segment.size());
        for(const char c:segment) {
            if(!accepted_char(c))return Status::InvalidPath;
            folded.push_back(lowercase(c));
        }
        if(reserved_device(folded))return Status::InvalidPath;
        if(!clean.empty()){clean.push_back('/');key.push_back('/');}
        clean.append(segment.data(),segment.size());
        key+=folded;
        if(end==value.size())break;
        begin=end+1;
        if(begin==value.size())return Status::InvalidPath;
    }
    return Status::Ok;
}
bool executable_or_plugin(std::string_view key) noexcept {
    const auto last_slash=key.find_last_of('/');
    const auto last_dot=key.find_last_of('.');
    if(last_dot==std::string_view::npos ||
       (last_slash!=std::string_view::npos && last_dot<last_slash))return false;
    const auto ext=key.substr(last_dot);
    constexpr std::array<std::string_view,10> Blocked{{
        ".exe",".dll",".asi",".so",".dylib",".bat",".cmd",
        ".ps1",".com",".scr"}};
    return std::find(Blocked.begin(),Blocked.end(),ext)!=Blocked.end();
}
// Checks are planning-time integrity checks, NOT a race-free openat2/handle
// containment guarantee if an untrusted process modifies the tree afterward.
Status plain_file_beneath(const std::filesystem::path& root,
                          const std::string& relative,
                          std::filesystem::path& out) {
    std::error_code ec;
    const auto root_status=std::filesystem::symlink_status(root,ec);
    if(ec||!std::filesystem::exists(root_status))return Status::SourceMissing;
    if(std::filesystem::is_symlink(root_status))return Status::SymlinkRejected;
    if(!std::filesystem::is_directory(root_status))return Status::NotRegularFile;
    const auto canonical_root=std::filesystem::canonical(root,ec);
    if(ec)return Status::SourceMissing;
    auto next=root;
    const std::filesystem::path parts(relative);
    for(const auto& part:parts){
        next/=part;
        const auto st=std::filesystem::symlink_status(next,ec);
        if(ec||!std::filesystem::exists(st))return Status::SourceMissing;
        if(std::filesystem::is_symlink(st))return Status::SymlinkRejected;
    }
    const auto final_status=std::filesystem::symlink_status(next,ec);
    if(ec||!std::filesystem::is_regular_file(final_status))
        return Status::NotRegularFile;
    const auto canonical_file=std::filesystem::canonical(next,ec);
    if(ec)return Status::SourceMissing;
    auto begin=canonical_file.begin();
    for(const auto& part:canonical_root){
        if(begin==canonical_file.end()||*begin!=part)return Status::OutsideRoot;
        ++begin;
    }
    if(begin==canonical_file.end())return Status::NotRegularFile;
    out=canonical_file;
    return Status::Ok;
}
} // namespace

Catalog::Catalog(std::filesystem::path original_root,
                 std::filesystem::path override_root)
    :original_root_(std::move(original_root)),
     override_root_(std::move(override_root)){}
Status Catalog::add(std::string_view virtual_path,std::string_view source_relative){
    std::string clean,key,source_clean,source_key;
    const auto target_status=inspect_path(virtual_path,clean,key);
    if(target_status!=Status::Ok)return target_status;
    const auto source_status=inspect_path(source_relative,source_clean,source_key);
    if(source_status!=Status::Ok)return source_status;
    if(executable_or_plugin(key)||executable_or_plugin(source_key))
        return Status::ExecutablePath;
    if(entries_.size()==MaxEntries)return Status::CapacityReached;
    std::filesystem::path verified;
    const auto st=plain_file_beneath(override_root_,source_clean,verified);
    if(st!=Status::Ok)return st;
    entries_.push_back({std::move(key),std::move(source_clean)});
    return Status::Ok;
}
Result Catalog::resolve(std::string_view virtual_path) const {
    std::string clean,key;
    const auto st=inspect_path(virtual_path,clean,key);
    if(st!=Status::Ok)return {st,Origin::Original,{}};
    if(executable_or_plugin(key))return {Status::ExecutablePath,Origin::Original,{}};
    for(auto it=entries_.rbegin();it!=entries_.rend();++it){
        if(it->key==key){
            std::filesystem::path verified;
            const auto code=plain_file_beneath(override_root_,it->source_relative,verified);
            return {code,Origin::Override,code==Status::Ok?verified:std::filesystem::path{}};
        }
    }
    std::filesystem::path original;
    const auto code=plain_file_beneath(original_root_,clean,original);
    return {code,Origin::Original,code==Status::Ok?original:std::filesystem::path{}};
}
const char* name(Status v) noexcept {
    switch(v){
        case Status::Ok:return "ok";
        case Status::InvalidPath:return "invalid-path";
        case Status::ExecutablePath:return "executable-path-blocked";
        case Status::SourceMissing:return "source-missing";
        case Status::SymlinkRejected:return "symlink-rejected";
        case Status::OutsideRoot:return "outside-root";
        case Status::NotRegularFile:return "not-regular-file";
        case Status::CapacityReached:return "capacity-reached";
    }
    return "unknown";
}
} // namespace scarlett::overlay
