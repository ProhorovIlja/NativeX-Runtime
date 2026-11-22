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
  struct ImportSymbol { std::string module, name; uint32_t ordinal; bool by_ordinal; };
  std::vector<Export> exports;
  std::vector<ImportSymbol> import_symbols, delay_import_symbols;
  struct RuntimeFunction { uint32_t begin, end, unwind; };
  std::vector<RuntimeFunction> runtime_functions;
  struct UnwindSummary {
    uint32_t rva;
    unsigned version, flags, prolog, slots;
    uint64_t stack_bytes;
    bool decoded, chain;
    uint32_t handler;
  };
  std::vector<UnwindSummary> unwind_summaries;
  uint32_t relocation_blocks=0;
  bool has_tls_directory=false;
  std::vector<uint32_t> tls_callbacks;
  uint32_t tls_template_rva=0, tls_template_bytes=0;
  uint32_t tls_index_rva=0, tls_zero_fill_bytes=0;
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
  // Inspect PE32+ IMAGE_THUNK_DATA64 without resolving or executing imports.
  // 8-byte entries, strict ordinal flag, bounded per descriptor and globally.
  void inspect_thunks(uint32_t thunk_rva,const std::string& dll,
                      std::vector<ImportSymbol>& destination) const {
    if(!thunk_rva)return; // Import descriptors without INT/IAT are preserved as modules.
    for(uint64_t i=0;i<=8192;i++){
      const uint64_t address=uint64_t(thunk_rva)+i*8u;
      if(address>UINT32_MAX)throw std::runtime_error("import thunk RVA overflow");
      const uint64_t thunk=u64(rva(uint32_t(address),8));
      if(!thunk)return;
      if(i==8192||destination.size()>=32768)
        throw std::runtime_error("PE import symbol limit exceeded");
      if(thunk&0x8000000000000000ull){
        // IMAGE_ORDINAL_FLAG64 set; all bits except low 16 must be zero.
        if(thunk&0x7fffffffffff0000ull)
          throw std::runtime_error("invalid PE import ordinal thunk");
        destination.push_back({dll,std::string{},uint32_t(thunk&0xffffu),true});
      }else{
        if(thunk>UINT32_MAX-2u)
          throw std::runtime_error("invalid PE import-by-name RVA");
        const uint32_t name_rva=uint32_t(thunk);
        // IMAGE_IMPORT_BY_NAME = 2-byte hint followed by ASCII NUL string.
        rva(name_rva,2);
        destination.push_back({dll,module(name_rva+2u),0u,false});
      }
    }
    throw std::runtime_error("unterminated PE import thunk table");
  }

  // Read-only AMD64 UNWIND_INFO header and version-1 basic op slots.
  // Do not interpret handlers, execute exceptions, or register function tables.
  // Version-2/3 formats are explicitly kept opaque rather than misdecoded.
  UnwindSummary inspect_unwind(const RuntimeFunction& fn,uint32_t image_size) const {
    const std::size_t first=rva(fn.unwind,4);
    const unsigned version=b[first]&7u;
    const unsigned flags=b[first]>>3u;
    const unsigned prolog=b[first+1u];
    const unsigned slots=b[first+2u];
    if(version==0u)throw std::runtime_error("invalid AMD64 UNWIND_INFO version zero");
    UnwindSummary info{fn.unwind,version,flags,prolog,slots,0,false,(flags&4u)!=0u,0};
    if(version!=1u)return info; // newer version-specific formats stay opaque
    if(flags>7u || ((flags&4u)!=0 && (flags&3u)!=0))
      throw std::runtime_error("invalid AMD64 UNWIND_INFO flags");
    const std::size_t aligned_slots=(slots+1u)&~std::size_t(1);
    const std::size_t suffix=info.chain?12u:((flags&3u)?4u:0u);
    const std::size_t full=4u+aligned_slots*2u+suffix;
    const std::size_t start=rva(fn.unwind,full);
    std::size_t index=0;
    while(index<slots){
      const std::size_t code_slot=start+4u+index*2u;
      const unsigned instruction_offset=b[code_slot];
      const unsigned opcode=b[code_slot+1u]&15u;
      const unsigned operand=b[code_slot+1u]>>4u;
      if(instruction_offset>prolog)
        throw std::runtime_error("AMD64 unwind operation beyond function prolog");
      unsigned extra=0;
      uint64_t allocation=0;
      switch(opcode){
        case 0u: allocation=8u;break; // PUSH_NONVOL
        case 1u: // ALLOC_LARGE
          if(operand==0u){extra=1u;}
          else if(operand==1u){extra=2u;}
          else throw std::runtime_error("unsupported large unwind allocation mode");
          break;
        case 2u: allocation=8u*(operand+1u);break; // ALLOC_SMALL
        case 3u:
          if(operand!=0u || (b[first+3u]&15u)==0u)
            throw std::runtime_error("invalid frame-register unwind op");
          break;
        case 4u: extra=1u;break; // SAVE_NONVOL
        case 5u: extra=2u;break; // SAVE_NONVOL_FAR
        case 8u: extra=1u;break; // SAVE_XMM128
        case 9u: extra=2u;break; // SAVE_XMM128_FAR
        case 10u:
          if(operand>1u)throw std::runtime_error("invalid machine-frame unwind mode");
          allocation=operand?48u:40u;break;
        default:
          throw std::runtime_error("unknown AMD64 version-one unwind opcode");
      }
      if(index+extra>=slots)
        throw std::runtime_error("truncated variable-length unwind operation");
      if(opcode==1u){
        const std::size_t extension=code_slot+2u;
        allocation=operand==0u
          ?uint64_t(u16(extension))*8u
          :uint64_t(u32(extension));
        // Both unwind ALLOC_LARGE encodings describe a nonzero number
        // of stack BYTES divisible by eight. The direct 32-bit variant
        // otherwise admits malformed sizes (e.g. 1) as apparently valid.
        if(allocation==0u || (allocation&7u)!=0u)
          throw std::runtime_error("invalid AMD64 large stack allocation size");
      }
      info.stack_bytes+=allocation;
      index+=1u+extra;
    }
    if(suffix){
      const std::size_t where=start+4u+aligned_slots*2u;
      if(info.chain){
        const uint32_t begin=u32(where),end=u32(where+4u),unwind=u32(where+8u);
        if(begin>=end||end>image_size||!unwind||unwind>=image_size||
           (unwind&3u))
          throw std::runtime_error("invalid chained AMD64 runtime function");
      }else{
        info.handler=u32(where);
        if(!info.handler||info.handler>=image_size)
          throw std::runtime_error("invalid AMD64 unwind handler RVA");
      }
    }
    info.decoded=true;
    return info;
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
        const uint32_t export_name_rva=u32(name_table+uint64_t(i)*4);
        const uint32_t ordinal=uint32_t(uint64_t(base)+index);
        // Forwarded exports point into the export-directory RVA range.
        const bool forwarded=uint64_t(target)>=exp.first &&
            uint64_t(target)<uint64_t(exp.first)+exp.second;
        exports.push_back({module(export_name_rva),ordinal,forwarded?module(target):std::string{}});
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
          const auto dll=module(name);
          imports.push_back(dll);
          // OriginalFirstThunk is the lookup table; FirstThunk is fallback.
          const uint32_t lookup=u32(p)?u32(p):u32(p+16);
          inspect_thunks(lookup,dll,import_symbols);
        }
        if(!done)throw std::runtime_error("unterminated import table");
      }
    }
    // AMD64 .pdata exception directory = sorted 12-byte RUNTIME_FUNCTION
    // entries. This is read-only metadata, not interpretation of UNWIND_INFO.
    // Reject malformed ranges before consumers use this inventory for ABI work.
    const auto exception=dir(3);
    if(exception.first || exception.second) {
      if(!exception.first || !exception.second || exception.second%12u ||
         exception.second/12u>65536u)
        throw std::runtime_error("invalid AMD64 exception directory size");
      const uint32_t image_size=u32(opt+56);
      const auto first=rva(exception.first,exception.second);
      for(uint32_t i=0;i<exception.second/12u;i++){
        const std::size_t p=first+std::size_t(i)*12u;
        const uint32_t begin=u32(p),end=u32(p+4),unwind=u32(p+8);
        if(begin>=end || end>image_size || unwind>=image_size || !unwind ||
           (unwind&3u) || (i && begin<runtime_functions.back().end))
          throw std::runtime_error("invalid AMD64 runtime function range");
        runtime_functions.push_back({begin,end,unwind});
      }
      for(const auto& fn:runtime_functions)
        unwind_summaries.push_back(inspect_unwind(fn,image_size));
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
    // IMAGE_TLS_DIRECTORY64 fields are VAs, not RVAs; do not load or run them.
    const auto tls=dir(9);
    if(tls.first || tls.second){
      if(!tls.first || tls.second<40)
        throw std::runtime_error("invalid TLS directory RVA/size");
      const size_t p=rva(tls.first,40);
      const uint32_t image_size=u32(opt+56);
      const auto checked_rva=[&](uint64_t va)->uint32_t {
        if(va<image_base || va-image_base>=image_size ||
           va-image_base>UINT32_MAX)
          throw std::runtime_error("TLS VA outside PE32+ image");
        return static_cast<uint32_t>(va-image_base);
      };
      const uint64_t start=u64(p),end=u64(p+8);
      const uint64_t index_va=u64(p+16),callback_va=u64(p+24);
      const uint32_t zero_fill=u32(p+32);
      if(zero_fill>64u*1024u*1024u)
        throw std::runtime_error("TLS zero-fill exceeds inspection limit");
      tls_zero_fill_bytes=zero_fill;
      if(start || end){
        if(!start || !end || end<start)
          throw std::runtime_error("invalid TLS raw template endpoints");
        tls_template_rva=checked_rva(start);
        if(end-image_base>image_size || end-start>UINT32_MAX)
          throw std::runtime_error("invalid TLS template size");
        tls_template_bytes=static_cast<uint32_t>(end-start);
      }
      if(index_va){
        tls_index_rva=checked_rva(index_va);
        if(uint64_t(tls_index_rva)+4u>image_size)
          throw std::runtime_error("invalid TLS index storage");
      }
      if(callback_va){
        const uint32_t callbacks_rva=checked_rva(callback_va);
        bool terminated=false;
        for(uint64_t i=0;i<=256u;i++){
          const uint64_t callback_entry_rva=uint64_t(callbacks_rva)+i*8u;
          if(callback_entry_rva>UINT32_MAX)
            throw std::runtime_error("TLS callback table RVA overflow");
          const uint64_t function_va=u64(rva(uint32_t(callback_entry_rva),8));
          if(!function_va){terminated=true;break;}
          if(i==256u)throw std::runtime_error("TLS callback count limit exceeded");
          tls_callbacks.push_back(checked_rva(function_va));
        }
        if(!terminated)throw std::runtime_error("unterminated TLS callbacks");
      }
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
        const auto dll=module(name);
        delay_imports.push_back(dll);
        // IMAGE_DELAYLOAD_DESCRIPTOR pINT (offset 16), IAT (offset 12) fallback.
        const uint32_t lookup=u32(pos+16)?u32(pos+16):u32(pos+12);
        inspect_thunks(lookup,dll,delay_import_symbols);
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
    out<<"],\"import_symbols\":[";
    for(size_t i=0;i<import_symbols.size();i++){
      if(i)out<<",";
      const auto& x=import_symbols[i];
      out<<"{\"module\":"<<quote(x.module)<<",\"name\":";
      if(x.by_ordinal)out<<"null";else out<<quote(x.name);
      out<<",\"ordinal\":";
      if(x.by_ordinal)out<<x.ordinal;else out<<"null";
      out<<"}";
    }
    out<<"],\"delay_import_symbols\":[";
    for(size_t i=0;i<delay_import_symbols.size();i++){
      if(i)out<<",";
      const auto& x=delay_import_symbols[i];
      out<<"{\"module\":"<<quote(x.module)<<",\"name\":";
      if(x.by_ordinal)out<<"null";else out<<quote(x.name);
      out<<",\"ordinal\":";
      if(x.by_ordinal)out<<x.ordinal;else out<<"null";
      out<<"}";
    }
    out<<"],\"exports\":[";
    for(size_t i=0;i<exports.size();i++){
      if(i)out<<",";
      out<<"{\"name\":"<<quote(exports[i].name)<<",\"ordinal\":"<<exports[i].ordinal
         <<",\"forwarder\":";
      if(exports[i].forwarder.empty())out<<"null";else out<<quote(exports[i].forwarder);
      out<<"}";
    }
    out<<"],\"runtime_functions\":[";
    for(size_t i=0;i<runtime_functions.size();i++){
      if(i)out<<",";
      const auto& fn=runtime_functions[i];
      out<<"{\"begin_rva\":"<<fn.begin<<",\"end_rva\":"<<fn.end
         <<",\"unwind_rva\":"<<fn.unwind<<"}";
    }
    out<<"],\"unwind_infos\":[";
    for(size_t i=0;i<unwind_summaries.size();i++){
      if(i)out<<",";
      const auto& x=unwind_summaries[i];
      out<<"{\"unwind_rva\":"<<x.rva<<",\"version\":"<<x.version
         <<",\"flags\":"<<x.flags<<",\"prolog_bytes\":"<<x.prolog
         <<",\"code_slots\":"<<x.slots
         <<",\"decoded_v1_basic_operations\":"<<(x.decoded?"true":"false")
         <<",\"stack_allocation_bytes\":";
      if(x.decoded)out<<x.stack_bytes;else out<<"null";
      out<<",\"chained\":"<<(x.chain?"true":"false")<<",\"handler_rva\":";
      if(x.handler)out<<x.handler;else out<<"null";
      out<<"}";
    }
    out<<"],\"unwind_codes_executed\":false";
    out<<",\"unwind_codes_decoded\":false";
    out<<",\"tls_template_rva\":";
    if(tls_template_rva)out<<tls_template_rva;else out<<"null";
    out<<",\"tls_template_bytes\":"<<tls_template_bytes;
    out<<",\"tls_zero_fill_bytes\":"<<tls_zero_fill_bytes;
    out<<",\"tls_index_rva\":";
    if(tls_index_rva)out<<tls_index_rva;else out<<"null";
    out<<",\"tls_callback_rvas\":[";
    for(size_t i=0;i<tls_callbacks.size();i++)
      out<<(i?",":"")<<tls_callbacks[i];
    out<<"],\"tls_callbacks_executed\":false";
    out<<",\"relocation_blocks\":"<<relocation_blocks
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
