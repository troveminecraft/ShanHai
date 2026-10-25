#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace terrain_runtime {
// Keep the mandatory eight-chunk dependency halo. Delay optional flight and
// gap-repair windows while the player's central 3x3 is still being built.
inline bool allowSupplementalLoading(bool nearReady,unsigned waiting,unsigned nearPending,bool tickBusy=false){
 return nearReady&&!tickBusy&&waiting<4&&nearPending<=32;
}
struct MotionPlan {double speed=0,aheadX=0,aheadZ=0;bool teleport=false,turned=false,allowAhead=false;};
struct StreamPlanner {
 double x=0,z=0,at=0,vx=0,vz=0,coolUntil=0;bool sampled=false;
 MotionPlan update(double X,double Z,double seconds,unsigned waiting,unsigned nearPending){
  MotionPlan p;p.aheadX=X;p.aheadZ=Z;
  double dt=seconds-at,dx=X-x,dz=Z-z;
  if(sampled&&dt>.05){
   double speed=std::hypot(dx,dz)/dt;
   p.teleport=std::hypot(dx,dz)>96&&(speed>192||(dt>=2&&std::hypot(dx,dz)>256));
   if(!p.teleport&&dt<2){double nx=dx/dt,nz=dz/dt;p.turned=(nx*vx+nz*vz<0&&speed>16);vx=nx;vz=nz;p.speed=speed;}
  }
  if(p.teleport){vx=vz=0;coolUntil=seconds+1.;}
  if(waiting>=4||nearPending>32)coolUntil=std::max(coolUntil,seconds+1.);
  if(p.speed>1){double reach=std::min(128.,p.speed*.75);p.aheadX=X+vx/p.speed*reach;p.aheadZ=Z+vz/p.speed*reach;}
  p.allowAhead=p.speed>=16&&!p.teleport&&seconds>=coolUntil;
  x=X;z=Z;at=seconds;sampled=true;return p;
 }
};
// One recovery for a stalled stationary location, including arrivals by normal flight.
struct NearChunkWatchdog {
 int x=100000,z=100000;double pendingSince=-1;bool attempted=false;
 bool poll(int X,int Z,double now,bool ready,bool recovering){
  if(X!=x||Z!=z){x=X;z=Z;pendingSince=-1;attempted=false;}
  if(ready){pendingSince=-1;attempted=false;return false;}
  if(recovering){attempted=true;return false;}
  if(attempted)return false;
  if(pendingSince<0)pendingSince=now;
  if(now-pendingSince<4)return false;
  attempted=true;return true;
 }
};
// Bounded recovery; readiness is supplied by BDS, never inferred from elapsed time.
struct ArrivalRecovery {
 enum class Action {None,Rebind,Resend,Timeout};
 bool active=false,rebound=false;double started=0,readySince=-1,nextResend=0,lastProgress=0;
 uint64_t progress=0;
 void begin(double now){active=true;rebound=false;started=lastProgress=now;readySince=-1;progress=0;}
 Action poll(double now,bool ready,uint64_t observedProgress=0,bool tickBusy=false){
  if(observedProgress!=progress){progress=observedProgress;lastProgress=now;}
  if(!active)return Action::None;
  if(ready){
   if(readySince<0)readySince=now;
   if(now-readySince>=.4&&now>=nextResend){active=false;nextResend=now+2;return Action::Resend;}
  }else readySince=-1;
  if(now-started>=30){active=false;return Action::Timeout;}
  if(!ready&&!rebound&&!tickBusy&&now-lastProgress>=8){rebound=true;return Action::Rebind;}
  return Action::None;
 }
};
}
