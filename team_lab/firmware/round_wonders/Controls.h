#pragma once
#include <stdint.h>
namespace round_wonders {
enum class ButtonAction {None, Shape, Theme, Mute};
class Controls {
 public:
  void initialize(bool down,uint64_t now){raw_=stable_=down;ready_=!down;changed_=pressed_=now;waiting_=held_=second_=false;}
  ButtonAction update(bool down,uint64_t now){
    if(down!=raw_){raw_=down;changed_=now;}
    if(raw_!=stable_&&now-changed_>=25){
      stable_=raw_;
      if(stable_){if(ready_){pressed_=changed_;held_=false;second_=waiting_&&changed_-deadline_<300;}} // Use the observed press edge, before debounce.
      else if(!ready_){ready_=true;}
      else if(held_){held_=false;waiting_=false;second_=false;}
      else if(changed_-pressed_>=1000){waiting_=false;second_=false;return ButtonAction::Mute;}
      else if(second_){waiting_=false;second_=false;return ButtonAction::Theme;}
      else {waiting_=true;deadline_=now;}
    }
    if(raw_&&stable_&&ready_&&!held_&&now-pressed_>=1000){held_=true;waiting_=false;second_=false;return ButtonAction::Mute;}
    if(waiting_&&!stable_&&now-deadline_>=300){waiting_=false;return ButtonAction::Shape;}
    return ButtonAction::None;
  }
  bool flush_single(){if(!waiting_||raw_||stable_)return false;waiting_=false;second_=false;return true;}
 private:
  bool raw_=false,stable_=false,ready_=true,waiting_=false,held_=false,second_=false;
  uint64_t changed_=0,pressed_=0,deadline_=0;
};
}
