// Read-only PE32+ metadata inspector. No third-party or game code.
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class PE {
  std::vector<unsigned char> b;
  struct Section { uint32_t va, size, raw, len; };
  std::vector<Section> sections;
  std::vector<std::string> names, imports;
  uint64_t image_base = 0;
  uint32_t entry = 0, headers = 0;
  void need(size_t pos,size_t n) const {
    if(pos>b.size() || n>b.size()-pos) throw std::runtime_error("truncated or out-of-range PE");
  }
  uint16_t u16(size_t p) const { need(p,2); return uint16_t(b[p]) | (uint16_t(b[p+1])<<8); }
  uint32_t u32(size_t p) const { return uint32_t(u16(p)) | (uint32_t(u16(p+2))<<16); }
  uint64_t u64(size_t p) const { return uint64_t(u32(p)) | (uint64_t(u32(p+4))<<32); }
  size_t rva(uint32_t addr,size_t len) const {
    if(addr<headers){need(addr,len);return addr;}
    for(auto s:sections){
      if(addr<s.va) continue;
      uint64_t off=uint64_t(addr)-s.va;
      if(off>=std::max(s.size,s.len)||off+len>s.len) continue;
      uint64_t p=uint64_t(s.raw)+off;
      if(p>b.size()||len>b.size()-size_t(p)) throw std::runtime_error("invalid section offset");
      return size_t(p);
    }
    throw std::runtime_error("unmapped RVA");
  }
  std::string module(uint32_t name) const {
    std::string s; size_t p=rva(name,1);
    for(unsigned i=0;i<256;i++){need(p,1);unsigned c=b[p++];if(!c)return s;
      if(c<32||c>126)throw std::runtime_error("non-ASCII module name");
      s.push_back(char(c));
    }
    throw std::runtime_error("unterminated import name");
  }
  static std::string quote(const std::string& s){
    std::ostringstream out;out<<'"';
    for(unsigned char c:s) if(c=='"'||c=='\\')out<<'\\'<<char(c);
    else if(c>=32&&c<127)out<<char(c);
    else out<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(c)<<std::dec;
    out<<'"';return out.str();
  }
public:
  explicit PE(const char* path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("file open failed");
    auto n=f.tellg(); if(n<0||n>512LL*1024*1024)throw std::runtime_error("file outside 512 MiB limit");
    b.resize(size_t(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),std::streamsize(n)))
      throw std::runtime_error("short read");
    if(u16(0)!=0x5a4d)throw std::runtime_error("not MZ");
    size_t pe=u32(0x3c);if(u32(pe)!=0x4550)throw std::runtime_error("not PE");
    size_t coff=pe+4;
    if(u16(coff)!=0x8664)throw std::runtime_error("not AMD64");
    unsigned count=u16(coff+2),optlen=u16(coff+16);
    if(!count||count>96||optlen<128)throw std::runtime_error("bad header sizes");
    size_t opt=coff+20;need(opt,optlen);
    if(u16(opt)!=0x20b)throw std::runtime_error("not PE32+");
    entry=u32(opt+16);image_base=u64(opt+24);headers=u32(opt+60);
    size_t sh=opt+optlen;need(sh,size_t(count)*40);
    for(unsigned i=0;i<count;i++){
      size_t p=sh+i*40;std::string name;
      for(unsigned j=0;j<8&&b[p+j];j++)name.push_back(char(b[p+j]));
      names.push_back(name);
      sections.push_back({u32(p+12),u32(p+8),u32(p+20),u32(p+16)});
    }
    if(u32(opt+108)>=2){
      uint32_t addr=u32(opt+120),length=u32(opt+124);
      if(addr&&length){
        bool done=false;
        for(uint64_t i=0;i<std::min<uint64_t>(1024,length/20);++i){
          if(uint64_t(addr)+20*i>UINT32_MAX)throw std::runtime_error("import RVA overflow");
          size_t p=rva(uint32_t(addr+20*i),20);
          uint32_t name=u32(p+12);
          if(!u32(p)&&!u32(p+4)&&!u32(p+8)&&!name&&!u32(p+16)){done=true;break;}
          if(!name)throw std::runtime_error("missing DLL name");
          imports.push_back(module(name));
        }
        if(!done)throw std::runtime_error("unterminated import table");
      }
    }
  }
  std::string json() const{
    std::ostringstream out;
    out<<"{\"kind\":\"pe32plus-metadata\",\"machine\":\"AMD64\",\"sections\":"<<sections.size()
       <<",\"entry_rva\":"<<entry<<",\"image_base\":"<<image_base<<",\"import_modules\":[";
    for(size_t i=0;i<imports.size();i++)out<<(i?",":"")<<quote(imports[i]);
    out<<"],\"section_names\":[";
    for(size_t i=0;i<names.size();i++)out<<(i?",":"")<<quote(names[i]);
    out<<"],\"executable_tested\":false}\n";return out.str();
  }
};
int main(int argc,char** argv){
  if(argc!=2){std::cerr<<"usage: pe_intake <PE32+ file>\n";return 2;}
  try{std::cout<<PE(argv[1]).json();return 0;}
  catch(const std::exception& e){std::cerr<<"pe_intake: "<<e.what()<<'\n';return 2;}
}
