// noise.js — 种子化改进型 Perlin(2D)+ fbm / ridged 组合层
export function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export function seedHash(seed, k) {
  let s = (seed ^ Math.imul(k + 1, 0x9E3779B1)) >>> 0;
  s = Math.imul(s ^ (s >>> 16), 0x21F0AAAD) >>> 0;
  s = Math.imul(s ^ (s >>> 15), 0x735A2D97) >>> 0;
  return (s ^ (s >>> 15)) >>> 0;
}

/* 世界坐标确定性哈希 → [0,1):地物逐格判定用,跨 tile 可复现 */
export function hash01(x, z, seed) {
  let h = (seed ^ Math.imul(x | 0, 0x27d4eb2d) ^ Math.imul(z | 0, 0x165667b1)) | 0;
  h = Math.imul(h ^ (h >>> 15), 0x85ebca6b);
  h = Math.imul(h ^ (h >>> 13), 0xc2b2ae35);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

const fade = t => t * t * t * (t * (t * 6 - 15) + 10);
const lerp = (a, b, t) => a + t * (b - a);
const G2 = [[1, 1], [-1, 1], [1, -1], [-1, -1], [1, 0], [-1, 0], [0, 1], [0, -1]];
function grad2(h, x, y) { const g = G2[h & 7]; return g[0] * x + g[1] * y; }
const G3 = [[1, 1, 0], [-1, 1, 0], [1, -1, 0], [-1, -1, 0], [1, 0, 1], [-1, 0, 1],
  [1, 0, -1], [-1, 0, -1], [0, 1, 1], [0, -1, 1], [0, 1, -1], [0, -1, -1]];
function grad3(h, x, y, z) { const g = G3[h % 12]; return g[0] * x + g[1] * y + g[2] * z; }

export class Perlin {
  constructor(seed) {
    const p = new Uint8Array(256);
    for (let i = 0; i < 256; i++) p[i] = i;
    const rnd = mulberry32(seed);
    for (let i = 255; i > 0; i--) {
      const j = (rnd() * (i + 1)) | 0;
      const t = p[i]; p[i] = p[j]; p[j] = t;
    }
    this.perm = new Uint8Array(512);
    for (let i = 0; i < 512; i++) this.perm[i] = p[i & 255];
  }

  perlin2(x, y) {
    const perm = this.perm;
    const X = Math.floor(x), Y = Math.floor(y);
    const xf = x - X, yf = y - Y;
    const xi = X & 255, yi = Y & 255;
    const u = fade(xf), v = fade(yf);
    const aa = perm[perm[xi] + yi], ab = perm[perm[xi] + yi + 1];
    const ba = perm[perm[xi + 1] + yi], bb = perm[perm[xi + 1] + yi + 1];
    const x1 = lerp(grad2(aa, xf, yf), grad2(ba, xf - 1, yf), u);
    const x2 = lerp(grad2(ab, xf, yf - 1), grad2(bb, xf - 1, yf - 1), u);
    return lerp(x1, x2, v);
  }

  fbm2(x, y, oct = 5, lac = 2, gain = 0.5) {
    let s = 0, a = 1, n = 0, f = 1;
    for (let i = 0; i < oct; i++) {
      s += a * this.perlin2(x * f, y * f);
      n += a; a *= gain; f *= lac;
    }
    return s / n;
  }

  ridged2(x, y, oct = 4, lac = 2, gain = 0.5) {
    let s = 0, a = 1, n = 0, f = 1;
    for (let i = 0; i < oct; i++) {
      const v = 1 - Math.abs(this.perlin2(x * f, y * f));
      s += a * v * v; n += a; a *= gain; f *= lac;
    }
    return s / n;
  }

  perlin3(x, y, z) {
    const perm = this.perm;
    const X = Math.floor(x) & 255, Y = Math.floor(y) & 255, Z = Math.floor(z) & 255;
    x -= Math.floor(x); y -= Math.floor(y); z -= Math.floor(z);
    const u = fade(x), v = fade(y), w = fade(z);
    const A = perm[X] + Y, AA = perm[A] + Z, AB = perm[A + 1] + Z;
    const Bb = perm[X + 1] + Y, BA = perm[Bb] + Z, BB = perm[Bb + 1] + Z;
    return lerp(
      lerp(lerp(grad3(perm[AA], x, y, z), grad3(perm[BA], x - 1, y, z), u),
           lerp(grad3(perm[AB], x, y - 1, z), grad3(perm[BB], x - 1, y - 1, z), u), v),
      lerp(lerp(grad3(perm[AA + 1], x, y, z - 1), grad3(perm[BA + 1], x - 1, y, z - 1), u),
           lerp(grad3(perm[AB + 1], x, y - 1, z - 1), grad3(perm[BB + 1], x - 1, y - 1, z - 1), u), v),
      w);
  }

  fbm3(x, y, z, oct = 3, lac = 2, gain = 0.5) {
    let s = 0, a = 1, n = 0, f = 1;
    for (let i = 0; i < oct; i++) {
      s += a * this.perlin3(x * f, y * f, z * f);
      n += a; a *= gain; f *= lac;
    }
    return s / n;
  }
}
