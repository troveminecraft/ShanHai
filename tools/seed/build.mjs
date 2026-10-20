import fs from 'node:fs';
import crypto from 'node:crypto';
import {createPillarLayout} from './pillars.mjs';
import {hash01,seedHash} from './noise.mjs';
import './deepslate-0.19.0.mjs';
const [seedText,templatePath,out]=process.argv.slice(2);
const kernelSource=fs.readFileSync(new URL('../../src/NativeRuntime.h',import.meta.url),'utf8');
const kernelVersion=/KernelVersion\[\]\s*=\s*"([^"]+)"/.exec(kernelSource)?.[1];
if(!kernelVersion)throw Error('Missing native kernel version');
if(!/^-?(0|[1-9][0-9]*)$/.test(seedText??''))throw Error('Expected signed 64-bit decimal terrain seed');
const seed=BigInt(seedText);if(seed<-(1n<<63n)||seed>=(1n<<63n))throw Error('Terrain seed outside signed 64-bit range');
const t=JSON.parse(fs.readFileSync(templatePath,'utf8')),C=structuredClone(t.config);
const bits=BigInt.asUintN(64,seed),lo=Number(bits&0xffffffffn),hi=Number(bits>>32n);
// Preserve the existing 32-bit streams for small positive seeds; use high bits too.
C.seed=(lo^(hi===0?0:seedHash(hi,1901)))>>>0;
const rnd=(i,s)=>hash01(i,0,C.seed^s);
const sf=C.stoneForest,gf=C.giantForest;
sf.centerX=Math.floor((rnd(0,1601)-.5)*3600);sf.centerZ=Math.floor((rnd(0,1603)-.5)*3600);
let selected=false;const giantCandidates=[];
for(let i=0;i<128;i++){
 const x=Math.floor((rnd(i,1611)-.5)*4400),z=Math.floor((rnd(i,1613)-.5)*4400);
 if(sf.largeEnabled!==false&&Math.hypot(x-sf.centerX,z-sf.centerZ)<sf.outerRadius+sf.edgeWarp+gf.outerRadius+180)continue;
 giantCandidates.push([x,z]);if(!selected){gf.centerX=x;gf.centerZ=z;selected=true;}
}
if(!selected)throw Error('Cannot place separated special regions');
const {NormalNoise,XoroshiroRandom}=globalThis.deepslate;
const random=XoroshiroRandom.create(seed).forkPositional();
const noises=Object.fromEntries(Object.entries(t.noiseDefinitions).map(([id,def])=>[id,new NormalNoise(random.fromHashOf(id),def)]));
const data={format:1,version:kernelVersion,config:C,functions:t.functions,settings:t.settings,noises,pillars:createPillarLayout(C),naturalLakes:[],seedManifest:{algorithm:'karma-seed-package-v1',terrainSeed:seed.toString(),runtimeSeed:C.seed,giantCandidates,templateSha256:crypto.createHash('sha256').update(fs.readFileSync(templatePath)).digest('hex'),ready:false}};
fs.writeFileSync(out,JSON.stringify(data));
console.log(JSON.stringify({seed:seed.toString(),runtimeSeed:C.seed,pillars:data.pillars.length,stone:[sf.centerX,sf.centerZ],giant:[gf.centerX,gf.centerZ]}));
