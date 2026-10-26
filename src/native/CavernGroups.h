#pragma once
#include "Noise.h"
namespace nativegen {
// Finite groups of up to four connected cave systems. Both room and passage
// geometry are swept through varying curved sections; no per-voxel 3-D noise.
class CavernGroups {
public:
 using Column=std::array<uint64_t,9>;
private:
 struct P {double x,y,z;};
 struct K {P p;double left,right,roof,depth;};
 struct Span {K a,b;double pad,dx,dz,len,len2;bool room;bool cliff=false,thermal=false;};
 struct Pillar {double x,z,r,phase;};
 struct Node {P p;double terrain=0,length=0,width=0,tall=0,angle=0;int pillars=0;bool room=false,active=false,mega=false;};
 struct Bend {int shape;std::array<P,9> route;};
 struct Chasm {P p;double length,width,tall,angle;bool lava,waterfall;};
 struct Group {std::vector<Span> spans;std::vector<Pillar> pillars;std::array<Node,4> nodes;J links=J::array(),attachments=J::array(),lavaBands=J::array();std::array<std::vector<int>,44*44> bins;std::vector<Chasm> chasms;std::vector<Bend> bends;};
 struct Mask {
  std::array<Column,256> columns{};
  std::array<bool,256> thermal{};
  auto& operator[](size_t i){return columns[i];}
 };
 static constexpr int Cell=352,Pitch=704;
 uint32_t seed;Perlin rock;bool megaEnabled=false,designEnabled=false,lavaBandEnabled=false,moreChasms=false;
 int upperYExclusive=320;bool uplandNetworks=false;
 Cache<uint64_t,std::shared_ptr<Group>> groups{32};
 Cache<uint64_t,std::shared_ptr<Mask>> masks{64};
 std::shared_ptr<Mask> active;int activeX=INT32_MIN,activeZ=INT32_MIN;
 Column const* activeColumn=nullptr;int columnX=INT32_MIN,columnZ=INT32_MIN;
 double rnd(int x,int z,int salt)const{return hash01(x,z,seedHash(seed,7200+salt));}
 static K blend(K a,K b,double t){return {{mix(t,a.p.x,b.p.x),mix(t,a.p.y,b.p.y),mix(t,a.p.z,b.p.z)},mix(t,a.left,b.left),mix(t,a.right,b.right),mix(t,a.roof,b.roof),mix(t,a.depth,b.depth)};}
 void sweep(Group&g,K a,K b,double bend,bool room=false,bool cliff=false,bool thermal=false){
  double dx=b.p.x-a.p.x,dz=b.p.z-a.p.z,len=std::hypot(dx,dz);if(len<.01)return;
  int steps=std::max(2,int(std::ceil(len/5.)));K prev=a;
  for(int i=1;i<=steps;++i){double t=double(i)/steps;K next=blend(a,b,t);
   double w=std::sin(t*3.141592653589793)*bend;next.p.x-=dz/len*w;next.p.z+=dx/len*w;
   double X=next.p.x-prev.p.x,Z=next.p.z-prev.p.z,L=std::hypot(X,Z);
   double pad=(cliff?1.60:1.20)*std::max({prev.left,prev.right,next.left,next.right})+1;
   g.spans.push_back({prev,next,pad,X,Z,L,L*L,room,cliff,thermal});prev=next;
  }
 }
 K passage(P p,double width=3.1){return {p,width,width,4.6,1.6};}
 // Interpolate the whole route with shared tangents. Independent sine bends
 // between radial control points retained the appearance of straight spokes.
 void curve(Group&g,std::vector<K> const&way){
  auto tangent=[](P a,P b){return P{(b.x-a.x)*.5,(b.y-a.y)*.5,(b.z-a.z)*.5};};
  for(size_t j=1;j<way.size();++j){auto a=way[j-1],b=way[j];
   auto m=tangent(way[j>1?j-2:j-1].p,b.p),n=tangent(a.p,way[std::min(j+1,way.size()-1)].p);
   int steps=std::max(2,int(std::ceil(std::hypot(b.p.x-a.p.x,b.p.z-a.p.z)/4.)));K prev=a;
   for(int i=1;i<=steps;++i){double t=double(i)/steps,t2=t*t,t3=t2*t;K next=blend(a,b,t);
    auto axis=[&](double A,double B,double M,double N){return (2*t3-3*t2+1)*A+(t3-2*t2+t)*M+(-2*t3+3*t2)*B+(t3-t2)*N;};
    next.p={axis(a.p.x,b.p.x,m.x,n.x),std::max(-49.,axis(a.p.y,b.p.y,m.y,n.y)),axis(a.p.z,b.p.z,m.z,n.z)};
    double dx=next.p.x-prev.p.x,dz=next.p.z-prev.p.z,len=std::hypot(dx,dz);
    if(len>.001){double pad=1.20*std::max({prev.left,prev.right,next.left,next.right})+1;g.spans.push_back({prev,next,pad,dx,dz,len,len*len,false});}prev=next;
   }
  }
 }
 K branchSection(P p,double width,bool narrow,int x,int z,int salt){
  if(designEnabled){
   double swell=.78+.75*rnd(x,z,salt+4),left=.70+.60*rnd(x,z,salt),right=.70+.60*rnd(x,z,salt+1);
   double ceiling=(narrow?2.6:3.7)+(narrow?1.8:4.8)*rnd(x,z,salt+2);
   return {p,width*swell*left,width*swell*right,ceiling,narrow?1.4:2.+2*rnd(x,z,salt+3)};
  }
  return {p,width*(.88+.20*rnd(x,z,salt)),width*(.88+.20*rnd(x,z,salt+1)),
   narrow?2.5+.7*rnd(x,z,salt+2):3.6+1.2*rnd(x,z,salt+2),narrow?1.0:1.4};
 }
 template<class Height> KARMA_NOINLINE std::shared_ptr<Group> group(int gx,int gz,Height&height){return groups.get(key2(gx,gz),[&]{
  auto g=std::make_shared<Group>();g->spans.reserve(700);
  for(int n=0;n<4;++n){int cx=gx*2+n%2,cz=gz*2+n/2;auto&r=g->nodes[n];
   r.p={(cx+.5)*Cell+(rnd(cx,cz,1)-.5)*32,0,(cz+.5)*Cell+(rnd(cx,cz,2)-.5)*32};
   r.terrain=height(int(r.p.x),int(r.p.z));r.angle=rnd(cx,cz,3)*6.283185307179586;
   r.active=r.terrain>-10;if(!r.active){r.p.y=-47;continue;}
   r.room=rnd(cx,cz,4)<.40&&r.terrain>40;
   r.length=100+65*rnd(cx,cz,5);r.width=24+30*rnd(cx,cz,6);r.tall=30+31*rnd(cx,cz,7);
   // Rare enlarged land chambers stay inside their existing finite group.
   // Ordinary rooms and the connecting network keep their original seeds.
   r.mega=megaEnabled&&r.room&&r.terrain>85&&rnd(cx,cz,501)<.16;
   if(r.mega){r.length=220+20*rnd(cx,cz,502);r.width=58+7*rnd(cx,cz,503);r.tall=68+25*rnd(cx,cz,504);}
   // Raise some rooms above zero where the surrounding rock can support them.
   double top=r.terrain;
   if(r.room)for(int i=0;i<8;++i){double a=r.angle+i*.7853981633974483;top=std::min(top,height(int(r.p.x+std::cos(a)*90),int(r.p.z+std::sin(a)*90)));}
   if(r.mega)for(int i=0;i<16;++i){double a=r.angle+i*.39269908169872414;top=std::min(top,height(int(r.p.x+std::cos(a)*140),int(r.p.z+std::sin(a)*140)));}
   double maxY=std::max(-42.,top-r.tall-26),band=rnd(cx,cz,8);
   r.p.y=band<.30?-47.:band<.67?mix(rnd(cx,cz,9),-20.,std::max(-15.,maxY*.45)):maxY;
   if(!r.room)r.p.y=std::max(-38.,std::min(r.p.y,r.terrain-32));
   // Preserve the old deep network, then add separate galleries inside raised
   // land. Raising the ceiling alone left most of the new mountain volume solid.
   if(uplandNetworks)for(int oz:{-80,80})for(int ox:{-80,80}){
    double localTerrain=height(int(r.p.x+ox),int(r.p.z+oz));if(localTerrain<=170)continue;
    for(int layer=0;layer<2;++layer){
     double target=100+(localTerrain-100)*(layer?.72:.40);
     P hub{r.p.x+ox,target,r.p.z+oz};
     // A local chamber and three irregular arms remain inside the owner cell.
     K left{{hub.x-22,hub.y,hub.z},11,14,13,4},right{{hub.x+22,hub.y+3,hub.z},14,10,11,4};
     sweep(*g,left,right,7);
     for(int arm=0;arm<3;++arm){
      double angle=r.angle+arm*2.0943951023931953+layer*.65;
      std::vector<K> way{branchSection(hub,4.5,false,cx,cz,6300+layer*100+arm*20)};
      for(int j=1;j<=3;++j){
       double distance=j*15.,turn=angle+.17*std::sin(j*1.7+r.angle);
       double X=hub.x+std::cos(turn)*distance,Z=hub.z+std::sin(turn)*distance;
       double roof=height(int(X),int(Z))-26;
       if(roof<112)break;
       P point{X,std::min(roof,target+8*std::sin(j*.9+arm)),Z};
       way.push_back(branchSection(point,4.2+.8*rnd(cx,cz,6600+arm*7+j),false,cx,cz,6500+layer*100+arm*20+j*4));
      }
      if(way.size()>1)curve(*g,way);
     }
     // Staggered descent joins the two upper levels and the existing network.
     P low{layer?hub.x:r.p.x,layer?100+(localTerrain-100)*.40:r.p.y,layer?hub.z:r.p.z};
     curve(*g,{passage(hub,3.2),passage({mix(.5,hub.x,low.x)+12,(hub.y+low.y)*.5,mix(.5,hub.z,low.z)-12},3.2),passage(low,3.2)});
    }
   }
   r.pillars=r.room?(rnd(cx,cz,10)<.45?0:1+int(rnd(cx,cz,11)*3)):0;
   if(r.mega){
    r.pillars=0;
    // Independent lateral lobes, staggered floor levels and vaulted roofs.
    // Short analytic sweeps reuse the chunk bins and column bit masks.
    std::vector<K> knots;
    for(int j=0;j<7;++j){double t=j/6.,d=(t-.5)*r.length;
     double side=(j==0||j==6||j==3)?0:(rnd(cx,cz,520+j)-.5)*18;
     P p{r.p.x+std::cos(r.angle)*d-std::sin(r.angle)*side,r.p.y+(j==3?0:(rnd(cx,cz,530+j)-.5)*15),r.p.z+std::sin(r.angle)*d+std::cos(r.angle)*side};
     double taper=(j==0||j==6)?.025:(j==1||j==5)?.48:1.;
     knots.push_back({p,r.width*taper*(.70+.30*rnd(cx,cz,540+j)),r.width*taper*(.65+.35*rnd(cx,cz,550+j)),r.tall*taper*(.65+.35*rnd(cx,cz,560+j)),(8+8*rnd(cx,cz,570+j))*taper});
    }
    for(int j=1;j<7;++j)sweep(*g,knots[j-1],knots[j],(rnd(cx,cz,580+j)-.5)*6,true);
    for(int j=0;j<3;++j){double along=(j-1)*34.,side=(j%2?-1:1)*(62+12*rnd(cx,cz,590+j));
     P p{r.p.x+std::cos(r.angle)*along-std::sin(r.angle)*side,r.p.y+4+10*rnd(cx,cz,600+j),r.p.z+std::sin(r.angle)*along+std::cos(r.angle)*side};
     K a=knots[j+2],b{p,16+8*rnd(cx,cz,610+j),13+9*rnd(cx,cz,620+j),25+20*rnd(cx,cz,630+j),5};
     sweep(*g,a,b,(rnd(cx,cz,640+j)-.5)*8,true);
    }
   }else if(r.room){
    std::vector<K> knots;
    for(int j=0;j<5;++j){double t=double(j)/4,d=(t-.5)*r.length;
     double side=j==2?0:(rnd(cx,cz,20+j)-.5)*46;
     P p{r.p.x+std::cos(r.angle)*d-std::sin(r.angle)*side,r.p.y+(j==2?0:(rnd(cx,cz,30+j)-.5)*12),r.p.z+std::sin(r.angle)*d+std::cos(r.angle)*side};
     double taper=(j==0||j==4)?(.18+.25*rnd(cx,cz,740+j)):(.55+.45*rnd(cx,cz,750+j));double width=r.width*taper;
     knots.push_back({p,width*(.7+.35*rnd(cx,cz,40+j)),width*(.7+.35*rnd(cx,cz,50+j)),r.tall*taper*(.68+.32*rnd(cx,cz,60+j)),(5+4*rnd(cx,cz,70+j))*taper});
    }
    for(int j=1;j<5;++j)sweep(*g,knots[j-1],knots[j],(rnd(cx,cz,80+j)-.5)*8,true);
    for(int p=0;p<r.pillars;++p){double d=(rnd(cx,cz,90+p)-.5)*r.length*.65;
     double side=(p%2?-1:1)*r.width*(.35+.18*rnd(cx,cz,100+p));
     g->pillars.push_back({r.p.x+std::cos(r.angle)*d-std::sin(r.angle)*side,r.p.z+std::sin(r.angle)*d+std::cos(r.angle)*side,2+3*rnd(cx,cz,110+p),rnd(cx,cz,120+p)*6.28});
    }
   }
   // Heading evolves along each path instead of aiming every control point
   // away from the node. Two narrow tributaries coexist with larger passages.
   for(int a=0;a<(designEnabled?3:4);++a){double angle=r.angle+a*1.5707963267948966+(rnd(cx,cz,810+a)-.5)*.6;
    bool narrow=a%2==1;double width=narrow?(designEnabled?2.:1.55)+.4*rnd(cx,cz,820+a):(designEnabled?3.3:2.7)+.5*rnd(cx,cz,820+a);
    std::vector<K> way{branchSection(r.p,width,narrow,cx,cz,830+a*40)};
    double turn=(rnd(cx,cz,840+a)-.5)*.9,grade=(rnd(cx,cz,850+a)-.5)*.55;
    for(int j=1;j<=(designEnabled?4:5);++j){angle+=turn;turn=turn*.55+(rnd(cx,cz,860+a*20+j)-.5)*1.1;
     double len=(designEnabled?16.:21.)+4*rnd(cx,cz,980+a*20+j);auto prev=way.back().p;
     grade=grade*.5+(rnd(cx,cz,1100+a*20+j)-.5)*.70;
     P next{prev.x+std::cos(angle)*len,prev.y+grade*len,prev.z+std::sin(angle)*len};
     next.y=std::clamp(next.y,-49.,std::max(-48.,height(int(next.x),int(next.z))-25));
     way.push_back(branchSection(next,width*(.90+.18*rnd(cx,cz,1220+a*20+j)),narrow,cx,cz,1340+a*30+j*4));
    }
    curve(*g,way);
   }
   if(designEnabled){
    // Side-view bends: the centreline turns through vertical space as well
    // as horizontal space. Keep finite, locally indexed sweeps and small tubes.
    int bendShape=int(rnd(cx,cz,1999)*3);
    for(int a=bendShape;a<bendShape+1;++a){double heading=r.angle+a*2.0943951,side=a==0?-1.:1.;
     double upper=std::min(r.terrain-28.,r.p.y+35+30*rnd(cx,cz,2000+a));
     double lower=std::max(-52.,upper-40-30*rnd(cx,cz,2010+a));
     std::vector<K> way;
     for(int j=0;j<=8;++j){double t=j/8.,bend=std::sin(t*3.141592653589793);
      double along=a==2?bend*50-16:(t-.5)*72,across=side*bend*(18+12*rnd(cx,cz,2020+a));
      double y=a==2?mix(t,lower,upper)+std::sin(t*6.283185307)*8:mix(t,upper,lower);
      P p{r.p.x+std::cos(heading)*along-std::sin(heading)*across,y,r.p.z+std::sin(heading)*along+std::cos(heading)*across};
      way.push_back(branchSection(p,1.5+.7*rnd(cx,cz,2030+a),true,cx,cz,2040+a*50+j*4));
     }
     Bend description;description.shape=a+1;for(int j=0;j<=8;++j)description.route[j]=way[j].p;g->bends.push_back(description);
     curve(*g,way);curve(*g,{passage(r.p,2),way[4]});
    }
    if(r.terrain>52&&rnd(cx,cz,2400)<(moreChasms?.36:.28)){
     bool lava=rnd(cx,cz,2401)<.48;double floor=lava?-56.:std::max(-36.,r.terrain-104-12*rnd(cx,cz,2402));
     bool large=rnd(cx,cz,2407)<.35;double tall=std::min((large?60.:30.)+(large?20.:30.)*rnd(cx,cz,2403),r.terrain-26-floor);
     if(tall>=30){double length=70+35*rnd(cx,cz,2404),width=2.5+2*rnd(cx,cz,2405);std::vector<K> way;
      for(int j=0;j<=6;++j){double t=j/6.,along=(t-.5)*length,side=std::sin(t*3.14159265)*8;
       P p{r.p.x+std::cos(r.angle)*along-std::sin(r.angle)*side,floor,r.p.z+std::sin(r.angle)*along+std::cos(r.angle)*side};
       double taper=(j==0||j==6)?.12:1.;way.push_back({p,width*taper,width*taper,tall,0});
      }
      for(int j=1;j<=6;++j)sweep(*g,way[j-1],way[j],0,true,true);
      g->chasms.push_back({way[3].p,length,width,tall,r.angle,lava,!lava&&rnd(cx,cz,2406)<.35});
      P ledge=way[3].p;ledge.y+=std::min(tall-8.,std::max(8.,r.p.y-floor));curve(*g,{passage(r.p,2.3),passage(ledge,2.3)});
     }
    }
   }
   // A basal gallery belongs to the cave network, not a surface pool stamped
   // after carving. Broad submerged floors share the deep lava datum; the
   // raised sides and the descending access remain dry rock ledges.
   if(lavaBandEnabled&&r.terrain>24&&rnd(cx,cz,5100)<.80){
    double length=112+40*rnd(cx,cz,5101),width=12+9*rnd(cx,cz,5102);
    std::vector<K> basin;J route=J::array();
    for(int j=0;j<=6;++j){double t=j/6.,along=(t-.5)*length;
     double side=std::sin(t*6.283185307)*(9+7*rnd(cx,cz,5103));
     double taper=(j==0||j==6)?.24:(.75+.25*rnd(cx,cz,5120+j));
     P p{r.p.x+std::cos(r.angle)*along-std::sin(r.angle)*side,-48.5+(rnd(cx,cz,5130+j)-.5)*1.6,r.p.z+std::sin(r.angle)*along+std::cos(r.angle)*side};
     basin.push_back({p,width*taper*(.75+.35*rnd(cx,cz,5140+j)),width*taper*(.75+.35*rnd(cx,cz,5150+j)),12+12*rnd(cx,cz,5160+j),7.5});
     route.push_back({p.x,p.y,p.z});
    }
    for(int j=1;j<=6;++j)sweep(*g,basin[j-1],basin[j],0,true,false,true);
    // Descend around the gallery rather than ending a narrow pipe in lava.
    P bank=basin[2].p;bank.x-=std::sin(r.angle)*basin[2].left*.80;bank.z+=std::cos(r.angle)*basin[2].left*.80;bank.y=-47;
    P mid{r.p.x+std::cos(r.angle+.8)*48,(r.p.y+bank.y)*.5,r.p.z+std::sin(r.angle+.8)*48};
    curve(*g,{passage(r.p,3.),passage(mid,3.2),passage(bank,3.)});
    g->lavaBands.push_back({{"route",route},{"width",width*2},{"access",{{r.p.x,r.p.y,r.p.z},{mid.x,mid.y,mid.z},{bank.x,bank.y,bank.z}}}});
   }
   // A deep spur guarantees access to the lava horizon in some taller systems.
   if((r.room||designEnabled)&&r.p.y>-40&&rnd(cx,cz,350)<.65){P end{r.p.x+std::cos(r.angle+1.1)*105,-49,r.p.z+std::sin(r.angle+1.1)*105};
    P mid{mix(.5,r.p.x,end.x)-std::sin(r.angle)*20,mix(.5,r.p.y,end.y),mix(.5,r.p.z,end.z)+std::cos(r.angle)*20};
    sweep(*g,passage(r.p),passage(mid),3);auto b=passage(end,5);b.depth=4;sweep(*g,passage(mid),b,-3);
   }
  }
  if(designEnabled){
   for(int iz=0;iz<4;++iz)for(int ix=0;ix<4;++ix){
    int sx=gx*4+ix,sz=gz*4+iz;if(rnd(sx,sz,3000)>.78)continue;
    double X=gx*Pitch+88+ix*176+(rnd(sx,sz,3001)-.5)*20;
    double Z=gz*Pitch+88+iz*176+(rnd(sx,sz,3002)-.5)*20;
    double ground=height(int(X),int(Z));if(ground<24)continue;
    double Y=mix(rnd(sx,sz,3003),-42.,std::max(-35.,std::min(40.,ground-32)));
    double heading=rnd(sx,sz,3004)*6.283185307,extent=32+14*rnd(sx,sz,3005);
    double grade=(rnd(sx,sz,3006)-.5)*24,width=2.2+.8*rnd(sx,sz,3007);
    std::vector<K> way;
    for(int j=0;j<=6;++j){double t=j/6.,along=(t-.5)*extent*2;
     double side=std::sin(t*6.283185307)*(7+6*rnd(sx,sz,3008));
     P q{X+std::cos(heading)*along-std::sin(heading)*side,Y+(t-.5)*grade+std::sin(t*3.14159265)*7,Z+std::sin(heading)*along+std::cos(heading)*side};
     q.y=std::clamp(q.y,-49.,std::max(-48.,height(int(q.x),int(q.z))-24));
     way.push_back(branchSection(q,width,false,sx,sz,3020+j*4));
    }
    // Every peripheral passage attaches to existing carved geometry. Its
    // nearest endpoint joins the nearest span in 3D; no isolated short tubes.
    double best=1e30;K source{},target{};bool found=false;
    for(auto end:{way.front(),way.back()})for(auto const&s:g->spans){
     double dy=s.b.p.y-s.a.p.y,den=s.len2+dy*dy;
     double t=clamp(((end.p.x-s.a.p.x)*s.dx+(end.p.z-s.a.p.z)*s.dz+(end.p.y-s.a.p.y)*dy)/den,0.,1.);
     K q=blend(s.a,s.b,t);double X=end.p.x-q.p.x,Y=end.p.y-q.p.y,Z=end.p.z-q.p.z,d=X*X+Y*Y+Z*Z;
     if(d<best){best=d;source=end;target=q;found=true;}
    }
    if(!found)continue;
    // A room span's centre is its floor datum. Join safely above that floor.
    target.p.y+=std::min(3.,target.roof*.25);target=branchSection(target.p,2.8,false,sx,sz,3090);
    P mid{(source.p.x+target.p.x)*.5,(source.p.y+target.p.y)*.5,(source.p.z+target.p.z)*.5};
    double dx=target.p.x-source.p.x,dz=target.p.z-source.p.z,L=std::max(1.,std::hypot(dx,dz));
    double bow=(rnd(sx,sz,3099)-.5)*14;mid.x-=dz/L*bow;mid.z+=dx/L*bow;
    curve(*g,way);curve(*g,{source,branchSection(mid,3.,false,sx,sz,3094),target});
    g->attachments.push_back({{"route",{{source.p.x,source.p.y,source.p.z},{mid.x,mid.y,mid.z},{target.p.x,target.p.y,target.p.z}}}});
   }
  }
  // Connect feasible land nodes first. Reject deep-sea crossings instead of
  // advertising a link which the protected seabed will later seal shut.
  static constexpr int edges[6][2]={{0,1},{1,3},{3,2},{2,0},{0,3},{1,2}};
  std::array<int,4> parent{0,1,2,3};auto root=[&](int n){while(parent[n]!=n)n=parent[n];return n;};
  int rotate=int(rnd(gx,gz,400)*4);
  for(int order=0;order<6;++order){int e=order<4?(order+rotate)%4:order,a=edges[e][0],b=edges[e][1];
   if(!g->nodes[a].active||!g->nodes[b].active||root(a)==root(b))continue;
   auto A=g->nodes[a].p,B=g->nodes[b].p;
   double dx=B.x-A.x,dz=B.z-A.z,len=std::hypot(dx,dz);J route=J::array({J::array({A.x,A.y,A.z})});
   std::vector<P> way{A};bool feasible=true;
   double lateral=(rnd(gx,gz,410+e*16)-.5)*64,phase=rnd(gx,gz,411+e*16)*6.283185307179586;
   for(int j=1;j<=12;++j){double t=j/12.;double bend=std::sin(t*3.141592653589793)*(lateral+18*std::sin(t*6.283185307179586+phase));
    P next{mix(t,A.x,B.x)-dz/len*bend,mix(t,A.y,B.y)+std::sin(t*6.283185307179586)*(12+12*rnd(gx,gz,790+e)),mix(t,A.z,B.z)+dx/len*bend};
    double h=height(int(next.x),int(next.z));if(h<-12){feasible=false;break;}
    next.y=std::clamp(next.y,-49.,std::max(-49.,h-26));if(j==12)next=B;way.push_back(next);
   }
   if(!feasible)continue;
   std::vector<K> sections;for(size_t j=0;j<way.size();++j){auto next=way[j];sections.push_back(designEnabled?branchSection(next,3.2,false,gx,gz,4000+e*100+int(j)*6):passage(next,2.7+.5*std::sin(j*.7+phase)));if(j)route.push_back({next.x,next.y,next.z});}curve(*g,sections);
   if(designEnabled)for(int j:{3,8}){
    // Unequal, staggered side chambers interrupt the tubular silhouette.
    auto a=sections[j],b=sections[j+1];a.left*=1.8;a.right*=1.15;a.roof*=1.15;b.left*=1.1;b.right*=1.8;b.depth+=2;
    sweep(*g,a,b,(rnd(gx,gz,4300+e*9+j)-.5)*9,true);
   }
   parent[root(a)]=root(b);
   g->links.push_back({{"from",a},{"to",b},{"route",route}});
  }
  // A bounded spanning tree connects the systems; attached side passages
  // distribute exploration without extending the graph across the whole world.
  // A 704-block group leaves at least 18 blocks of solid margin on each side.
  // These assertions reject parameter drift; no geometry is clipped to tiles.
  for(auto const&s:g->spans)if(std::min(s.a.p.x,s.b.p.x)-s.pad<gx*Pitch+18||std::max(s.a.p.x,s.b.p.x)+s.pad>(gx+1)*Pitch-18||std::min(s.a.p.z,s.b.p.z)-s.pad<gz*Pitch+18||std::max(s.a.p.z,s.b.p.z)+s.pad>(gz+1)*Pitch-18)throw std::runtime_error("Cave group escaped finite support");
  // Build bounded chunk indices once per group. A new chunk need not test
  // hundreds of distant room and passage spans to discover it is empty.
  for(size_t i=0;i<g->spans.size();++i){auto const&s=g->spans[i];
   int l=int(std::floor((std::min(s.a.p.x,s.b.p.x)-s.pad)/16))-gx*44,r=int(std::floor((std::max(s.a.p.x,s.b.p.x)+s.pad)/16))-gx*44;
   int n=int(std::floor((std::min(s.a.p.z,s.b.p.z)-s.pad)/16))-gz*44,b=int(std::floor((std::max(s.a.p.z,s.b.p.z)+s.pad)/16))-gz*44;
   for(int z=n;z<=b;++z)for(int x=l;x<=r;++x)g->bins[z*44+x].push_back(int(i));
  }
  return g;
 });}
 void setRange(Column&c,int lo,int hi)const{lo=std::max(-56,lo)+64;hi=std::min(upperYExclusive-1,hi)+64;if(lo>hi)return;
  for(int w=lo/64;w<=hi/64;++w){int a=w==lo/64?lo%64:0,b=w==hi/64?hi%64:63;c[w]|=(~uint64_t(0)<<a)&(~uint64_t(0)>>(63-b));}}
 template<class Height> KARMA_NOINLINE std::shared_ptr<Mask> raster(int cx,int cz,Height&height){
  auto out=std::make_shared<Mask>();int x0=cx*16,z0=cz*16,gx=int(std::floor(x0/double(Pitch))),gz=int(std::floor(z0/double(Pitch)));auto g=group(gx,gz,height);
  std::vector<Span const*> nearby;for(int id:g->bins[(cz-gz*44)*44+cx-gx*44]){auto const&s=g->spans[id];if(std::max(s.a.p.x,s.b.p.x)+s.pad>=x0&&std::min(s.a.p.x,s.b.p.x)-s.pad<=x0+15&&std::max(s.a.p.z,s.b.p.z)+s.pad>=z0&&std::min(s.a.p.z,s.b.p.z)-s.pad<=z0+15)nearby.push_back(&s);}
  if(nearby.empty())return out;
  std::array<double,256> wall{},floor{},roof{};std::array<int,256> lo{},hi{};lo.fill(upperYExclusive);hi.fill(-64);
  for(int z=0;z<16;++z)for(int x=0;x<16;++x){int i=z*16+x;double X=x+x0,Z=z+z0;
   wall[i]=.15*rock.at(X/21.,Z/21.)+.05*rock.at(X/6.+13,Z/6.-7);
   floor[i]=rock.at(X/23.-31,Z/23.+23);roof[i]=rock.at(X/32.+11,Z/32.-17);
  }
  for(auto ptr:nearby){auto const&s=*ptr;
   std::array<double,576> cliffSin{},cliffWidth{};
   bool flatCliff=s.cliff&&s.a.p.y==s.b.p.y&&s.a.roof==s.b.roof;
   if(flatCliff)for(int y=std::max(-56,int(s.a.p.y)-3);y<=std::min(upperYExclusive-1,int(s.a.p.y+s.a.roof)+3);++y){double t=(y-s.a.p.y)/s.a.roof;cliffSin[y+64]=std::sin(t*3.14159265);cliffWidth[y+64]=.70+.30*t;}

   int l=std::max(x0,int(std::ceil(std::min(s.a.p.x,s.b.p.x)-s.pad))),r=std::min(x0+15,int(std::floor(std::max(s.a.p.x,s.b.p.x)+s.pad)));
   int n=std::max(z0,int(std::ceil(std::min(s.a.p.z,s.b.p.z)-s.pad))),b=std::min(z0+15,int(std::floor(std::max(s.a.p.z,s.b.p.z)+s.pad)));
   for(int z=n;z<=b;++z)for(int x=l;x<=r;++x){double t=((x-s.a.p.x)*s.dx+(z-s.a.p.z)*s.dz)/s.len2;t=clamp(t,0,1);
    auto k=blend(s.a,s.b,t);int i=(z-z0)*16+x-x0;double side=((x-k.p.x)*(-s.dz)+(z-k.p.z)*s.dx)/s.len;
    // Closest-point caps overlap at curved joins. Rejecting projections past
    // short span ends leaves radial solid sheets through wide chambers.
    double rx=x-k.p.x,rz=z-k.p.z,radius=(side<0?k.left:k.right)*(1+wall[i]);
    // Most cells in a span's bounding box are outside its curved footprint.
    // Reject them without sqrt/hypot; retain the original calculation inside
    // so the optimized generator produces exactly the same block boundaries.
    if(rx*rx+rz*rz>radius*radius*(s.cliff?2.:1.000000000001))continue;
    double q=s.cliff?0:std::hypot(rx,rz)/radius;if(!s.cliff&&q>=1)continue;
    if(s.cliff){
     // The wall drifts with Y and undercuts in the middle; no inserted flat
     // rectangle or isolated rock slab. Column noise roughens connected rock.
     int low=std::max(-56,int(std::ceil(k.p.y+floor[i]*1.2))),high=std::min(upperYExclusive-1,int(std::floor(k.p.y+k.roof+roof[i]*2)));
     for(int y=low;y<=high;++y){double tY=(y-k.p.y)/k.roof;
      double shift=radius*.22*(flatCliff?cliffSin[y+64]:std::sin(tY*3.14159265)),width=radius*(flatCliff?cliffWidth[y+64]:(.70+.30*tY))+wall[i];
      double aSide=side-shift,along=rx*s.dx/s.len+rz*s.dz/s.len;
      if(aSide*aSide+along*along<width*width)(*out)[i][(y+64)/64]|=uint64_t(1)<<((y+64)%64);
     }lo[i]=std::min(lo[i],low);hi[i]=std::max(hi[i],high);continue;
    }
    double rough=std::min(1.,(k.left+k.right)/5.);
    double arc=std::sqrt(1-q*q),depth=k.depth;
    // The previous -49 centreline clamp plus 1-2 block floors almost never
    // reached the -52 lava datum. Deep ordinary passages now have real basins.
    if(designEnabled)depth=std::max(depth,8.*clamp((-k.p.y-40.)/9.,0.,1.));
    double low=k.p.y-depth*(1-q*q)+floor[i]*(s.room?7:(designEnabled?1.8:.75)*rough)*(s.room?arc:1);
    if(s.thermal){
     // One continuous submerged trough, rising to irregular banks, with broad
     // variation inherited from cached column noise. No deep-water lid above it.
     low=k.p.y-7.5*(1-q*q)+floor[i]*1.8;
     out->thermal[i]=true;
    }
    double high=k.p.y+k.roof*arc+roof[i]*(s.room?9:(designEnabled?2.2:.8)*rough)*arc;
    int L=int(std::ceil(low)),H=int(std::floor(high));setRange((*out)[i],L,H);
    if(s.room){lo[i]=std::min(lo[i],L);hi[i]=std::max(hi[i],H);}
   }
  }
  for(auto const&p:g->pillars)for(int z=0;z<16;++z)for(int x=0;x<16;++x){int i=z*16+x;if(hi[i]-lo[i]<10)continue;
   double d=std::hypot(x0+x-p.x,z0+z-p.z);if(d>p.r*1.9+1)continue;
   for(int y=std::max(-55,lo[i]);y<=std::min(upperYExclusive-1,hi[i]);++y){double t=(y-lo[i])/double(hi[i]-lo[i]);
    double radius=p.r*(.7+1.1*std::pow(2*t-1,2))+.6*std::sin(t*9+p.phase)+wall[i];
    if(d<radius)(*out)[i][(y+64)/64]&=~(uint64_t(1)<<((y+64)%64));
   }
  }
  return out;
 }
public:
 // The recipe chooses the active ceiling; old recipes retain their exact
 // clipping while the fixed-capacity mask also has room for the raised world.
 explicit CavernGroups(uint32_t s,bool mega=false,bool design=false,bool lavaBand=false,bool chasms=false,int maxYExclusive=320,bool uplands=false):seed(s),rock(seedHash(s,7101)),megaEnabled(mega),designEnabled(design),lavaBandEnabled(lavaBand),moreChasms(chasms),upperYExclusive(maxYExclusive),uplandNetworks(uplands){
  if(maxYExclusive!=320&&maxYExclusive!=512)throw std::invalid_argument("Unsupported cavern height");
 }
 int maxYExclusive()const{return upperYExclusive;}
 template<class Height> Column const& column(int x,int z,Height height){
  // The block loop walks Y inside each column. Reuse its bitset pointer rather
  // than recomputing signed chunk division and local indices for every voxel.
  if(x!=columnX||z!=columnZ){int cx=int(std::floor(x/16.)),cz=int(std::floor(z/16.));
   if(cx!=activeX||cz!=activeZ){active=masks.get(key2(cx,cz),[&]{return raster(cx,cz,height);});activeX=cx;activeZ=cz;}
   activeColumn=&(*active)[(z-cz*16)*16+x-cx*16];columnX=x;columnZ=z;
  }
  return *activeColumn;
 }
 bool thermalColumn()const{return active&&active->thermal[(columnZ-activeZ*16)*16+columnX-activeX*16];}
 template<class Height> bool contains(int x,int y,int z,Height height){if(y<-56||y>=upperYExclusive)return false;
  return (column(x,z,height)[(y+64)/64]>>((y+64)%64))&1;
 }
 template<class Height> std::vector<std::array<int,3>> waterfallCandidates(int cx,int cz,Height height){
  std::vector<std::array<int,3>> out;if(!designEnabled)return out;int gx=int(std::floor(cx/44.)),gz=int(std::floor(cz/44.));auto g=group(gx,gz,height);
  for(auto const&c:g->chasms)if(c.waterfall){double t=.70,shift=c.width*.22*std::sin(t*3.14159265),width=c.width*(.70+.30*t);
   for(int side:{-1,1})for(int d=-2;d<=2;++d){double offset=shift+side*(width+d);int x=roundJS(c.p.x-std::sin(c.angle)*offset),z=roundJS(c.p.z+std::cos(c.angle)*offset),y=int(c.p.y+c.tall*t);
    if(int(std::floor(x/16.))==cx&&int(std::floor(z/16.))==cz)out.push_back({x,y,z});
   }
  }return out;
 }
 template<class Height> J inventory(int lo,int hi,Height height,bool geometry=false){J rows=J::array();
  for(int gz=int(std::floor(lo/double(Pitch)));gz<=int(std::floor(hi/double(Pitch)));++gz)for(int gx=int(std::floor(lo/double(Pitch)));gx<=int(std::floor(hi/double(Pitch)));++gx){
   auto g=group(gx,gz,height);J nodes=J::array();for(auto const&r:g->nodes)nodes.push_back({{"x",r.p.x},{"y",r.p.y},{"z",r.p.z},{"hall",r.room},{"mega",r.mega},{"active",r.active},{"length",r.length},{"width",r.width*2},{"height",r.tall},{"pillars",r.pillars},{"terrain",r.terrain},{"angle",r.angle}});
   J chasms=J::array();for(auto const&c:g->chasms)chasms.push_back({{"x",c.p.x},{"y",c.p.y},{"z",c.p.z},{"length",c.length},{"width",c.width*2},{"height",c.tall},{"angle",c.angle},{"lava",c.lava},{"waterfall",c.waterfall}});
   rows.push_back({{"lavaBands",g->lavaBands},{"chasms",chasms},{"gx",gx},{"gz",gz},{"nodes",nodes},{"links",g->links},{"attachments",g->attachments}});
   if(geometry){J bends=J::array();for(auto const&b:g->bends){J route=J::array();for(auto p:b.route)route.push_back({p.x,p.y,p.z});bends.push_back({{"shape",b.shape},{"route",route}});}rows.back()["verticalBends"]=bends;J spans=J::array();for(auto const&s:g->spans)if(!s.room)spans.push_back({{"a",{s.a.p.x,s.a.p.y,s.a.p.z}},{"b",{s.b.p.x,s.b.p.y,s.b.p.z}},{"width",s.a.left+s.a.right},{"headroom",s.a.roof+s.a.depth}});rows.back()["passages"]=std::move(spans);}
  }return rows;
 }
};
}
