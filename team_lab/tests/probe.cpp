#include "Controls.h"
#include "RoundWorld.h"
#include "RoundRenderer.h"
#include "Music.h"
#include "Adpcm.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
using namespace round_wonders;
static void advance(RoundWorld& w,float seconds){for(unsigned i=0;i<seconds*60;i++)w.update(1.0f/60);}
static void controls_test(){
 Controls c;c.initialize(false,0);
 assert(c.update(true,100)==ButtonAction::None);assert(c.update(true,126)==ButtonAction::None);
 c.update(false,180);assert(c.update(false,206)==ButtonAction::None);assert(c.update(false,505)==ButtonAction::None);assert(c.update(false,506)==ButtonAction::Shape);
 c.update(true,700);c.update(true,726);c.update(false,760);c.update(false,786);c.update(true,900);c.update(true,926);c.update(false,950);assert(c.update(false,976)==ButtonAction::Theme);assert(c.update(false,1500)==ButtonAction::None);
 c.update(true,2000);c.update(true,2026);assert(c.update(true,3026)==ButtonAction::Mute);assert(c.update(true,5000)==ButtonAction::None);c.update(false,5100);c.update(false,5126);assert(c.update(false,6000)==ButtonAction::None);
 c.initialize(true,0);assert(c.update(true,2000)==ButtonAction::None);c.update(false,2100);c.update(false,2126);assert(c.update(false,3000)==ButtonAction::None);
 c.initialize(false,0);c.update(true,100);c.update(false,110);assert(c.update(false,500)==ButtonAction::None);
 c.update(true,600);c.update(true,626);c.update(false,700);c.update(false,726);assert(c.flush_single());assert(!c.flush_single());assert(c.update(false,2000)==ButtonAction::None);
 // A physical one-second hold includes the debounce interval and fires once.
 c.initialize(false,0);c.update(true,100);c.update(true,126);
 assert(c.update(true,1099)==ButtonAction::None);assert(c.update(true,1100)==ButtonAction::Mute);assert(c.update(true,5000)==ButtonAction::None);
 c.update(false,5100);assert(c.update(false,5126)==ButtonAction::None);assert(c.update(false,6000)==ButtonAction::None);
 c.update(true,6100);c.update(true,6126);assert(c.update(true,7100)==ButtonAction::Mute);
 c.update(false,7200);assert(c.update(false,7226)==ButtonAction::None);assert(c.update(false,8000)==ButtonAction::None);
 // A frame arriving on release must not lose the hold or turn it into a tap.
 c.initialize(false,0);c.update(true,100);c.update(true,126);assert(c.update(false,1100)==ButtonAction::None);
 assert(c.update(false,1140)==ButtonAction::Mute);assert(c.update(false,1500)==ButtonAction::None);
 // Debouncing a release just below the threshold must not manufacture a hold.
 c.initialize(false,0);c.update(true,100);c.update(true,126);assert(c.update(false,1099)==ButtonAction::None);
 assert(c.update(false,1125)==ButtonAction::None);assert(c.update(false,1425)==ButtonAction::Shape);
 // A held second press overrides double-click; touch cannot flush a stray shape.
 c.initialize(false,0);c.update(true,100);c.update(true,126);c.update(false,180);c.update(false,206);
 c.update(true,300);c.update(true,326);assert(!c.flush_single());assert(c.update(true,1300)==ButtonAction::Mute);
 c.update(false,1400);assert(c.update(false,1426)==ButtonAction::None);assert(c.update(false,1800)==ButtonAction::None);
}
static void world_test(){
 RoundWorld w;auto& p=w.people[0];p.angle=kPi-.001f;p.direction=1;p.cooldown=100;w.update(1.0f/30);assert(p.angle<0);
 p.angle=-.1f;p.radius=90;p.support=-1;p.ladder=-1;p.velocity=0;advance(w,.5f);assert(p.radius>94);advance(w,4);assert(p.radius==kGround&&p.support==0);
 p.angle=-.251f;p.radius=124;p.support=1;p.trait=3;p.direction=1;p.rest=0;p.cooldown=100;w.update(1.0f/30);assert(p.direction==-1&&p.support==1);
 p.angle=-.251f;p.trait=0;p.direction=1;p.rest=0;w.update(1.0f/30);assert(p.support==-1);
 p.angle=-2.3f;p.radius=kGround;p.support=0;p.ladder=-1;p.cooldown=0;p.speed=0;p.rest=0;p.phase=1;while(noise(p.phase)<.3f)p.phase+=.1f;
 w.update(1.0f/60);assert(p.ladder==0);advance(w,4);assert(p.radius==124);
 RoundWorld inventory;
 for(unsigned shape=0;shape<6;shape++){inventory.selected=shape;for(int copy=0;copy<6;copy++)assert(inventory.place({180,180}));}
 inventory.selected=0;assert(inventory.place({180,190}));assert(std::count_if(inventory.tokens,inventory.tokens+kTokens,[](const Token& t){return t.active;})==36);
 advance(inventory,.6f);Reveal event;unsigned count=0;while(inventory.next_reveal(event)){inventory.release(event,24000,1);count++;}assert(count==36);
 for(int frame=0;frame<36000;frame++){
  inventory.update(1.0f/60);while(inventory.next_reveal(event))inventory.release(event,frame*400,1);
  for(const auto& person:inventory.people){assert(std::isfinite(person.angle)&&std::isfinite(person.radius));assert(person.angle>=-kPi&&person.angle<=kPi);assert(person.radius>=25&&person.radius<=kGround+.01f);}
 }
 for(float angle:{0.0f,1.0f,2.0f,3.0f}){const Vec q=position(angle,140),a=RoundRenderer::inward(q,40),b=RoundRenderer::inward(q,200);assert(std::abs(length(a-Vec(180,180))-100)<.001);assert(length(b-Vec(180,180))<.001);}
}
static std::vector<uint8_t> load(const char* path){std::ifstream f(path,std::ios::binary);assert(f.good());return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
static void decoder_test(){
 // Cross-language fixture: JS decode() FNV-1a over little-endian decoded PCM.
 uint8_t bytes[132];for(unsigned i=0;i<132;i++)bytes[i]=(i*57+13)&255;
 bytes[0]=0xd0;bytes[1]=0x8a;bytes[2]=40;bytes[3]=0;
 int16_t out[256];decode_adpcm(bytes,out);uint32_t hash=2166136261U;
 for(auto n:out){hash=(hash^(static_cast<uint16_t>(n)&255))*16777619U;hash=(hash^(static_cast<uint16_t>(n)>>8))*16777619U;}
 assert(hash==3627484514U);
}
static void render_until(Music& mixer,uint64_t end){int16_t block[256];while(mixer.produced<end)mixer.render(block,static_cast<unsigned>(std::min<uint64_t>(256,end-mixer.produced)));}
static void music_test(const std::vector<uint8_t>& blob,const std::string& directory){
 assert(blob.size()==kAssetBytes);std::vector<int16_t> memory(kBankSamples);Bank bank;bank.memory=memory.data();Music mixer;
 for(unsigned theme=0;theme<6;theme++){
  bank.begin(theme);while(!bank.load(blob.data())){}assert(bank.used<=kBankSamples);mixer.initialize(blob.data(),&bank);
  auto cue=next_entrance(theme,0,512,mixer.epoch);for(unsigned id=0;id<6;id++)assert(mixer.schedule(id,id+1,1,cue));assert(mixer.phrases==3);
  int16_t block[256];int peak=0;double power=0;std::ofstream wav(directory+"/theme-"+std::to_string(theme)+".pcm",std::ios::binary);
  for(unsigned b=0;b<24000*5/256;b++){mixer.render(block,256);wav.write(reinterpret_cast<char*>(block),sizeof(block));for(auto sample:block){peak=std::max(peak,std::abs(static_cast<int>(sample)));power+=sample*static_cast<double>(sample);}}
  assert(peak>1000&&peak<32767);assert(power/(24000*5)>1000);mixer.mute(true);for(int i=0;i<6;i++)mixer.render(block,256);assert(std::all_of(block,block+256,[](int16_t n){return n==0;}));
  const auto mutedAt=mixer.produced,origin=mixer.origin;const auto epoch=mixer.epoch;
  mixer.mute(false);bool audible=false;for(int i=0;i<94;i++){mixer.render(block,256);audible|=std::any_of(block,block+256,[](int16_t n){return std::abs(static_cast<int>(n))>10;});}
  assert(audible&&mixer.produced==mutedAt+94*256&&mixer.origin==origin&&mixer.epoch==epoch&&mixer.theme==theme);
  std::printf("[native] theme=%u peak=%d rms=%.1f\n",theme,peak,std::sqrt(power/(24000*5)));
  // History must not reserve silence for absent contenders, in any theme.
  mixer.initialize(blob.data(),&bank);cue=next_entrance(theme,mixer.origin,512,mixer.epoch);
  for(unsigned id=0;id<6;id++)assert(mixer.schedule(id,id+1,1,cue));
  render_until(mixer,static_cast<uint64_t>(16*60*kAudioRate/kThemes[theme].bpm));
  cue=next_entrance(theme,mixer.origin,mixer.produced+512,mixer.epoch);
  for(unsigned id=0;id<3;id++){assert(mixer.schedule(id,id+1,2,cue));assert(mixer.foreground(id+1,cue.when));}
  assert(mixer.phrases==6);
  // Only an actual same-beat contender can reassign a pending reservation.
  assert(mixer.schedule(3,4,2,cue));assert(!mixer.foreground(1,cue.when));assert(mixer.foreground(4,cue.when));assert(mixer.phrases==6);
  render_until(mixer,cue.when+1);
  const auto later=next_entrance(theme,mixer.origin,mixer.produced+512,mixer.epoch);
  assert(mixer.schedule(4,5,2,later));assert(!mixer.foreground(5,later.when));assert(mixer.phrases==6);
  assert(mixer.foreground(4,cue.when)); // Already playing phrases are not interrupted.
  // Subtract matching backing mixes to measure the actual accent signal.
  std::vector<Music> comparison(4);for(auto& player:comparison)player.initialize(blob.data(),&bank);
  cue=next_entrance(theme,0,512,comparison[0].epoch);
  for(unsigned i=0;i<2;i++)for(unsigned id:{0U,1U,3U})assert(comparison[i].schedule(id,id+1,1,cue));
  assert(comparison[0].schedule(2,3,1,cue));assert(!comparison[0].foreground(3,cue.when));
  assert(comparison[2].schedule(2,3,1,cue));assert(comparison[2].foreground(3,cue.when));
  std::vector<int16_t> opening[4];for(unsigned i=0;i<4;i++){render_until(comparison[i],cue.when);opening[i].resize(1920);comparison[i].render(opening[i].data(),1920);}
  double accentPower=0,phrasePower=0;for(unsigned i=0;i<1920;i++){accentPower+=std::pow(opening[0][i]-opening[1][i],2);phrasePower+=std::pow(opening[2][i]-opening[3][i],2);}
  const double ratio=std::sqrt(accentPower/phrasePower);assert(ratio>.3&&ratio<.8);
  std::printf("[native] theme=%u accent/phrase opening RMS=%.2f\n",theme,ratio);
 }
 bank.begin(0);while(!bank.load(blob.data())){}mixer=Music{};mixer.initialize(blob.data(),&bank);bool heard[36]={};
 for(unsigned cycle=1;cycle<=12;cycle++){
  const auto cue=next_entrance(0,mixer.origin,mixer.produced+512,mixer.epoch);
  for(unsigned copy=0;copy<36;copy++)assert(mixer.schedule(copy%6,copy+1,cycle,cue));
  unsigned foreground=0;for(unsigned copy=0;copy<36;copy++)if(mixer.foreground(copy+1,cue.when)){heard[copy]=true;foreground++;}
  assert(foreground==3); // Count final winners, not provisional reservations.
  int16_t block[256];const auto end=mixer.produced+static_cast<uint64_t>(16*60*24000/84);while(mixer.produced<end)mixer.render(block,256);
 }
 assert(std::all_of(heard,heard+36,[](bool n){return n;}));assert(mixer.late==0);
 std::vector<int16_t> otherMemory(kBankSamples);Bank other;other.memory=otherMemory.data();other.begin(5);while(!other.load(blob.data())){}
 const auto oldEpoch=mixer.epoch,before=mixer.theme;const auto produced=mixer.produced;
 const auto stale=next_entrance(before,mixer.origin,produced+24000,oldEpoch);
 mixer.transition(&other);assert(mixer.theme==before&&mixer.epoch==oldEpoch);
 int16_t block[1024];mixer.render(block,1024);
 assert(mixer.theme==5&&mixer.active_bank()==&other&&mixer.epoch==oldEpoch+1);
 assert(mixer.origin>produced&&mixer.origin<produced+1024);
 assert(!mixer.schedule(0,1,1,stale));
 const auto fresh=next_entrance(5,mixer.origin,mixer.produced+512,mixer.epoch);assert(mixer.schedule(0,1,1,fresh));
}
static void previews(const std::string& directory){
 constexpr unsigned count=360*360;std::vector<uint16_t> background(count),guarded(count+64,0xf81f);auto* pixels=guarded.data()+32;
 RoundRenderer renderer;renderer.background(background.data());RoundWorld world;
 auto save=[&](const std::string& file){std::ofstream out(directory+"/"+file+".rgb565",std::ios::binary);out.write(reinterpret_cast<char*>(pixels),count*2);assert(std::all_of(guarded.begin(),guarded.begin()+32,[](uint16_t n){return n==0xf81f;}));assert(std::all_of(guarded.end()-32,guarded.end(),[](uint16_t n){return n==0xf81f;}));};
 renderer.draw(world,pixels,background.data(),0);save("world");
 for(unsigned id=0;id<6;id++){
  world=RoundWorld{};Reveal reveal={static_cast<uint8_t>(id),1,1,position(.6f,110)};world.release(reveal,0,1);
  for(unsigned frame=0;frame<4;frame++){renderer.draw(world,pixels,background.data(),static_cast<uint64_t>((frame==0?.18:frame==1?1.3:frame==2?3.0:6.8)*24000));save("effect-"+std::to_string(id)+"-"+std::to_string(frame));}
 }
 world=RoundWorld{};for(unsigned shape=0;shape<6;shape++){Reveal reveal={static_cast<uint8_t>(shape),static_cast<uint16_t>(shape+1),1,position(shape*kTau/6,125)};world.release(reveal,0,1);}renderer.draw(world,pixels,background.data(),48000);save("all-six");
}
int main(int argc,char** argv){assert(argc==3);controls_test();world_test();decoder_test();const auto blob=load(argv[1]);music_test(blob,argv[2]);previews(argv[2]);std::puts("[native] Controls, radial physics, inventory, rendering bounds, ADPCM compatibility, six themes, theme transitions, and 36-copy foreground fairness passed.");}
