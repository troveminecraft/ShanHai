#pragma once
namespace glacier_weather {
struct Policy {
 bool snowy=false;
 unsigned dwell=0,cooldown=0,restoreTicks=0,refresh=0;
 int step(bool enter,bool stay){
  if(cooldown)--cooldown;
  if(snowy){
   if(!stay){snowy=false;dwell=0;cooldown=40;restoreTicks=20;refresh=0;return -1;}
   if(++refresh>=100){refresh=0;return 1;}
   return 0;
  }
  if(restoreTicks){--restoreTicks;if(restoreTicks%4==0)return -1;}
  if(!enter||cooldown){dwell=0;return 0;}
  if(++dwell<10)return 0;
  snowy=true;dwell=0;refresh=0;restoreTicks=0;return 1;
 }
};
inline constexpr float Strength=.30f;
}
