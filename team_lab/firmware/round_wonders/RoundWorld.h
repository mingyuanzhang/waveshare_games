#pragma once
#include <stdint.h>
#include <stddef.h>
namespace round_wonders {
constexpr float kPi=3.14159265358979323846f,kTau=kPi*2,kGround=168;
constexpr unsigned kPeople=24,kTokens=36,kEffects=24;
struct Vec {float x=0,y=0;Vec(){} Vec(float a,float b):x(a),y(b){} Vec operator+(Vec b)const{return {x+b.x,y+b.y};} Vec operator-(Vec b)const{return {x-b.x,y-b.y};} Vec operator*(float s)const{return {x*s,y*s};}};
float length(Vec p);Vec radial(Vec p);Vec position(float angle,float radius);float wrap(float angle);float noise(float n);
struct Arc {float radius,start,end;bool spring=false;};
struct Ladder {float angle,outer,inner;};
struct Person {float angle=0,radius=kGround,velocity=0,direction=1,speed=14,phase=0,rest=0,cooldown=0;int support=0,ladder=-1;uint8_t color=0,trait=0;};
struct Token {bool active=false;uint8_t shape=0;uint16_t uid=0,cycle=0;Vec point;float timer=.55f,period=9;uint32_t placed=0;};
struct Effect {bool active=false;uint8_t shape=0;uint16_t uid=0,cycle=0;Vec point;uint64_t starts=0;float life=7;uint32_t epoch=0;};
struct Reveal {uint8_t shape;uint16_t uid,cycle;Vec point;};
class RoundWorld {
 public:
  RoundWorld();
  Person people[kPeople];Token tokens[kTokens];Effect effects[kEffects];
  static const Arc arcs[4];static const Ladder ladders[5];
  uint8_t selected=0;float time=0;uint32_t edits=0;
  void update(float dt);
  bool place(Vec point);bool move(uint16_t uid,Vec point);int hit(Vec point)const;bool blocked(Vec point)const;
  bool next_reveal(Reveal& reveal);
  void release(const Reveal& reveal,uint64_t start,uint32_t epoch);
  static bool on_arc(float angle,const Arc& arc);
 private:
  uint16_t next_uid_=0;Reveal pending_[kTokens];unsigned queued_=0,drained_=0;
  void step(Person& person,float dt);int surface(float angle,float oldRadius,float nextRadius)const;
};
}
