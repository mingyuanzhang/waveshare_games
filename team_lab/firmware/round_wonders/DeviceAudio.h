#pragma once
#include "Music.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
namespace round_wonders {
struct AudioState {uint64_t now=0,produced=0,origin=0;uint32_t epoch=0;unsigned theme=0,underruns=0,slow=0,maximumUs=0,lateCues=0;bool muted=false;Bank* bank=nullptr;};
class DeviceAudio {
 public:
  bool begin();void pump();bool reveal(unsigned shape,unsigned uid,unsigned cycle,MusicCue& result);
  void next_theme();bool toggle_mute(bool& muted);AudioState state();unsigned desired_theme()const{return desired_;}bool loading()const{return loading_||waiting_;}
 private:
  struct Command {uint8_t kind=0,shape=0;uint16_t uid=0,cycle=0;MusicCue cue;Bank* bank=nullptr;bool mute=false;};
  Bank banks_[2];Music music_;i2s_chan_handle_t channel_=nullptr;QueueHandle_t queue_=nullptr;
  portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;AudioState published_;uint64_t played_=0,lastDmaUs_=0;
  unsigned desired_=0,loadBank_=1;bool loading_=false,waiting_=false,desiredMuted_=false;
  static void run(void* self);static bool IRAM_ATTR sent(i2s_chan_handle_t,i2s_event_data_t*,void* self);
  void publish();void loop();
};
}
