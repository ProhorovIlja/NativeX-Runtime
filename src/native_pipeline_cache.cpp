#include "native_pipeline_cache.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <utility>
namespace scarlett::pipeline {
namespace {
constexpr std::array<std::uint8_t,8> Magic{'N','X','P','S','O',1,0,0};
constexpr std::size_t Overhead=24; // magic, scope size, record count, checksum
constexpr std::size_t MaxScope=4096;
std::uint64_t hash(const Bytes& b,std::size_t n){std::uint64_t h=14695981039346656037ull;for(std::size_t i=0;i<n;++i){h^=b[i];h*=1099511628211ull;}return h;}
void append(Bytes& b,std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
std::uint64_t integer(const Bytes& b,std::size_t at,unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=static_cast<std::uint64_t>(b[at+i])<<(8*i);return v;}
bool valid(const Bytes& key,const Bytes& blob){return !key.empty()&&key.size()<=MaxKeyBytes&&!blob.empty()&&blob.size()<=MaxBlobBytes;}
std::size_t record_size(const Bytes& key,const Bytes& blob){return 8+key.size()+blob.size();}
struct TempDir {std::filesystem::path path;~TempDir(){if(!path.empty()){std::error_code e;std::filesystem::remove(path/"snapshot",e);std::filesystem::remove(path,e);}}};
}
Cache::Cache(Bytes scope,std::size_t budget,std::size_t entries)
    :scope_(std::move(scope)),budget_(budget),limit_(entries),bytes_(Overhead+scope_.size()){
    if(scope_.empty()||scope_.size()>MaxScope||budget_<bytes_||budget_>MaxSnapshotBytes||!limit_||limit_>4096)throw std::invalid_argument("invalid pipeline cache limits");
}
Status Cache::put(const Bytes& key,const Bytes& blob){
    if(!valid(key,blob))return Status::Invalid;
    auto it=entries_.find(key);const auto old=it==entries_.end()?0:record_size(it->first,it->second);
    auto n=record_size(key,blob);
    if(n>budget_-(bytes_-old)||(it==entries_.end()&&entries_.size()==limit_))return Status::Full;
    if(it==entries_.end())entries_.emplace(key,blob);else {Bytes replacement(blob);it->second.swap(replacement);}
    bytes_=bytes_-old+n;return Status::Ok;
}
const Bytes* Cache::find(const Bytes& key) const {auto it=entries_.find(key);return it==entries_.end()?nullptr:&it->second;}
bool Cache::erase(const Bytes& key){auto it=entries_.find(key);if(it==entries_.end())return false;bytes_-=record_size(it->first,it->second);entries_.erase(it);return true;}
Status Cache::save(const std::filesystem::path& file) const {
    if(file.empty()||file.filename().empty())return Status::Invalid;
    Bytes raw;raw.reserve(bytes_);raw.insert(raw.end(),Magic.begin(),Magic.end());append(raw,scope_.size(),4);append(raw,entries_.size(),4);raw.insert(raw.end(),scope_.begin(),scope_.end());
    for(const auto& kv:entries_){append(raw,kv.first.size(),4);append(raw,kv.second.size(),4);raw.insert(raw.end(),kv.first.begin(),kv.first.end());raw.insert(raw.end(),kv.second.begin(),kv.second.end());}
    append(raw,hash(raw,raw.size()),8);
    // Parent must be trusted and already exist. Never truncate an existing temp file.
    TempDir temp;std::error_code ec;auto parent=file.parent_path();if(parent.empty())parent=".";
    for(unsigned i=0;i<32;++i){auto candidate=parent/(".nativex-pso-tmp-"+std::to_string(i));if(std::filesystem::create_directory(candidate,ec)){temp.path=std::move(candidate);break;}if(ec)return Status::IoError;}
    if(temp.path.empty())return Status::IoError;
    const auto path=temp.path/"snapshot";
    {std::ofstream out(path,std::ios::binary|std::ios::trunc);if(!out)return Status::IoError;out.write(reinterpret_cast<const char*>(raw.data()),static_cast<std::streamsize>(raw.size()));out.close();if(!out)return Status::IoError;}
    std::filesystem::rename(path,file,ec);return ec?Status::IoError:Status::Ok;
}
Status Cache::load(const std::filesystem::path& file){
    std::error_code ec;auto fs=std::filesystem::file_size(file,ec);if(ec){if(ec==std::errc::no_such_file_or_directory)return Status::Miss;return Status::IoError;}
    if(fs>budget_)return Status::Full;
    if(fs<Overhead)return Status::Corrupt;
    Bytes raw(static_cast<std::size_t>(fs));std::ifstream in(file,std::ios::binary);if(!in)return Status::IoError;
    in.read(reinterpret_cast<char*>(raw.data()),static_cast<std::streamsize>(raw.size()));if(!in)return Status::Corrupt;
    if(in.peek()!=std::char_traits<char>::eof()||in.bad())return Status::Corrupt;
    const auto end=raw.size()-8;
    if(!std::equal(Magic.begin(),Magic.end(),raw.begin())||integer(raw,end,8)!=hash(raw,end))return Status::Corrupt;
    const auto scope=static_cast<std::size_t>(integer(raw,8,4)),count=static_cast<std::size_t>(integer(raw,12,4));
    std::size_t pos=16;if(!scope||scope>MaxScope||scope>end-pos)return Status::Corrupt;
    if(scope!=scope_.size()||!std::equal(scope_.begin(),scope_.end(),raw.begin()+static_cast<std::ptrdiff_t>(pos)))return Status::ScopeMismatch;
    if(count>limit_)return Status::Full;
    pos+=scope;std::map<Bytes,Bytes> next;
    for(std::size_t i=0;i<count;++i){if(end-pos<8)return Status::Corrupt;auto k=static_cast<std::size_t>(integer(raw,pos,4)),v=static_cast<std::size_t>(integer(raw,pos+4,4));pos+=8;
        if(!k||k>MaxKeyBytes||!v||v>MaxBlobBytes||k>end-pos||v>end-pos-k)return Status::Corrupt;
        const auto start=raw.begin()+static_cast<std::ptrdiff_t>(pos);Bytes key(start,start+static_cast<std::ptrdiff_t>(k));Bytes blob(start+static_cast<std::ptrdiff_t>(k),start+static_cast<std::ptrdiff_t>(k+v));pos+=k+v;
        if(!next.emplace(std::move(key),std::move(blob)).second)return Status::Corrupt;
    }
    if(pos!=end)return Status::Corrupt;
    entries_.swap(next);bytes_=raw.size();return Status::Ok;
}
} // namespace scarlett::pipeline
