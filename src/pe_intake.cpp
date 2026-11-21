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
#include <utility>

class PE {
  std::vector<unsigned char> b;
  struct Section { uint32_t va, size, raw, len; };
  std::vector<Section> sections;
  std::vector<std::string> names, imports, delay_imports;
  struct Export { std::string name; uint32_t ordinal; std::string forwarder; };
  std::vector<Export> exports;
  uint32_t relocation_blocks=0;
  bool has_tls_directory=false;
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
    std::string s;
    for(uint64_t i=0;i<256;i++){
      if(uint64_t(name)+i>UINT32_MAX)throw std::runtime_error("name RVA overflow");
      unsigned c=b[rva(uint32_t(uint64_t(name)+i),1)];
      if(!c)return s;
      if(c<32||c>126)throw std::runtime_error("non-ASCII PE name");
      s.push_back(char(c));
    }
    throw std::runtime_error("unterminated PE name");
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
    // Directory tuples are RVA+size; a claimed directory must fit the optional header.
    const auto dir=[&](unsigned index)->std::pair<uint32_t,uint32_t>{
      unsigned count=u32(opt+108);
      if(count<=index)return {0,0};
      if(optlen<112+8*(index+1))throw std::runtime_error("truncated PE data directory");
      return {u32(opt+112+8*index),u32(opt+116+8*index)};
    };
    // Export directory: name/ordinal/forwarder metadata only, never resolve or load it.
    const auto exp=dir(0);
    if(exp.first && exp.second){
      if(exp.second<40)throw std::runtime_error("short export directory");
      const size_t p=rva(exp.first,40);
      const uint32_t base=u32(p+16), fn_count=u32(p+20), name_count=u32(p+24);
      const uint32_t fn_rva=u32(p+28), names_rva=u32(p+32), ord_rva=u32(p+36);
      if(fn_count>8192 || name_count>4096 || name_count>fn_count ||
         uint64_t(base)+fn_count>uint64_t(UINT32_MAX)+1)
        throw std::runtime_error("unsupported export counts");
      size_t functions=0, name_table=0, ordinal_table=0;
      if(fn_count){
        if(!fn_rva)throw std::runtime_error("missing export function table");
        functions=rva(fn_rva,uint64_t(fn_count)*4);
      }
      if(name_count){
        if(!names_rva || !ord_rva)throw std::runtime_error("missing export name tables");
        name_table=rva(names_rva,uint64_t(name_count)*4);
        ordinal_table=rva(ord_rva,uint64_t(name_count)*2);
      }
      for(uint32_t i=0;i<name_count;i++){
        const uint16_t index=u16(ordinal_table+uint64_t(i)*2);
        if(index>=fn_count)throw std::runtime_error("invalid export ordinal index");
        const uint32_t target=u32(functions+uint64_t(index)*4);
        const uint32_t n=u32(name_table+uint64_t(i)*4);
        const uint32_t ordinal=uint32_t(uint64_t(base)+index);
        // Forwarded exports point into the export-directory RVA range.
        const bool forwarded=uint64_t(target)>=exp.first &&
            uint64_t(target)<uint64_t(exp.first)+exp.second;
        exports.push_back({module(n),ordinal,forwarded?module(target):std::string{}});
      }
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
    const auto reloc=dir(5);
    if(reloc.first && reloc.second){
      uint64_t consumed=0;
      while(consumed<reloc.second){
        if(reloc.second-consumed<8 || uint64_t(reloc.first)+consumed>UINT32_MAX)
          throw std::runtime_error("truncated relocations");
        size_t pos=rva(uint32_t(uint64_t(reloc.first)+consumed),8);
        uint32_t block=u32(pos+4);
        if(block<8 || block%2 || block>reloc.second-consumed)
          throw std::runtime_error("invalid relocation block");
        rva(uint32_t(uint64_t(reloc.first)+consumed),block);
        consumed+=block;
        ++relocation_blocks;
      }
    }
    const auto tls=dir(9);
    if(tls.first && tls.second){
      if(tls.second<40)throw std::runtime_error("truncated TLS directory");
      rva(tls.first,40);
      has_tls_directory=true;
    }
    const auto delay=dir(13);
    if(delay.first && delay.second){
      bool terminated=false;
      for(uint64_t i=0;i<std::min<uint64_t>(1024,delay.second/32);++i){
        if(uint64_t(delay.first)+i*32>UINT32_MAX)throw std::runtime_error("delay import RVA overflow");
        size_t pos=rva(uint32_t(uint64_t(delay.first)+i*32),32);
        uint32_t attrs=u32(pos), name=u32(pos+4);
        bool all_zero=true;
        for(unsigned j=0;j<32;j+=4)all_zero=all_zero && (u32(pos+j)==0);
        if(all_zero){terminated=true;break;}
        if(!(attrs&1)||!name)throw std::runtime_error("unsupported delay import descriptor");
        delay_imports.push_back(module(name));
      }
      if(!terminated)throw std::runtime_error("unterminated delay imports");
    }
  }
  std::string json() const{
    std::ostringstream out;
    out<<"{\"kind\":\"pe32plus-metadata\",\"machine\":\"AMD64\",\"sections\":"<<sections.size()
       <<",\"entry_rva\":"<<entry<<",\"image_base\":"<<image_base<<",\"import_modules\":[";
    for(size_t i=0;i<imports.size();i++)out<<(i?",":"")<<quote(imports[i]);
    out<<"],\"delay_import_modules\":[";
    for(size_t i=0;i<delay_imports.size();i++)out<<(i?",":"")<<quote(delay_imports[i]);
    out<<"],\"exports\":[";
    for(size_t i=0;i<exports.size();i++){
      if(i)out<<",";
      out<<"{\"name\":"<<quote(exports[i].name)<<",\"ordinal\":"<<exports[i].ordinal
         <<",\"forwarder\":";
      if(exports[i].forwarder.empty())out<<"null";else out<<quote(exports[i].forwarder);
      out<<"}";
    }
    out<<"],\"relocation_blocks\":"<<relocation_blocks
       <<",\"has_tls_directory\":"<<(has_tls_directory?"true":"false")
       <<",\"section_names\":[";
    for(size_t i=0;i<names.size();i++)out<<(i?",":"")<<quote(names[i]);
    out<<"],\"executable_tested\":false}\n";return out.str();
  }
};
int main(int argc,char** argv){
  if(argc!=2){std::cerr<<"usage: pe_intake <PE32+ file>\n";return 2;}
  try{std::cout<<PE(argv[1]).json();return 0;}
  catch(const std::exception& e){std::cerr<<"pe_intake: "<<e.what()<<'\n';return 2;}
}
