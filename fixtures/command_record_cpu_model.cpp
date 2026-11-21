// Source-built synthetic command-record writer/reader, not a console binary format.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
namespace {
constexpr std::size_t Capacity=128, Guard=16, Header=8;
enum class Queue:std::uint8_t{Graphics,Dma};
enum class Op:std::uint16_t{Topology=1,Draw=2,Copy=3};
std::size_t count(Queue q,Op op){
    if(q==Queue::Graphics && op==Op::Topology)return 1;
    if(q==Queue::Graphics && op==Op::Draw)return 2;
    if(q==Queue::Dma && op==Op::Copy)return 3;
    return 0;
}
void put16(std::uint8_t* p,std::uint16_t x){
    p[0]=static_cast<std::uint8_t>(x);
    p[1]=static_cast<std::uint8_t>(x>>8);
}
void put32(std::uint8_t* p,std::uint32_t x){
    for(unsigned i=0;i<4;i++)p[i]=static_cast<std::uint8_t>(x>>(8*i));
}
std::uint16_t get16(const std::uint8_t* p){
    return static_cast<std::uint16_t>(std::uint16_t(p[0])|(std::uint16_t(p[1])<<8));
}
std::uint32_t get32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|
           (std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
struct Record{Op opcode;std::uint32_t sequence;std::vector<std::uint32_t> words;};
bool parse(const std::vector<std::uint8_t>& bytes,Queue q,std::vector<Record>& out){
    if(bytes.size()>Capacity)return false;
    std::vector<Record> parsed;
    std::size_t offset=0;
    std::uint32_t seq=1;
    while(offset<bytes.size()){
        if(bytes.size()-offset<Header)return false;
        const Op op=static_cast<Op>(get16(bytes.data()+offset));
        const std::size_t length=get16(bytes.data()+offset+2);
        const std::size_t payload=count(q,op);
        if(!payload || length!=Header+4*payload ||
           length>bytes.size()-offset || get32(bytes.data()+offset+4)!=seq)return false;
        Record rec{op,seq,{}};
        for(std::size_t i=0;i<payload;i++)
            rec.words.push_back(get32(bytes.data()+offset+Header+4*i));
        parsed.push_back(std::move(rec));
        offset+=length;
        if(seq==std::numeric_limits<std::uint32_t>::max())return false;
        ++seq;
    }
    out=std::move(parsed);
    return true;
}
class Writer{
    std::array<std::uint8_t,Capacity+2*Guard> store_{};
    Queue queue_;
    std::size_t size_,used_=0;
    std::uint32_t seq_=1;
public:
    Writer(Queue q,std::size_t size):queue_(q),size_(size){
        if(size==0||size>Capacity)throw std::invalid_argument("capacity");
        store_.fill(0xa5);
        reset();
    }
    void reset(){
        std::fill(store_.begin()+Guard,store_.begin()+Guard+Capacity,0);
        used_=0;seq_=1;
    }
    bool guards()const{
        for(std::size_t i=0;i<Guard;i++)
            if(store_[i]!=0xa5||store_[Guard+Capacity+i]!=0xa5)return false;
        return true;
    }
    bool append(Op op,const std::vector<std::uint32_t>& words){
        const std::size_t n=count(queue_,op);
        if(!n||words.size()!=n||seq_==std::numeric_limits<std::uint32_t>::max())
            return false;
        const std::size_t length=Header+4*n;
        if(length>size_-used_)return false;
        auto* p=store_.data()+Guard+used_;
        put16(p,static_cast<std::uint16_t>(op));
        put16(p+2,static_cast<std::uint16_t>(length));
        put32(p+4,seq_);
        for(std::size_t i=0;i<n;i++)put32(p+Header+4*i,words[i]);
        used_+=length;++seq_;
        return true;
    }
    std::size_t used()const{return used_;}
    std::vector<std::uint8_t> snapshot()const{
        return{store_.begin()+Guard,store_.begin()+Guard+used_};
    }
};
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
void test_graphics(){
    Writer w{Queue::Graphics,96};
    check(w.guards(),"bad initial guards");
    check(w.append(Op::Topology,{3}),"topology");
    check(w.append(Op::Draw,{19,1}),"draw");
    for(std::uint32_t i=0;i<4;i++)
        check(w.append(Op::Draw,{i+100,3}),"near-capacity draw");
    check(w.used()==92,"unexpected encoded length");
    const auto original=w.snapshot();
    check(!w.append(Op::Draw,{1,1}),"overflow permitted");
    check(!w.append(Op::Copy,{1,2,3}),"graphics accepted DMA");
    check(!w.append(Op::Draw,{1}),"invalid payload permitted");
    check(w.guards()&&w.snapshot()==original,"rejection not atomic");
    std::vector<Record> decoded;
    check(parse(original,Queue::Graphics,decoded)&&decoded.size()==6,"roundtrip");
    check(decoded[0].opcode==Op::Topology&&decoded[0].words[0]==3,"topology value");
    check(decoded[5].sequence==6&&decoded[5].words[0]==103,"draw values");
    std::vector<Record> sentinel{{Op::Topology,999,{777}}};
    auto reject=[&](std::vector<std::uint8_t> bad){
        auto out=sentinel;
        check(!parse(bad,Queue::Graphics,out),"corruption accepted");
        check(out.size()==1&&out[0].sequence==999&&out[0].words[0]==777,
              "failed parse clobbered output");
    };
    auto bad=original;bad[0]=0xff;reject(bad);
    bad=original;bad[2]=0;bad[3]=0;reject(bad);
    bad=original;bad[2]=0xff;bad[3]=0x7f;reject(bad);
    bad=original;bad[4]=2;reject(bad);
    bad=original;bad.pop_back();reject(bad);
    bad=original;bad.push_back(0);reject(bad);
    check(!parse(original,Queue::Dma,sentinel),"graphics parsed as DMA");
    w.reset();
    check(w.guards()&&w.used()==0&&w.snapshot().empty(),"reset");
    check(w.append(Op::Draw,{8,9}),"append after reset");
    decoded.clear();
    check(parse(w.snapshot(),Queue::Graphics,decoded)&&decoded[0].sequence==1,
          "sequence after reset");
}
void test_dma(){
    Writer w{Queue::Dma,64};
    check(!w.append(Op::Topology,{1}),"DMA accepted graphics");
    check(w.append(Op::Copy,{0,4096,1024}),"DMA first");
    check(w.append(Op::Copy,{4096,8192,1024}),"DMA second");
    std::vector<Record> out;
    check(parse(w.snapshot(),Queue::Dma,out)&&out.size()==2,"DMA decode");
    check(out[1].words[1]==8192&&w.guards(),"DMA values");
    check(!parse(w.snapshot(),Queue::Graphics,out),"DMA parsed as graphics");
}
void test_stress(){
    std::uint32_t seed=0x5a7a2026u;
    for(unsigned trial=0;trial<256;trial++){
        Writer w{Queue::Graphics,128};
        unsigned records=0;
        for(unsigned i=0;i<16;i++){
            seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
            const Op op=(seed&1)?Op::Draw:Op::Topology;
            const auto words=op==Op::Draw?
                std::vector<std::uint32_t>{seed,seed^0xfedcba98u}:
                std::vector<std::uint32_t>{seed};
            if(!w.append(op,words))break;
            ++records;
            check(w.guards(),"stress guard");
        }
        std::vector<Record> out;
        check(parse(w.snapshot(),Queue::Graphics,out),"stress decode");
        check(out.size()==records,"stress count");
        for(unsigned i=0;i<records;i++)
            check(out[i].sequence==i+1,"stress order");
    }
}
}
int main(){
    try{
        test_graphics();test_dma();test_stress();
        std::cout<<"{\"kind\":\"synthetic-command-record-contract\","
                 <<"\"stress_cases\":256,\"queue_types_separate\":true,"
                 <<"\"overflow_and_corruption_rejected\":true,"
                 <<"\"rejected_write_atomic\":true,"
                 <<"\"xbox_record_format_tested\":false,\"gpu_executed\":false}\n";
    }catch(const std::exception& e){
        std::cerr<<"synthetic command record test failed: "<<e.what()<<'\n';
        return 1;
    }
    return 0;
}
