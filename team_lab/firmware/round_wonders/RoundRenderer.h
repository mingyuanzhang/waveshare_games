#pragma once
#include "RoundWorld.h"
#include "Raster.h"
namespace round_wonders {
class RoundRenderer {
 public:
  void background(uint16_t* pixels);
  void draw(const RoundWorld& world,uint16_t* pixels,const uint16_t* background,uint64_t samples,
            const char* toast=nullptr,float toastAlpha=0,unsigned detail=6);
  static Vec inward(Vec source,float distance);
 private:
  void person(echo_game::Raster& r,const Person& person);
  void token(echo_game::Raster& r,const Token& token,float time);
  void effect(echo_game::Raster& r,const Effect& effect,float age,bool detailed);
};
}
