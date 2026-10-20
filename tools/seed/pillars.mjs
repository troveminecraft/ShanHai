import {Perlin,seedHash,hash01} from './noise.mjs';
export function createPillarLayout(C){
if(C.stoneForest.largeEnabled===false)return [];
const q=C.stoneForest,p=new Perlin(seedHash(C.seed,q.seed));
const smooth=(a,b,x)=>{const t=Math.max(0,Math.min(1,(x-a)/(b-a)));return t*t*(3-2*t);};
const rand=(x,z,s)=>hash01(x,z,C.seed^(q.seed+s));
// A curved west/east boundary passes through the origin. Only the forest's
// local layout seed changes; the independent terrain seed remains unchanged.
function smallForestWeight(x,z){
 const boundary=q.boundaryWarp*(p.perlin2(19,z/q.boundaryWavelength+7)-p.perlin2(19,7));
 return smooth(-q.transitionHalfWidth,q.transitionHalfWidth,x-boundary);
}
let layout=null;
const bucket=128,segments=64;
function radius(a,t){return a.radius*(1+.07*Math.cos(2*t+a.phase)+.06*Math.sin(3*t+a.phase*1.7)+.035*Math.cos(5*t-a.phase));}
function shape(id,small){
 const r=rand(id,13,17),phase=rand(id,29,19)*Math.PI*2;
 const a={id:String(id),small,radius:small?q.smallRadiusMin+r*(q.smallRadiusMax-q.smallRadiusMin):q.largeRadiusMin+r*(q.largeRadiusMax-q.largeRadiusMin),phase,
  height:small?q.smallHeightMin+rand(id,31,23)*(q.smallHeightMax-q.smallHeightMin):q.largeHeightMin+rand(id,31,23)*(q.largeHeightMax-q.largeHeightMin)};
 a.outer=a.radius*1.165;a.points=[];
 for(let i=0;i<segments;i++){const t=i*Math.PI*2/segments,r=radius(a,t);a.points.push([r*Math.cos(t),r*Math.sin(t)]);}
 return a;
}
function separation(a,b){
 const dx=b.x-a.x,dz=b.z-a.z,d=Math.hypot(dx,dz);
 if(d<=radius(a,Math.atan2(dz,dx))||d<=radius(b,Math.atan2(-dz,-dx)))return 0;
 let best=Infinity;
 for(const v of a.points)for(const w of b.points){const x=v[0]-w[0]-dx,z=v[1]-w[1]-dz,s=x*x+z*z;if(s<best)best=s;}
 return Math.sqrt(best);
}
function getLayout(){
 if(layout)return layout;const started=performance.now(),pillars=[],bins=new Map();
 const add=a=>{pillars.push(a);const key=Math.floor(a.x/bucket)+','+Math.floor(a.z/bucket);if(!bins.has(key))bins.set(key,[]);bins.get(key).push(a);};
 const nearby=(x,z,r)=>{const out=[];for(let bz=Math.floor((z-r)/bucket);bz<=Math.floor((z+r)/bucket);bz++)for(let bx=Math.floor((x-r)/bucket);bx<=Math.floor((x+r)/bucket);bx++)out.push(...(bins.get(bx+','+bz)||[]));return out;};
 const left=shape(0,false),right=shape(1,true);left.x=-6-Math.max(...left.points.map(v=>v[0]));left.z=0;right.x=6-Math.min(...right.points.map(v=>v[0]));right.z=0;add(left);add(right);
 // Deterministic tangent packing of circular profiles. Each accepted column
 // is 12..14 blocks from an older neighbour; no rectangular ownership cells.
 const limit=q.outerRadius+q.edgeWarp+100;let attempts=0;
 for(let i=0;i<pillars.length&&pillars.length<q.maxPillars;i++){
  const anchor=pillars[i];
  for(let j=0;j<32&&pillars.length<q.maxPillars;j++){
   const id=2+i*32+j,angle=j*2.399963229728653+rand(i,5,11)*Math.PI*2;
   const a=shape(id,rand(id,7,3)<smallForestWeight(anchor.x+70*Math.cos(angle),anchor.z+70*Math.sin(angle)));
   let d=anchor.outer+a.outer+12;const dx=Math.cos(angle),dz=Math.sin(angle);
   a.x=anchor.x+d*dx;a.z=anchor.z+d*dz;attempts++;
   if(Math.hypot(a.x,a.z)>limit)continue;
   for(let k=0;k<4;k++){const gap=separation(a,anchor);d-=Math.max(0,gap-12);a.x=anchor.x+d*dx;a.z=anchor.z+d*dz;}
   const region=smallForestWeight(a.x,a.z);if((region===0&&a.small)||(region===1&&!a.small))continue;
   let nearest=Infinity,valid=true;
   for(const b of nearby(a.x,a.z,a.outer+q.largeRadiusMax*1.165+16)){
    if(Math.hypot(a.x-b.x,a.z-b.z)>a.outer+b.outer+16)continue;
    const gap=separation(a,b);if(gap<11.5){valid=false;break;}nearest=Math.min(nearest,gap);
   }
   if(valid&&nearest<=14){a.nearestGap=nearest;add(a);}
  }
 }
 layout={pillars,bins,nearby,buildMs:performance.now()-started,attempts};return layout;
}
function pillarLayout(){return getLayout().pillars.map(({points,...a})=>({...a}));}

return pillarLayout();
}
