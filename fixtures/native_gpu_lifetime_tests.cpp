#include "native_gpu_lifetime.hpp"
#include <algorithm>
#include <iostream>
#include <random>
using namespace scarlett::gpu_lifetime;
void check(bool b){if(!b)throw std::runtime_error("unified GPU lifetime invariant");}
void boundary(){
    using Q=Queue;
    for(std::array<Q,2> order:{std::array<Q,2>{Q::Graphics,Q::Compute},std::array<Q,2>{Q::Compute,Q::Graphics}}){
        Ledger x(200,4);auto a=x.reserve(1,100,2).value();
        check(x.begin(Q::Copy,1)&&x.upload(a.token,Q::Copy));
        check(!x.observe(Q::Copy,1)&&!x.publish(a.token)&&!x.submit(Q::Copy,2));
        check(x.submit(Q::Copy,1)&&x.observe(Q::Copy,1)&&x.publish(a.token));
        check(x.begin(Q::Graphics,5)&&x.begin(Q::Compute,7)&&x.use(a.token,Q::Graphics)&&x.use(a.token,Q::Compute));
        auto b=x.reserve(1,100,2).value();check(!x.reserve(2,1,1));
        check(x.begin(Q::Copy,2)&&x.upload(b.token,Q::Copy)&&x.submit(Q::Copy,2)&&x.observe(Q::Copy,2)&&x.publish(b.token));
        check(x.active(1)==b.token&&!x.use(a.token,Q::Graphics)&&!x.released(a.token));
        check(!x.observe(Q::Graphics,5)&&!x.observe(Q::Compute,7));
        for(auto q:order){Id f=q==Q::Graphics?5:7;check(x.submit(q,f)&&x.observe(q,f));if(q==order[0])check(!x.can_release(a.token));}
        check(x.can_release(a.token)&&x.stats().charged==200&&x.stats().descriptors==4);
        check(x.released(a.token)&&!x.released(a.token));auto c=x.reserve(2,50,2).value();check(c.offset==0&&c.token!=a.token);
        x.set_budget(1);check(!x.reserve(3,1,1));check(x.retire(c.token)&&x.released(c.token));
        check(!x.begin(Q::Copy,Invalid)&&!x.observe(Q::Copy,Invalid)&&!x.begin(static_cast<Q>(99),8));
    }
    Ledger x(Invalid,1);auto a=x.reserve(1,Invalid,1).value();check(!x.reserve(2,1,1)&&x.retire(a.token)&&x.released(a.token));
    // Abandoned recorded work remains protected until its real submitted fence.
    auto b=x.reserve(1,1,1).value();check(x.begin(Q::Copy,3)&&x.upload(b.token,Q::Copy)&&x.retire(b.token));
    check(!x.released(b.token)&&!x.observe(Q::Copy,3)&&x.submit(Q::Copy,3)&&x.observe(Q::Copy,3)&&x.released(b.token));
}
void stress(unsigned seed){
    Ledger x(100,32,16);std::vector<Record> model;Fences open{},submitted{},completed{};Id next=1,budget=100,peak=0;std::mt19937 rng(seed);std::size_t accepted=0;
    auto sum=[&](){Id bytes=0;for(auto r:model)bytes+=r.bytes;return bytes;};
    auto done=[&](Record r){return std::equal(r.uses.begin(),r.uses.end(),completed.begin(),[](Id a,Id b){return a<=b;});};
    for(unsigned step=0;step<100000;++step){
        Id slot=1+rng()%8,token=model.empty()?0:model[rng()%model.size()].token;if(rng()%7==0)token=next+1;
        auto i=std::find_if(model.begin(),model.end(),[&](Record r){return r.token==token;});bool exists=i!=model.end(),ok=false,actual=false;
        auto n=static_cast<std::size_t>(rng()%3);auto q=static_cast<Queue>(n);Id f=submitted[n]+rng()%4;if(rng()%31==0)f=Invalid;
        switch(rng()%9){
        case 0:{Id bytes=1+rng()%40;std::size_t count=1+rng()%6;std::optional<std::size_t> off;
            if(model.size()<16&&sum()<=budget&&bytes<=budget-sum()&&std::none_of(model.begin(),model.end(),[&](Record r){return r.slot==slot&&(r.state==State::Reserved||r.state==State::Uploading);})){
                for(std::size_t s=0;s+count<=32;++s){bool free=true;for(auto r:model)if(s<r.offset+r.count&&r.offset<s+count)free=false;if(free){off=s;break;}}
            }auto got=x.reserve(slot,bytes,count);actual=bool(got);ok=bool(off);if(ok){check(got&&got->offset==*off&&got->token==next);model.push_back({next++,slot,bytes,*off,count,State::Reserved,{}});peak=std::max(peak,sum());}break;}
        case 1:ok=!open[n]&&f!=Invalid&&f>submitted[n];actual=x.begin(q,f);if(ok)open[n]=f;break;
        case 2:ok=exists&&open[n]&&i->state==State::Reserved;actual=x.upload(token,q);if(ok){i->state=State::Uploading;i->uses[n]=open[n];}break;
        case 3:ok=exists&&open[n]&&q!=Queue::Copy&&i->state==State::Active;actual=x.use(token,q);if(ok)i->uses[n]=open[n];break;
        case 4:if(rng()%2==0)f=open[n];ok=open[n]&&open[n]==f;actual=x.submit(q,f);if(ok){submitted[n]=f;open[n]=0;}break;
        case 5:if(rng()%2==0)f=submitted[n];ok=f!=Invalid&&f>=completed[n]&&f<=submitted[n];actual=x.observe(q,f);if(ok)completed[n]=f;break;
        case 6:ok=exists&&i->state==State::Uploading&&done(*i);actual=x.publish(token);if(ok){for(auto& r:model)if(r.slot==i->slot&&r.state==State::Active)r.state=State::Retired;i->state=State::Active;}break;
        case 7:ok=exists&&i->state!=State::Retired;actual=x.retire(token);if(ok)i->state=State::Retired;break;
        default:if(rng()%3==0){budget=rng()%200;x.set_budget(budget);ok=actual=true;}else{ok=exists&&i->state==State::Retired&&done(*i);check(x.can_release(token)==ok);actual=x.released(token);if(ok)model.erase(i);}break;
        }
        check(actual==ok);accepted+=actual;std::size_t used=0;
        for(auto r:model){used+=r.count;auto got=x.inspect(r.token);check(got&&got->state==r.state&&got->uses==r.uses&&got->offset==r.offset&&got->bytes==r.bytes&&got->count==r.count&&got->slot==r.slot);}
        for(Id s=1;s<=8;++s){Id expected=0;for(auto r:model)if(r.slot==s&&r.state==State::Active){check(!expected);expected=r.token;}check(x.active(s)==expected);}
        const auto stats=x.stats();check(stats.charged==sum()&&stats.budget==budget&&stats.peak==peak&&stats.entries==model.size()&&stats.descriptors==used&&stats.open==open&&stats.submitted==submitted&&stats.completed==completed);
    }
    std::cout<<"seed="<<seed<<" operations=100000 accepted="<<accepted<<" discrepancies=0\n";
}
int main(){boundary();for(unsigned seed:{64u,4070u,20261125u})stress(seed);}
