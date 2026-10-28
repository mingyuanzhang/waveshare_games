#include "RoundDevice.h"
#include "RoundWorld.h"
#include "RoundRenderer.h"
#include "Controls.h"
#include "DeviceAudio.h"
#include "Display_ST77916.h"
#include "DisplaySync.h"
#include "I2C_Driver.h"
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_timer.h"
using namespace round_wonders;
namespace {
constexpr uint8_t kActionButtonPin=0; // The physical BOOT/action button; active low.
RoundWorld world,savedWorld;RoundRenderer renderer;Controls controls;DeviceAudio audio;
uint16_t *pixels=nullptr,*backgroundPixels=nullptr,*strip=nullptr;
bool audioReady=false,finger=false,dragged=false,benchmark=false;int heldToken=-1;Vec press,lastTouch;
uint64_t lastTouchMs=0,lastFrameMs=0,lastWorldUs=0,lastStatsMs=0,toastUntil=0,benchStart=0;
float accumulator=0;const char* toast=nullptr;unsigned frames=0,overBudget=0,maximum=0,renderMaximum=0,transferMaximum=0;
uint64_t totalUs=0;unsigned histogram[128]={};
const char* names[]={"CIRCLE","TRIANGLE","DIAMOND","SQUARE","HEXAGON","CRESCENT"};
uint64_t now_ms(){return static_cast<uint64_t>(esp_timer_get_time())/1000;}
AudioState sound_state(){if(audioReady)return audio.state();AudioState state;state.now=static_cast<uint64_t>(esp_timer_get_time())*24/1000;state.produced=state.now;state.epoch=1;return state;}
void show(const char* text,uint64_t now){toast=text;toastUntil=now+1100;}
void next_shape(uint64_t now){world.selected=(world.selected+1)%6;show(names[world.selected],now);}
void toggle_sound(uint64_t now){
  if(!audioReady){show("SOUND UNAVAILABLE",now);return;}
  bool muted;if(audio.toggle_mute(muted))show(muted?"SOUND OFF":"SOUND ON",now);
}
bool read_touch(bool& down,Vec& point){
  Wire.beginTransmission(uint8_t(CST816_ADDR));Wire.write(uint8_t(CST816_REG_GestureID));
  if(Wire.endTransmission(true)!=0||Wire.requestFrom(uint8_t(CST816_ADDR),uint8_t(6))!=6)return false;
  uint8_t data[6];for(auto& byte:data)byte=Wire.read();down=(data[1]&15)!=0;point={float(((data[2]&15)<<8)|data[3]),float(((data[4]&15)<<8)|data[5])};return true;
}
void poll_touch(uint64_t now){
  if(now-lastTouchMs<10)return;lastTouchMs=now;bool down;Vec q;if(!read_touch(down,q))return;
  if(down&&!finger){if(controls.flush_single())next_shape(now);finger=true;dragged=false;press=lastTouch=q;heldToken=world.hit(q);}
  else if(down&&finger){lastTouch=q;if(length(q-press)>7)dragged=true;}
  else if(!down&&finger){
    finger=false;const bool ok=dragged&&heldToken>=0?world.move(world.tokens[heldToken].uid,lastTouch):world.place(lastTouch);
    if(!ok)show("TRY A FREE SPOT",now);heldToken=-1;
  }
}
void present(const AudioState& state,uint64_t now){
  const int64_t began=esp_timer_get_time();const float alpha=now<toastUntil?std::min(1.0f,(toastUntil-now)/300.0f):0;
  renderer.draw(world,pixels,backgroundPixels,state.now,toast,alpha,5);const int64_t rendered=esp_timer_get_time();
  for(int y=0;y<360;y+=40){std::memcpy(strip,pixels+y*360,360*40*2);LCD_addWindow(0,y,359,y+39,strip);}
  const unsigned elapsed=esp_timer_get_time()-began;frames++;totalUs+=elapsed;maximum=std::max(maximum,elapsed);
  renderMaximum=std::max(renderMaximum,static_cast<unsigned>(rendered-began));transferMaximum=std::max(transferMaximum,static_cast<unsigned>(esp_timer_get_time()-rendered));
  if(elapsed>40000)overBudget++;histogram[std::min(127U,elapsed/1000)]++;
}
void stats(const char* tag){
  if(!Serial)return;
  const auto state=sound_state();unsigned tally=0,p95=0;for(unsigned bin=0;bin<128;bin++){tally+=histogram[bin];if(tally*100>=frames*95){p95=bin+1;break;}}
  Serial.printf("[round] %s frames=%u avg_us=%llu max_us=%u p95_ms=%u over40=%u render_max_us=%u transfer_max_us=%u audio_underruns=%u audio_slow=%u audio_max_us=%u late_cues=%u theme=%u free_internal=%u free_psram=%u\n",tag,frames,frames?totalUs/frames:0,maximum,p95,overBudget,renderMaximum,transferMaximum,state.underruns,state.slow,state.maximumUs,state.lateCues,state.theme,heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
void begin_benchmark(uint64_t now){
  savedWorld=world;world=RoundWorld{};world.edits=0;
  for(unsigned shape=0;shape<6;shape++)for(unsigned copy=0;copy<6;copy++){
    world.selected=shape;for(unsigned attempt=0;attempt<12;attempt++)if(world.place(position((copy*6+shape)*2.39996f+attempt*.15f,35+copy*23)))break;
  }
  world.selected=0;for(auto& t:world.tokens)if(t.active)t.timer=.15f;
  frames=overBudget=maximum=renderMaximum=transferMaximum=0;totalUs=0;std::fill(histogram,histogram+128,0);benchmark=true;benchStart=now;Serial.printf("[round] BENCH BEGIN duration=%ums people=24 tokens=36 audio=on\n",kThemeCount*6500+3000);
}
}
void round_wonders_setup(){
  Serial.begin(115200);Serial.setTxTimeoutMs(10);pinMode(kActionButtonPin,INPUT_PULLUP);controls.initialize(digitalRead(kActionButtonPin)==LOW,now_ms());pinMode(9,OUTPUT);digitalWrite(9,LOW);
  pixels=static_cast<uint16_t*>(heap_caps_malloc(360*360*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));backgroundPixels=static_cast<uint16_t*>(heap_caps_malloc(360*360*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  strip=static_cast<uint16_t*>(heap_caps_malloc(360*40*2,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA));ESP_ERROR_CHECK(pixels&&backgroundPixels&&strip?ESP_OK:ESP_ERR_NO_MEM);
  echo_lcd_sync_init();I2C_Init();Wire.setClock(400000);Wire.setTimeOut(10);Backlight_Init();Set_Backlight(0);LCD_Init();renderer.background(backgroundPixels);
  audioReady=audio.begin();if(!audioReady)Serial.println("[round] Audio unavailable; world continues silently");
  lastWorldUs=esp_timer_get_time();lastFrameMs=now_ms();show("HOLD BOOT 1S: SOUND",lastFrameMs);toastUntil=lastFrameMs+2400;present(sound_state(),lastFrameMs);Set_Backlight(60);
  Serial.printf("[round] READY radial world; action/BOOT click=shape double=music style hold 1s=sound on/off; no SD; audio=%s\n",audioReady?"ready":"failed");
}
void round_wonders_loop(){
  const uint64_t now=now_ms();
  while(Serial.available()){
    const char command=Serial.read();if(command=='B'&&!benchmark)begin_benchmark(now);if(command=='S')next_shape(now);if(command=='T'&&audioReady)audio.next_theme();if(command=='M')toggle_sound(now);if(command=='?')stats("STATUS");
  }
  const auto action=controls.update(digitalRead(kActionButtonPin)==LOW,now);
  if(action==ButtonAction::Shape)next_shape(now);
  if(action==ButtonAction::Theme&&audioReady){audio.next_theme();show(kThemes[audio.desired_theme()].name,now);}
  if(action==ButtonAction::Mute)toggle_sound(now);
  poll_touch(now);if(audioReady){audio.pump();if(audio.loading())show(kThemes[audio.desired_theme()].name,now);}
  auto sound=sound_state();const uint64_t currentUs=esp_timer_get_time();accumulator+=std::min(.1f,(currentUs-lastWorldUs)/1000000.0f);lastWorldUs=currentUs;
  while(accumulator>=1.0f/60){world.update(1.0f/60);accumulator-=1.0f/60;}
  Reveal reveal;while(world.next_reveal(reveal)){MusicCue cue;if(audioReady)audio.reveal(reveal.shape,reveal.uid,reveal.cycle,cue);else{cue.when=sound.now;cue.epoch=sound.epoch;}world.release(reveal,cue.when,cue.epoch);}
  for(auto& effect:world.effects)if(effect.active){
    if(effect.epoch!=sound.epoch){if(effect.starts>sound.now&&audioReady){MusicCue cue;audio.reveal(effect.shape,effect.uid,effect.cycle,cue);effect.starts=cue.when;}effect.epoch=sound.epoch;}
    if(sound.now>=effect.starts&&sound.now-effect.starts>static_cast<uint64_t>(effect.life*24000))effect.active=false;
  }
  if(now-lastFrameMs>=40){present(sound,now);lastFrameMs=now;}
  if(now-lastStatsMs>=5000){stats("STATS");lastStatsMs=now;}
  if(benchmark){
    const unsigned target=std::min(kThemeCount-1,static_cast<unsigned>((now-benchStart)/6500));if(audioReady&&audio.desired_theme()!=target)audio.next_theme();
    if(now-benchStart>=kThemeCount*6500+3000){stats("BENCH END");world=savedWorld;benchmark=false;}
  }
  delay(1);
}
