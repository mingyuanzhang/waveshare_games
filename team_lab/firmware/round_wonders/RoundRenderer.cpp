#include "RoundRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace round_wonders {
namespace {
using echo_game::Raster;
const uint32_t colors[6][3]={{0x9de5ed,0x638fc7,0xc4b6df},{0xefae83,0xedcf8f,0xdc8fa4},{0xe5aeb9,0xd1badc,0xf3cfad},{0xb4c9a7,0xe7b5c3,0xc7accd},{0xadbbec,0xdac6e4,0xb0d9cb},{0xaad2ca,0xe6cfa2,0xbdc9ec}};
float clamp(float n,float a=0,float b=1){return std::max(a,std::min(b,n));}
float spring(float t){return t<=0?0:1-std::exp(-6.5f*t)*(std::cos(t*16)+.2f*std::sin(t*16));}
struct Brush {
  Raster& r;Vec origin,right,up;float sx=1,sy=1,alpha=1;
  Brush(Raster& raster,Vec point):r(raster),origin(point){const Vec n=radial(point-Vec(180,180));up=n*-1;right={-n.y,n.x};}
  Vec at(float x,float y)const{return origin+right*(x*sx)+up*(y*sy);}
  void line(float x,float y,float xx,float yy,uint32_t c,float a=1,float w=.8f){const Vec p=at(x,y),q=at(xx,yy);r.line(p.x,p.y,q.x,q.y,w,c,alpha*a);}
  void dot(float x,float y,float size,uint32_t c,float a=1){const Vec p=at(x,y);r.disc(p.x,p.y,std::max(.1f,size*(sx+sy)*.5f),c,alpha*a);}
  void ellipse(float x,float y,float rx,float ry,uint32_t c,float a=1){
    Vec last=at(x+rx,y);for(int i=1;i<=24;i++){const float t=i*kTau/24;const Vec p=at(x+std::cos(t)*rx,y+std::sin(t)*ry);r.line(last.x,last.y,p.x,p.y,.7f,c,alpha*a);last=p;}
  }
  void curve(float x,float y,float ax,float ay,float bx,float by,float xx,float yy,uint32_t c,float a=1){
    Vec last=at(x,y);for(int i=1;i<=6;i++){const float t=i/6.0f,u=1-t;const Vec p=at(u*u*u*x+3*u*u*t*ax+3*u*t*t*bx+t*t*t*xx,u*u*u*y+3*u*u*t*ay+3*u*t*t*by+t*t*t*yy);r.line(last.x,last.y,p.x,p.y,.7f,c,alpha*a);last=p;}
  }
  void petal(float x,float y,float angle,float size,uint32_t c,float a=1){
    const float dx=std::sin(angle),dy=std::cos(angle),wx=dy*size*.3f,wy=-dx*size*.3f;
    const float tx=x+dx*size,ty=y+dy*size;
    curve(x,y,x+wx,y+wy,x+dx*size*.7f+wx,y+dy*size*.7f+wy,tx,ty,c,a);
    curve(x,y,x-wx,y-wy,x+dx*size*.7f-wx,y+dy*size*.7f-wy,tx,ty,c,a*.75f);
    line(x,y,tx,ty,c,a*.2f,.7f);
  }
};
void shape(Brush& b,unsigned id,float radius,uint32_t c){
  if(id==0){b.ellipse(0,0,radius,radius,c,.85f);return;}
  if(id==5){b.curve(radius*.6f,-radius,-radius*1.6f,-radius,-radius*1.6f,radius,radius*.6f,radius,c);b.curve(radius*.6f,-radius,-radius*.5f,-radius*.4f,-radius*.5f,radius*.4f,radius*.6f,radius,c);return;}
  const int count=id==1?3:id==4?6:4;const float turn=id==3?kPi/4:0;Vec old=b.at(std::sin(turn)*radius,std::cos(turn)*radius);
  for(int i=1;i<=count;i++){const float angle=i*kTau/count+turn;const Vec p=b.at(std::sin(angle)*radius,std::cos(angle)*radius);b.r.line(old.x,old.y,p.x,p.y,.8f,c,b.alpha);old=p;}
}
}
Vec RoundRenderer::inward(Vec source,float distance){const Vec p=source-Vec(180,180);return Vec(180,180)+radial(p)*std::max(0.0f,length(p)-std::max(0.0f,distance));}
void RoundRenderer::background(uint16_t* pixels){
  Raster r(pixels);r.clear(0x10151d);
  for(int y=0;y<360;y++)for(int x=0;x<360;x++){
    const float d=length(Vec(x-179.5f,y-179.5f));if(d>180){pixels[y*360+x]=0;continue;}
    const float g=clamp(1-d/190);const unsigned red=17+g*8,green=24+g*10,blue=31+g*10;
    pixels[y*360+x]=Raster::rgb565((red<<16)|(green<<8)|blue);
  }
  r.circle(180,180,kGround,.8f,0xbec7b2,.36f);r.circle(180,180,kGround+3,.5f,0xbec7b2,.08f);
  for(unsigned i=0;i<70;i++){const float a=i*2.399963f,d=25+noise(i+1)*135;const Vec p=position(a,d);r.disc(p.x,p.y,.55f,0xdcd6c3,.13f);}
  for(const auto& arc:RoundWorld::arcs){
    const float span=arc.start<arc.end?arc.end-arc.start:kTau+arc.end-arc.start;Vec last=position(arc.start,arc.radius);
    const int segments=std::max(6,static_cast<int>(span*arc.radius/5));
    for(int i=1;i<=segments;i++){const float angle=arc.start+span*i/segments;const Vec p=position(angle,arc.radius);r.line(last.x,last.y,p.x,p.y,arc.spring?1.2f:.8f,arc.spring?0xd7a5ac:0xb6c1c5,.5f);last=p;}
  }
  for(const auto& ladder:RoundWorld::ladders){
    const Vec n={std::cos(ladder.angle),std::sin(ladder.angle)},t={-n.y,n.x};
    for(float side:{-2.5f,2.5f}){const Vec a=position(ladder.angle,ladder.outer)+t*side,b=position(ladder.angle,ladder.inner)+t*side;r.line(a.x,a.y,b.x,b.y,.7f,0xb3c5cb,.4f);}
    for(float radius=ladder.inner+4;radius<ladder.outer;radius+=6){const Vec a=position(ladder.angle,radius)+t*2.5f,b=position(ladder.angle,radius)-t*2.5f;r.line(a.x,a.y,b.x,b.y,.6f,0xb3c5cb,.4f);}
  }
}
void RoundRenderer::person(Raster& r,const Person& p){
  Brush b(r,position(p.angle,p.radius));const uint32_t coat=colors[p.color][0];
  const float stride=p.rest>0?.3f:std::sin(p.phase)*1.3f;
  b.line(-.6f,2,-1.4f-stride,0,0xbab8af);b.line(.6f,2,1.4f+stride,0,0xbab8af);
  b.line(0,3,0,6.5f,coat,1,2);b.line(-.6f,6,-2.6f,p.ladder>=0?8.2f:4.3f+stride*.5f,coat);
  b.line(.6f,6,2.5f,p.ladder>=0?7.5f:4.3f-stride*.5f,coat);
  b.dot(0,8.4f,1.4f,p.trait%3==0?0xc39f87:0xe5c9ac);b.line(-1.6f,9.7f,1.6f,9.7f,coat,.8f);
}
void RoundRenderer::token(Raster& r,const Token& t,float time){
  Brush b(r,t.point);b.sx=b.sy=1+std::sin(time*1.7f+t.uid)*.03f;shape(b,t.shape,3.7f,0xe4d7bd);b.dot(0,0,.65f,0xeed5ae,.65f);
}
void RoundRenderer::effect(Raster& r,const Effect& e,float age,bool detailed){
  if(age<0||age>e.life)return;
  Brush b(r,e.point);const auto* c=colors[e.shape];const float seed=e.uid*13.1f+e.cycle*19.7f;
  const float fade=clamp((e.life-age)/1.3f),grow=spring(age),wobble=std::sin(age*21)*std::exp(-age*6)*.12f;
  b.sx=grow*(1+wobble);b.sy=grow*(1-wobble*.7f);b.alpha=clamp(age/.08f)*fade;
  if(!detailed){b.ellipse(0,0,4+age*2,2+age*.5f,c[0],clamp(1-age/2));return;}
  const float sourceRadius=length(e.point-Vec(180,180));
  if(e.shape==0){
    for(int j=0;j<5;j++){const float x=(j-2)*9,y=12+std::sin(j+seed)*4;b.dot(x,y,9,c[j%3],.06f);b.curve(x-10,y,x-11,y+13,x+11,y+13,x+12,y,c[j%3],.5f);}
    for(int i=0;i<28;i++){
      const float t=age-noise(seed+i)*1.5f;if(t<0)continue;
      const Vec start=b.at((noise(seed+i*3)-.5f)*52,9),n=radial(start-Vec(180,180));
      const float radius=length(start-Vec(180,180))+t*(29+noise(i)*14);
      if(radius<kGround){const Vec p=Vec(180,180)+n*radius,q=p-n*3;r.line(p.x,p.y,q.x,q.y,.7f,c[i%3],b.alpha*.7f);}
      else {const float t2=(radius-kGround)/25;if(t2<1){Brush ripple(r,Vec(180,180)+n*kGround);ripple.alpha=b.alpha*(1-t2);ripple.ellipse(0,0,2+t2*7,.8f+t2*.7f,c[i%3],.5f);}}
    }
  }else if(e.shape==1){
    for(int burst=0;burst<2;burst++){
      const float t=age-burst*.65f;if(t<0)continue;
      const Vec origin=inward(e.point,std::max(0.0f,t)*std::max(15.0f,sourceRadius/4.5f)+18);Brush flower(r,origin);
      flower.alpha=b.alpha*clamp((4.8f-t)/1.7f)*clamp(length(origin-Vec(180,180))/18);
      const float opening=spring(std::max(0.0f,t-.22f));
      for(int i=0;i<24;i++){const float a=i*kTau/24+seed,reach=(17+noise(i+seed)*12)*opening;
        flower.curve(0,0,std::sin(a-.3f)*reach*.4f,std::cos(a-.3f)*reach*.4f,std::sin(a+.1f)*reach*.8f,std::cos(a+.1f)*reach*.8f,std::sin(a)*reach,std::cos(a)*reach,c[i%3],.8f);
        flower.dot(std::sin(a)*reach,std::cos(a)*reach,.7f,c[i%3],.9f);
      }
    }
  }else if(e.shape==2){
    for(int i=0;i<4;i++){
      const float t=age-i*.25f;if(t<0)continue;const Vec center=inward(e.point,t*23);
      Brush balloon(r,center);balloon.origin=balloon.origin+balloon.right*((i-1.5f)*9+std::sin(age+i)*3);
      balloon.alpha=b.alpha*clamp((sourceRadius-t*23)/18);balloon.sx=spring(t);balloon.sy=balloon.sx;
      balloon.dot(0,0,6,c[i%3],.09f);balloon.ellipse(0,0,6,8,c[i%3],.8f);
      balloon.curve(0,-8,-4,-6,-4,5,0,8,c[(i+1)%3],.6f);balloon.curve(0,-8,4,-6,4,5,0,8,c[(i+1)%3],.6f);
      balloon.curve(0,-8,std::sin(age+i)*6,-13,-4,-20,1,-27,c[i%3],.6f);
    }
  }else if(e.shape==3){
    for(int i=0;i<5;i++){
      const float t=age-i*.035f;if(t<0)continue;const float a=(i/4.0f-.5f)*1.9f;
      const float reach=std::min(std::max(12.0f,sourceRadius-12),29+noise(seed+i)*23)*clamp(t/.16f);
      const float x=std::sin(a)*reach,y=std::cos(a)*reach;
      b.curve(0,0,0,reach*.35f,x*.7f,reach*.6f,x,y,c[i%3],.65f);
      b.petal(x*.32f,y*.48f,a+(i%2?1:-1)*1.3f,8,c[(i+1)%3],.45f);
      const float bloom=spring(std::max(0.0f,t-.05f));
      for(int p=0;p<7;p++)b.petal(x,y,p*kTau/7+std::sin(age*.5f+i)*.08f,(7+noise(seed+p)*3)*bloom,c[(i+1)%3],.7f);
      b.dot(x,y,1.2f,0xeed5a7,.85f);
    }
  }else if(e.shape==4){
    const float head=std::min(sourceRadius,age*std::max(24.0f,sourceRadius/5)+20);
    b.alpha*=clamp((sourceRadius-head)/18);
    for(int j=0;j<9;j++){
      float xx=0,yy=0;
      for(int i=1;i<=24;i++){const float u=i/24.0f,y=u*head,x=std::sin(u*8-age+j*.17f)*std::sin(u*kPi)*(8+j*.6f)+(j-4)*u*1.1f;
        b.line(xx,yy,x,y,c[j%3],j%3?.3f:.7f);xx=x;yy=y;
      }
    }
  }else{
    const Vec center=inward(e.point,age*std::max(13.0f,sourceRadius/5));Brush orbit(r,center);orbit.alpha=b.alpha*clamp(length(center-Vec(180,180))/18);
    for(int i=0;i<4;i++){
      const float radius=(12+i*5)*grow;orbit.ellipse(0,i*2,radius,radius*(.3f+i*.08f),c[i%3],.5f);
      const float a=age*(.5f+i*.08f)+i*2.4f;orbit.dot(std::cos(a)*radius,i*2+std::sin(a)*radius*(.3f+i*.08f),1.5f,c[i%3]);
    }
    orbit.dot(0,0,2,0xeddab1,.7f);
  }
  if(age<.7f)b.ellipse(0,0,4+age*20,2+age*5,c[1],(1-age/.7f)*.6f);
}
void RoundRenderer::draw(const RoundWorld& world,uint16_t* pixels,const uint16_t* backgroundPixels,uint64_t samples,const char* toast,float toastAlpha,unsigned detail){
  std::memcpy(pixels,backgroundPixels,360*360*2);Raster r(pixels);
  unsigned order[kEffects],count=0;
  for(unsigned i=0;i<kEffects;i++){const auto& e=world.effects[i];if(e.active&&samples>=e.starts&&(samples-e.starts)/24000.0f<e.life)order[count++]=i;}
  std::sort(order,order+count,[&](unsigned a,unsigned b){return world.effects[a].starts<world.effects[b].starts;});
  // Latest discoveries retain detail even when an old slot has been reused.
  for(unsigned i=0;i<count;i++){const auto& e=world.effects[order[i]];effect(r,e,(samples-e.starts)/24000.0f,count-i<=detail);}
  for(const auto& p:world.people)person(r,p);for(const auto& t:world.tokens)if(t.active)token(r,t,world.time);
  if(toast&&toastAlpha>0){const int width=std::strlen(toast)*6+16;r.rect(180-width/2,170,width,20,0x131d25,toastAlpha*.85f);r.text(toast,180,180,1,0xe6d4b6);}
}
}
