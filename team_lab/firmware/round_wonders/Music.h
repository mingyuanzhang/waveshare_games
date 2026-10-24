#pragma once
#include "AudioAssets.h"
#include <stdint.h>
#include <stddef.h>
namespace round_wonders {
struct Bank {
  int16_t* memory=nullptr;const int16_t* samples[kSampleCount]={};
  int theme=-1;unsigned item=0,frame=0,used=0;bool ready=false;
  void begin(int target);bool load(const uint8_t* blob,unsigned budget=2048);
};
struct MusicCue {uint64_t when=0;uint32_t epoch=0;uint16_t phase=0;float beat=0;};
MusicCue next_entrance(unsigned theme,uint64_t origin,uint64_t earliest,uint32_t epoch);
class Music {
 public:
  void initialize(const uint8_t* blob,Bank* bank);
  void switch_bank(Bank* bank);
  void transition(Bank* bank){pendingBank_=bank;}
  Bank* active_bank()const{return bank_;}
  bool schedule(unsigned shape,unsigned uid,unsigned cycle,const MusicCue& cue);
  bool foreground(unsigned uid,uint64_t when)const;
  void render(int16_t* output,unsigned frames);
  void mute(bool value){muted_=value;if(value)clear();}
  void clear();
  uint64_t produced=0,origin=0;uint32_t epoch=1;unsigned theme=0;
  unsigned phrases=0,accents=0,coalesced=0,late=0,stolen=0;
 private:
  struct Voice {const int16_t* pcm=nullptr;uint32_t length=0,phase=0,step=0,age=0,gate=0;int gain=0;};
  struct Phrase {bool active=false,full=false,heardBefore=false;uint8_t shape=0;uint16_t count=0,next=0;unsigned uid=0;uint32_t note=0;uint64_t when=0,end=0,lastBefore=0;};
  struct History {unsigned uid=0;uint64_t seen=0,last=0;bool heard=false;};
  const uint8_t* blob_=nullptr;Bank* bank_=nullptr;Bank* pendingBank_=nullptr;Voice voices_[32];Phrase phrases_[24];History history_[72];
  int16_t background_[256]={},room_[2048]={};uint32_t backgroundFrame_=0,roomIndex_=0;int master_=0;bool muted_=false;uint64_t nextEvent_=0;
  uint16_t word(uint32_t offset)const;uint32_t dword(uint32_t offset)const;
  void phrase_end(Phrase& phrase);void update_next();void start_notes();void note(uint32_t offset,bool accent);int next_sample();
};
}
