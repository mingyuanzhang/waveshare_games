#include "Music.h"
#include "Adpcm.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace round_wonders {
void Bank::begin(int target){theme=target;item=frame=used=0;ready=false;std::fill(samples,samples+kSampleCount,nullptr);}
bool Bank::load(const uint8_t* blob,unsigned budget){
  int16_t scratch[256];
  while(item<kSampleCount&&budget>=256){
    const auto& sample=kSamples[item];bool needed=false;for(unsigned v=0;v<kThemes[theme].voiceCount;v++)needed|=kThemes[theme].voices[v]==sample.voice;
    if(!needed){item++;continue;}
    if(frame==0)samples[item]=memory+used;
    const unsigned count=std::min<uint32_t>(256,sample.frames-frame);decode_adpcm(blob+sample.offset+(frame/256)*132,scratch,count);
    std::memcpy(memory+used,scratch,count*2);frame+=count;used+=count;budget-=256;
    if(frame==sample.frames){item++;frame=0;}
  }
  ready=item==kSampleCount;return ready;
}
static double groove(double beat,float swing){const double whole=std::floor(beat),part=beat-whole;return whole+(part<.5?part*2*swing:swing+(part-.5)*2*(1-swing));}
static double straight(double beat,float swing){const double whole=std::floor(beat),part=beat-whole;return whole+(part<swing?part/(2*swing):.5+(part-swing)/(2*(1-swing)));}
MusicCue next_entrance(unsigned theme,uint64_t origin,uint64_t earliest,uint32_t epoch){
  const auto& t=kThemes[theme%6];const double position=straight((static_cast<double>(earliest)-origin)*t.bpm/(60*kAudioRate),t.swing);
  const double beat=std::max(0.0,std::ceil(position/t.step-1e-10)*t.step);
  MusicCue cue;cue.beat=beat;cue.when=origin+static_cast<uint64_t>(std::llround(groove(beat,t.swing)*60*kAudioRate/t.bpm));cue.phase=static_cast<unsigned>(std::llround(beat/t.step))%t.phases;cue.epoch=epoch;return cue;
}
uint16_t Music::word(uint32_t o)const{return blob_[o]|(blob_[o+1]<<8);}
uint32_t Music::dword(uint32_t o)const{return word(o)|(static_cast<uint32_t>(word(o+2))<<16);}
void Music::clear(){for(auto& v:voices_)v.pcm=nullptr;for(auto& p:phrases_)p.active=false;for(auto& h:history_)h=History{};nextEvent_=std::numeric_limits<uint64_t>::max();}
void Music::initialize(const uint8_t* data,Bank* bank){blob_=data;produced=origin=0;epoch=0;muted_=false;pendingBank_=nullptr;phrases=accents=coalesced=late=stolen=0;switch_bank(bank);}
void Music::switch_bank(Bank* bank){bank_=bank;theme=bank->theme;origin=produced;backgroundFrame_=0;master_=0;epoch++;clear();std::fill(room_,room_+2048,0);}
void Music::update_next(){
  nextEvent_=std::numeric_limits<uint64_t>::max();
  for(auto& p:phrases_)if(p.active){
    if(p.next<p.count)nextEvent_=std::min(nextEvent_,p.when+static_cast<uint64_t>(word(kNoteOffset+(p.note+p.next)*8))*24);
    else if(produced>=p.end)p.active=false;
  }
}
bool Music::schedule(unsigned shape,unsigned uid,unsigned cycle,const MusicCue& cue){
  if(cue.epoch!=epoch||shape>=6||muted_)return false;
  if(cue.when<produced){late++;return false;}
  unsigned active=0;History* owner=nullptr;History* free=nullptr;History* oldestSeen=&history_[0];
  const uint64_t expiry=static_cast<uint64_t>(32*60*kAudioRate/kThemes[theme].bpm);
  Phrase* shared=nullptr;Phrase* duplicate=nullptr;
  for(auto& p:phrases_)if(p.active&&p.end>cue.when){if(p.full)active++;if(p.shape==shape&&p.when==cue.when){if(!p.full)shared=&p;else duplicate=&p;}}
  for(auto& h:history_){if(h.uid&&cue.when-h.seen>=expiry)h=History{};if(!h.uid&&!free)free=&h;if(h.uid==uid)owner=&h;if(h.seen<oldestSeen->seen)oldestSeen=&h;}
  const bool heard=owner&&owner->heard;const uint64_t last=heard?owner->last:0;
  Phrase* replace=nullptr;
  if(duplicate||active>=3)for(auto& p:phrases_){
    // Only a real contender can take a pending slot; audible phrases finish.
    if((!duplicate||&p==duplicate)&&p.active&&p.full&&p.when==cue.when&&p.next==0&&p.heardBefore&&(!heard||p.lastBefore>last)&&
       (!replace||p.lastBefore>replace->lastBefore))replace=&p;
  }
  const bool full=replace||(!duplicate&&active<3);
  if(replace){
    replace->full=false;replace->count=1;phrase_end(*replace);phrases--;accents++;
    for(auto& h:history_)if(h.uid==replace->uid){h.heard=replace->heardBefore;h.last=replace->lastBefore;break;}
    // Several copies may already share this shape's accent on this entrance.
    for(const auto& p:phrases_)if(&p!=replace&&p.active&&!p.full&&p.shape==replace->shape&&p.when==cue.when){replace->active=false;coalesced++;break;}
  }
  if(!owner)owner=free?free:oldestSeen;owner->uid=uid;owner->seen=cue.when;if(full){owner->heard=true;owner->last=cue.when;}
  if(shared&&!full){coalesced++;return true;}
  Phrase* slot=shared;for(auto& p:phrases_)if(!slot&&(!p.active||p.end<=produced)){slot=&p;break;}
  if(!slot)slot=&*std::min_element(phrases_,phrases_+24,[](const Phrase& a,const Phrase& b){return a.end<b.end;});
  const unsigned variant=((cycle?cycle-1:0)+uid)%3,index=kThemes[theme].first+(cue.phase*6+shape)*3+variant;
  const uint32_t at=kCueOffset+index*8,first=dword(at);const unsigned count=full?word(at+4):1;
  slot->active=true;slot->full=full;slot->heardBefore=heard;slot->lastBefore=last;slot->uid=uid;slot->shape=shape;slot->note=first;slot->count=count;slot->next=0;slot->when=cue.when;
  phrase_end(*slot);
  if(full)phrases++;else accents++;update_next();return true;
}
bool Music::foreground(unsigned uid,uint64_t when)const{
  for(const auto& p:phrases_)if(p.active&&p.full&&p.uid==uid&&p.when==when)return true;
  return false;
}
void Music::phrase_end(Phrase& p){
  unsigned end=0;for(unsigned n=0;n<p.count;n++){const uint32_t at=kNoteOffset+(p.note+n)*8;end=std::max(end,word(at)+(p.full?word(at+2):std::min(320,static_cast<int>(word(at+2))))+100U);}
  p.end=p.when+static_cast<uint64_t>(end)*24;
}
void Music::note(uint32_t offset,bool accent){
  const unsigned midi=blob_[offset+4],instrument=blob_[offset+5],velocity=accent?153:blob_[offset+7];
  unsigned sample=0;int best=10000;
  for(unsigned i=0;i<kSampleCount;i++)if(bank_->samples[i]&&kSamples[i].voice==instrument){
    const int distance=std::abs(static_cast<int>(kSamples[i].key)-static_cast<int>(midi))*16+std::abs(static_cast<int>(kSamples[i].velocity)-static_cast<int>(velocity));
    if(distance<best){sample=i;best=distance;}
  }
  if(best==10000)return;
  Voice* voice=nullptr;for(auto& v:voices_)if(!v.pcm){voice=&v;break;}
  if(!voice){voice=&*std::min_element(voices_,voices_+32,[](const Voice& a,const Voice& b){return a.gain<b.gain;});stolen++;}
  voice->pcm=bank_->samples[sample];voice->length=kSamples[sample].frames;voice->phase=voice->age=0;
  voice->step=static_cast<uint32_t>(std::llround(65536*std::pow(2.0,(static_cast<int>(midi)-kSamples[sample].key)/12.0)));
  voice->gate=accent?std::min(320U,static_cast<unsigned>(word(offset+2)))*24:word(offset+2)*24;
  voice->gain=static_cast<int>(blob_[offset+6])*(accent?42:79); // Accents retain about half the opening note's amplitude.
}
void Music::start_notes(){
  for(auto& p:phrases_)if(p.active)while(p.next<p.count){
    const uint32_t offset=kNoteOffset+(p.note+p.next)*8;const uint64_t when=p.when+static_cast<uint64_t>(word(offset))*24;
    if(when>produced)break;note(offset,!p.full);p.next++;
  }update_next();
}
int Music::next_sample(){
  const auto& track=kThemes[theme];
  if(backgroundFrame_%256==0)decode_adpcm(blob_+track.offset+(backgroundFrame_/256)*132,background_);
  int mixed=(static_cast<int>(background_[backgroundFrame_%256])*19005)>>15;
  if(++backgroundFrame_==track.frames)backgroundFrame_=0;
  for(auto& voice:voices_)if(voice.pcm){
    const unsigned at=voice.phase>>16;if(at+1>=voice.length){voice.pcm=nullptr;continue;}
    const int a=voice.pcm[at],b=voice.pcm[at+1],fraction=voice.phase&65535;
    const int value=a+static_cast<int>((static_cast<int64_t>(b-a)*fraction)>>16);
    int gain=voice.gain;if(voice.age>voice.gate){const uint32_t release=voice.age-voice.gate;if(release>=2400){voice.pcm=nullptr;continue;}gain=static_cast<int>((static_cast<int64_t>(gain)*(2400-release))/2400);}
    mixed+=static_cast<int>((static_cast<int64_t>(value)*gain)>>15);voice.phase+=voice.step;voice.age++;
  }
  const int echo=(room_[(roomIndex_+2048-888)%2048]+room_[(roomIndex_+2048-1464)%2048])/16;
  room_[roomIndex_]=std::max(-32768,std::min(32767,mixed/3));roomIndex_=(roomIndex_+1)%2048;mixed+=echo;
  const int target=muted_||pendingBank_?0:22528;if(master_<target)master_=std::min(target,master_+48);else if(master_>target)master_=std::max(target,master_-48);
  mixed=static_cast<int>((static_cast<int64_t>(mixed)*master_)>>15);
  if(std::abs(mixed)>24000){const int d=std::abs(mixed)-24000;mixed=(mixed<0?-1:1)*(24000+static_cast<int>((static_cast<int64_t>(d)*8191)/(d+8191)));}
  return mixed;
}
void Music::render(int16_t* out,unsigned frames){for(unsigned i=0;i<frames;i++){if(produced>=nextEvent_)start_notes();out[i]=next_sample();produced++;if(pendingBank_&&master_==0){auto* bank=pendingBank_;pendingBank_=nullptr;switch_bank(bank);}}}
}
