#include "RoundWorld.h"
#include <algorithm>
#include <cmath>
namespace round_wonders {
float length(Vec p){return std::sqrt(p.x*p.x+p.y*p.y);}
Vec radial(Vec p){const float n=length(p);return n>1e-4f?p*(1/n):Vec(0,1);}
Vec position(float a,float r){return {180+std::cos(a)*r,180+std::sin(a)*r};}
float wrap(float a){while(a>kPi)a-=kTau;while(a<-kPi)a+=kTau;return a;}
float noise(float n){const float a=std::sin(n*127.1f+311.7f)*43758.5453f;return a-std::floor(a);}
const Arc RoundWorld::arcs[4]={{124,-2.85f,-.25f,false},{126,.35f,2.8f,false},{83,-1.8f,1.55f,false},{142,2.95f,-2.92f,true}};
const Ladder RoundWorld::ladders[5]={{-2.3f,kGround,124},{-.55f,kGround,124},{1.95f,kGround,126},{-1.45f,124,83},{.8f,126,83}};
bool RoundWorld::on_arc(float a,const Arc& arc){a=wrap(a);return arc.start<arc.end?a>=arc.start&&a<=arc.end:a>=arc.start||a<=arc.end;}
RoundWorld::RoundWorld(){
  for(unsigned i=0;i<kPeople;i++){
    auto& p=people[i];p.trait=i%6;p.color=i%6;p.direction=i%3?1:-1;p.speed=11+noise(i+4)*10;p.phase=i*.7f;p.cooldown=i*.13f;
    if(i<10){p.angle=-kPi+i*kTau/10;p.radius=kGround;p.support=0;}
    else {p.support=i<17?1:3;const auto& arc=arcs[p.support-1];p.angle=arc.start+(arc.end-arc.start)*(.08f+(i%7)/8.0f);p.radius=arc.radius;}
  }
}
bool RoundWorld::blocked(Vec q)const{
  for(const auto& p:people){const Vec feet=position(p.angle,p.radius),up=radial(feet-Vec(180,180));const Vec head=feet-up*9;const Vec segment=head-feet;
    const Vec delta=q-feet;const float t=std::max(0.0f,std::min(1.0f,(delta.x*segment.x+delta.y*segment.y)/81));
    if(length(q-(feet+segment*t))<5.8f)return true;
  }return false;
}
bool RoundWorld::place(Vec q){
  if(!std::isfinite(q.x)||!std::isfinite(q.y)||length(q-Vec(180,180))>163||blocked(q))return false;
  unsigned count=0;Token* slot=nullptr;Token* oldest=nullptr;
  for(auto& t:tokens){if(!t.active&&!slot)slot=&t;if(t.active&&t.shape==selected){count++;if(!oldest||t.placed<oldest->placed)oldest=&t;}}
  if(count>=6)slot=oldest;if(!slot)return false;
  if(!slot->active){slot->active=true;slot->uid=++next_uid_;slot->cycle=0;}
  slot->shape=selected;slot->point=q;slot->timer=.55f;slot->period=9+selected*1.7f;slot->placed=++edits;return true;
}
bool RoundWorld::move(uint16_t uid,Vec q){
  if(length(q-Vec(180,180))>163||blocked(q))return false;
  for(auto& t:tokens)if(t.active&&t.uid==uid){t.point=q;t.timer=.55f;t.placed=++edits;return true;}return false;
}
int RoundWorld::hit(Vec q)const{int found=-1;float best=14;for(unsigned i=0;i<kTokens;i++)if(tokens[i].active){const float d=length(tokens[i].point-q);if(d<best){found=i;best=d;}}return found;}
int RoundWorld::surface(float angle,float oldRadius,float nextRadius)const{
  int hit=0;float r=kGround;
  for(unsigned i=0;i<4;i++)if(arcs[i].radius>=oldRadius-.2f&&arcs[i].radius<=nextRadius&&on_arc(angle,arcs[i])&&arcs[i].radius<r){hit=i+1;r=arcs[i].radius;}
  return hit;
}
void RoundWorld::step(Person& p,float dt){
  p.cooldown=std::max(0.0f,p.cooldown-dt);p.phase+=dt*p.speed*.52f;
  if(p.rest>0){p.rest-=dt;return;}
  if(p.ladder>=0){
    const auto& ladder=ladders[p.ladder];const bool inward=p.velocity<0;const float target=inward?ladder.inner:ladder.outer;
    p.radius+=p.velocity*dt;
    if((inward&&p.radius<=target)||(!inward&&p.radius>=target)){
      p.radius=target;p.ladder=-1;p.velocity=0;p.support=target==kGround?0:-1;
      for(unsigned i=0;i<4;i++)if(std::abs(arcs[i].radius-target)<.1f&&on_arc(p.angle,arcs[i]))p.support=i+1;
      p.cooldown=2;p.rest=.15f;
    }return;
  }
  if(p.support>=0){
    for(unsigned i=0;i<5;i++){
      const auto& ladder=ladders[i];const float distance=std::abs(wrap(p.angle-ladder.angle))*p.radius;
      if(p.cooldown==0&&distance<3&&(std::abs(p.radius-ladder.outer)<1||std::abs(p.radius-ladder.inner)<1)&&noise(p.phase+p.trait)>.28f){
        p.angle=ladder.angle;p.ladder=i;p.velocity=p.radius>ladder.inner+1?-13:13;p.support=-1;return;
      }
    }
    const float next=wrap(p.angle+p.direction*p.speed*dt/std::max(30.0f,p.radius));
    if(p.support>0&&!on_arc(next,arcs[p.support-1])){
      if(p.trait==1||p.trait==3){p.direction=-p.direction;p.rest=.15f;return;}
      p.support=-1;p.velocity=0;
    }
    p.angle=next;
    if(p.support>0&&arcs[p.support-1].spring){p.support=-1;p.velocity=-65;p.cooldown=.8f;}
  }else{
    const float previous=p.radius;p.velocity+=42*dt;p.radius+=p.velocity*dt;p.angle=wrap(p.angle+p.direction*p.speed*.12f*dt/std::max(28.0f,p.radius));
    if(p.radius<25){p.radius=25;p.velocity=std::max(0.0f,p.velocity);}
    if(p.velocity>0){const int landing=surface(p.angle,previous,p.radius);const float radius=landing?arcs[landing-1].radius:kGround;
      if(p.radius>=radius){p.radius=radius;p.velocity=0;p.support=landing;p.cooldown=.7f;}
    }
  }
}
void RoundWorld::update(float dt){
  if(!std::isfinite(dt)||dt<=0)return;dt=std::min(dt,1.0f/30);time+=dt;
  for(auto& p:people)step(p,dt);
  for(auto& t:tokens)if(t.active){t.timer-=dt;if(t.timer<=0){t.timer+=t.period;t.cycle++;if(queued_<kTokens)pending_[queued_++]={t.shape,t.uid,t.cycle,t.point};}}
}
bool RoundWorld::next_reveal(Reveal& r){if(drained_==queued_){drained_=queued_=0;return false;}r=pending_[drained_++];return true;}
void RoundWorld::release(const Reveal& r,uint64_t starts,uint32_t epoch){
  Effect* slot=nullptr;for(auto& e:effects)if(!e.active){slot=&e;break;}
  if(!slot)slot=&*std::min_element(effects,effects+kEffects,[](const Effect& a,const Effect& b){return a.starts<b.starts;});
  slot->active=true;slot->shape=r.shape;slot->uid=r.uid;slot->cycle=r.cycle;slot->point=r.point;slot->starts=starts;slot->epoch=epoch;
  slot->life=r.shape==2?std::min(8.0f,length(r.point-Vec(180,180))/23+2):7.5f;
}
}
