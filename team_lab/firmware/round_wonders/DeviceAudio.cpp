#include "DeviceAudio.h"
#include "es8311.h"
#include <Arduino.h>
#include <Wire.h>
#include <algorithm>
#include "esp_heap_caps.h"
#include "esp_timer.h"
extern "C" const uint8_t WonderAudioBlob[];
namespace round_wonders {
bool DeviceAudio::begin(){
  for(auto& bank:banks_){bank.memory=static_cast<int16_t*>(heap_caps_malloc(kBankSamples*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!bank.memory)return false;}
  banks_[0].begin(0);while(!banks_[0].load(WonderAudioBlob,8192))delay(1);
  music_.initialize(WonderAudioBlob,&banks_[0]);published_.bank=&banks_[0];
  queue_=xQueueCreate(64,sizeof(Command));if(!queue_)return false;
  i2s_chan_config_t channel=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
  channel.dma_desc_num=3;channel.dma_frame_num=256;channel.auto_clear_after_cb=true;
  if(i2s_new_channel(&channel,&channel_,nullptr)!=ESP_OK)return false;
  i2s_std_config_t config={};config.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(24000);
  config.clk_cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
  config.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO);
  config.slot_cfg.slot_mask=I2S_STD_SLOT_LEFT;
  config.gpio_cfg.mclk=GPIO_NUM_2;config.gpio_cfg.bclk=GPIO_NUM_48;config.gpio_cfg.ws=GPIO_NUM_38;config.gpio_cfg.dout=GPIO_NUM_47;config.gpio_cfg.din=I2S_GPIO_UNUSED;
  if(i2s_channel_init_std_mode(channel_,&config)!=ESP_OK)return false;
  i2s_event_callbacks_t callbacks={};callbacks.on_sent=sent;
  if(i2s_channel_register_event_callback(channel_,&callbacks,this)!=ESP_OK)return false;
  uint8_t address=0;for(uint8_t candidate:{uint8_t(0x18),uint8_t(0x19),uint8_t(0x30)}){Wire.beginTransmission(candidate);if(Wire.endTransmission()==0){address=candidate;break;}}
  if(!address){Serial.println("[round] ES8311 did not acknowledge its I2C address");return false;}
  auto codec=es8311_create(I2C_NUM_0,address);if(!codec)return false;
  es8311_clock_config_t clocks={false,false,true,24000*256,24000};
  if(es8311_init(codec,&clocks,ES8311_RESOLUTION_16,ES8311_RESOLUTION_16)!=ESP_OK)return false;
  if(es8311_voice_volume_set(codec,85,nullptr)!=ESP_OK)return false;
  es8311_microphone_config(codec,false);
  int16_t initial[768];music_.render(initial,768);size_t loaded=0;
  if(i2s_channel_preload_data(channel_,initial,sizeof(initial),&loaded)!=ESP_OK||loaded!=sizeof(initial))return false;
  publish();lastDmaUs_=esp_timer_get_time();
  if(i2s_channel_enable(channel_)!=ESP_OK)return false;
  pinMode(9,OUTPUT);digitalWrite(9,HIGH);
  if(xTaskCreatePinnedToCore(run,"wonder-audio",8192,this,5,nullptr,0)!=pdPASS)return false;
  Serial.printf("[round] Audio ready: ES8311=0x%02x 24000Hz mono; assets=%u; two banks=%u bytes\n",address,kAssetBytes,kBankSamples*4);
  return true;
}
bool IRAM_ATTR DeviceAudio::sent(i2s_chan_handle_t,i2s_event_data_t* event,void* context){
  auto* self=static_cast<DeviceAudio*>(context);portENTER_CRITICAL_ISR(&self->lock_);
  self->played_+=event->size/2;self->lastDmaUs_=esp_timer_get_time();
  if(self->played_>self->published_.produced&&self->played_>24000)self->published_.underruns++;
  portEXIT_CRITICAL_ISR(&self->lock_);return false;
}
void DeviceAudio::publish(){portENTER_CRITICAL(&lock_);published_.bank=music_.active_bank();published_.produced=music_.produced;published_.origin=music_.origin;published_.epoch=music_.epoch;published_.theme=music_.theme;published_.lateCues=music_.late;portEXIT_CRITICAL(&lock_);}
AudioState DeviceAudio::state(){
  portENTER_CRITICAL(&lock_);AudioState result=published_;const uint64_t played=played_,stamp=lastDmaUs_;portEXIT_CRITICAL(&lock_);
  const uint64_t now=esp_timer_get_time();result.now=played+std::min(uint64_t(255),(now>stamp?(now-stamp)*24/1000:0));return result;
}
void DeviceAudio::run(void* self){static_cast<DeviceAudio*>(self)->loop();}
void DeviceAudio::loop(){
  int16_t block[256];Command command;
  for(;;){
    while(xQueueReceive(queue_,&command,0)==pdTRUE){
      if(command.kind==1){music_.transition(command.bank);}
      else if(command.kind==2){music_.mute(command.mute);portENTER_CRITICAL(&lock_);published_.muted=command.mute;portEXIT_CRITICAL(&lock_);}
      else music_.schedule(command.shape,command.uid,command.cycle,command.cue);
    }
    const int64_t began=esp_timer_get_time();music_.render(block,256);const unsigned elapsed=esp_timer_get_time()-began;
    portENTER_CRITICAL(&lock_);published_.maximumUs=std::max(published_.maximumUs,elapsed);if(elapsed>10666)published_.slow++;portEXIT_CRITICAL(&lock_);
    publish();size_t written=0;
    if(i2s_channel_write(channel_,block,sizeof(block),&written,1000)!=ESP_OK||written!=sizeof(block)){portENTER_CRITICAL(&lock_);published_.underruns++;portEXIT_CRITICAL(&lock_);}
  }
}
bool DeviceAudio::reveal(unsigned shape,unsigned uid,unsigned cycle,MusicCue& cue){
  const auto snapshot=state();cue=next_entrance(snapshot.theme,snapshot.origin,std::max(snapshot.produced+512,snapshot.now+1024),snapshot.epoch);
  Command command;command.shape=shape;command.uid=uid;command.cycle=cycle;command.cue=cue;
  return xQueueSend(queue_,&command,0)==pdTRUE;
}
void DeviceAudio::next_theme(){desired_=(desired_+1)%kThemeCount;}
bool DeviceAudio::toggle_mute(bool& muted){
  Command command;command.kind=2;command.mute=!desiredMuted_;
  if(xQueueSend(queue_,&command,0)!=pdTRUE)return false;
  muted=desiredMuted_=command.mute;return true;
}
void DeviceAudio::pump(){
  const auto current=state();
  if(waiting_){if(current.bank==&banks_[loadBank_])waiting_=false;else return;}
  if(!loading_&&desired_!=current.theme){loadBank_=current.bank==&banks_[0]?1:0;banks_[loadBank_].begin(desired_);loading_=true;}
  if(loading_){
    if(static_cast<unsigned>(banks_[loadBank_].theme)!=desired_)banks_[loadBank_].begin(desired_);
    const int64_t deadline=esp_timer_get_time()+5000;
    while(!banks_[loadBank_].load(WonderAudioBlob,4096)&&esp_timer_get_time()<deadline){}
    if(banks_[loadBank_].ready){
      Command command;command.kind=1;command.bank=&banks_[loadBank_];
      if(xQueueSend(queue_,&command,0)==pdTRUE){loading_=false;waiting_=true;}
    }
  }
}
}
