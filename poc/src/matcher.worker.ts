// Principled mosaic assignment in two phases (now in color: each photo and
// each cell is described by one average RGB; luma is Rec.709 of that color).
//
// Phase A — histogram fitting: decide WHICH 65,536 tiles to use, ignoring
//   position. Runs on luma — the perceptually dominant axis. The must-have
//   mass is fixed (every pool photo once = the pool's own histogram); the
//   free budget fills the target histogram's deficits, mapped to the nearest
//   achievable brightness. The leftover surplus is the irreducible mismatch,
//   reported per build.
//
// Phase B — placement: jittered rank matching on luma, the exact solution of
//   the 1-D optimal-transport problem phase A sets up: sort cells by target
//   brightness (plus small random jitter), sort the multiset by luma, match
//   rank to rank. Monotone by construction — no brightness "crossings".
//
// Phase B2 — color, inside each luma level: all tiles of one level share the
//   same luma, so only chroma distinguishes them. Cells of the level are
//   sorted by a warm–cool key (R−B, plus jitter); the level's must-have
//   photos (each used exactly once) are placed by monotone minimum-cost
//   matching on that key — the same no-crossings guarantee as luma, one
//   dimension down. The remaining cells are free: each picks a random photo
//   from the level's near-nearest set in full RGB distance (a tolerance band
//   around the best match keeps the variety that random duplicates used to
//   provide, without letting one photo carpet a flat region).

const JITTER = 10; // +- luma jitter on cell targets before rank matching
const CHROMA_JITTER = 10; // +- jitter on the warm-cool key inside a level
const FREE_TOL = 12; // free picks accept photos within this RGB distance of the best
// Where the target color is unreachable (e.g. blue sky: no blue photos at
// that luma), the tolerance band can shrink to 2-3 photos which then carpet
// the region. Guarantee at least this many candidates (the nearest ones).
const MIN_CAND = 8;

let count = 0;
let rgbs = new Uint8Array(0); // 3 bytes per photo: avg R, G, B
let lumas = new Uint8Array(0); // Rec.709 luma of the avg color

function lumaOf(r: number, g: number, b: number): number {
  return Math.round(0.2126 * r + 0.7152 * g + 0.0722 * b);
}

// Warm–cool chroma key of a photo.
function ckey(p: number): number {
  return rgbs[p * 3] - rgbs[p * 3 + 2];
}

// Photo buckets by luma, each sorted ascending by chroma key.
let buckets: Int32Array[] = [];
let bucketLen = new Int32Array(256);

function rebuildBuckets(): void {
  bucketLen = new Int32Array(256);
  for (let p = 0; p < count; p++) bucketLen[lumas[p]]++;
  buckets = Array.from({ length: 256 }, (_, b) => new Int32Array(bucketLen[b]));
  const fill = new Int32Array(256);
  for (let p = 0; p < count; p++) {
    const b = lumas[p];
    buckets[b][fill[b]++] = p;
  }
  for (const b of buckets) b.sort((x, y) => ckey(x) - ckey(y));
}

// Distance from `b` to the nearest luma with len[luma] > 0; -1 if none.
function nearestDist(b: number, len: Int32Array): number {
  for (let d = 0; d < 256; d++) {
    if (b - d >= 0 && len[b - d] > 0) return d;
    if (b + d <= 255 && len[b + d] > 0) return d;
  }
  return -1;
}

// Pick the luma at distance d from b, choosing randomly (availability-
// weighted) when both sides qualify.
function lumaAt(b: number, d: number, len: Int32Array): number {
  const lo = b - d, hi = b + d;
  const nLo = lo >= 0 ? len[lo] : 0;
  const nHi = hi <= 255 && d > 0 ? len[hi] : 0;
  return Math.floor(Math.random() * (nLo + nHi)) < nLo ? lo : hi;
}

function shuffle(a: Int32Array): void {
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    const t = a[i]; a[i] = a[j]; a[j] = t;
  }
}

function build(
  cellRGB: Uint8Array,
  G: number
): { assign: Int32Array; homeMask: Uint8Array; misfit: number /* EMD */ } {
  const cells = cellRGB.length / 3;
  const cellLuma = new Uint8Array(cells);
  for (let c = 0; c < cells; c++) {
    cellLuma[c] = lumaOf(cellRGB[c * 3], cellRGB[c * 3 + 1], cellRGB[c * 3 + 2]);
  }

  // ---- Phase A: the multiset. rem[l] = tiles of luma l we will use.
  const T = new Int32Array(256);
  for (let c = 0; c < cells; c++) T[cellLuma[c]]++;

  const rem = new Int32Array(256);
  rem.set(bucketLen); // mandatory: every photo once

  // Free budget fills target deficits, mapped to nearest achievable luma.
  const demand = new Float64Array(256);
  let sumDemand = 0;
  for (let l = 0; l < 256; l++) {
    const d = T[l] - bucketLen[l];
    if (d > 0) {
      const dist = nearestDist(l, bucketLen);
      if (dist < 0) continue;
      demand[lumaAt(l, dist, bucketLen)] += d;
      sumDemand += d;
    }
  }
  const F = cells - count;
  if (sumDemand > 0 && F > 0) {
    let assigned = 0;
    let maxL = 0;
    for (let l = 0; l < 256; l++) {
      const add = Math.floor((demand[l] * F) / sumDemand);
      rem[l] += add;
      assigned += add;
      if (demand[l] > demand[maxL]) maxL = l;
    }
    rem[maxL] += F - assigned; // rounding remainder
  }

  // Irreducible mismatch: earth-mover's distance between the multiset and
  // the target histogram (1-D EMD = sum of |CDF differences|). Divided by
  // cell count downstream, it reads as "average brightness error per tile".
  let misfit = 0;
  let cumA = 0, cumT = 0;
  for (let l = 0; l < 256; l++) {
    cumA += rem[l];
    cumT += T[l];
    misfit += Math.abs(cumA - cumT);
  }

  // ---- Phase B: jittered rank matching on luma.
  const assign = new Int32Array(cells);
  const homeMask = new Uint8Array(cells);

  // Sort cells by jittered target via counting sort. Cells are scattered
  // into key buckets in random order, so ties (flat regions) break randomly.
  const KEY_RANGE = 256 + 2 * JITTER;
  const keys = new Int32Array(cells);
  const keyCount = new Int32Array(KEY_RANGE);
  const scatter = new Int32Array(cells);
  for (let i = 0; i < cells; i++) scatter[i] = i;
  shuffle(scatter);
  for (let i = 0; i < cells; i++) {
    const c = scatter[i];
    const k = cellLuma[c] + JITTER + Math.round((Math.random() * 2 - 1) * JITTER);
    keys[c] = Math.max(0, Math.min(KEY_RANGE - 1, k));
    keyCount[keys[c]]++;
  }
  const keyStart = new Int32Array(KEY_RANGE);
  for (let k = 1; k < KEY_RANGE; k++) keyStart[k] = keyStart[k - 1] + keyCount[k - 1];
  const sortedCells = new Int32Array(cells);
  const fillPos = keyStart.slice();
  for (let i = 0; i < cells; i++) {
    const c = scatter[i];
    sortedCells[fillPos[keys[c]]++] = c;
  }

  // ---- Phase B2: within each luma level, place by chroma.
  // Memoized free-pick candidate lists, keyed by (level, quantized cell RGB).
  const freeCand = new Map<number, Int32Array>();

  let cursor = 0;
  for (let a = 0; a < 256; a++) {
    const K = rem[a];
    if (K <= 0) continue;
    const blockStart = cursor;
    cursor += K;

    // Level's bucket; every rem[a] > 0 has bucketLen[a] > 0 by construction,
    // but fall back to the nearest non-empty bucket just in case.
    let bucket = buckets[a];
    if (bucket.length === 0) {
      const d = nearestDist(a, bucketLen);
      bucket = buckets[lumaAt(a, d, bucketLen)];
    }
    const M = Math.min(bucket.length, K);

    // Sort the level's cells by jittered warm–cool key. Pack key<<17 | rank
    // so a plain numeric sort carries the rank along (cells <= 65536 < 2^17).
    const packed = new Int32Array(K);
    for (let i = 0; i < K; i++) {
      const c = sortedCells[blockStart + i];
      const jit = Math.round((Math.random() * 2 - 1) * CHROMA_JITTER);
      const k = cellRGB[c * 3] - cellRGB[c * 3 + 2] + jit; // -275..275
      packed[i] = ((k + 512) << 17) | i;
    }
    packed.sort();
    const cellAt = (r: number): number =>
      sortedCells[blockStart + (packed[r] & 0x1ffff)];

    // Must-haves: monotone minimum-cost matching of the bucket's M photos
    // (sorted by chroma key) onto the K sorted cells. dp[j][c] = best cost
    // matching the first j photos into the first c cells.
    const claimed = new Uint8Array(K);
    if (M > 0 && bucket === buckets[a]) {
      const pk = new Int32Array(M);
      for (let j = 0; j < M; j++) pk[j] = ckey(bucket[j]);
      const ck = new Int32Array(K);
      for (let r = 0; r < K; r++) ck[r] = (packed[r] >> 17) - 512;

      let prev = new Float64Array(K + 1); // dp[j-1][*], starts as dp[0][*] = 0
      let cur = new Float64Array(K + 1);
      const take = new Uint8Array(M * K);
      for (let j = 1; j <= M; j++) {
        cur[j - 1] = Infinity;
        for (let c = j; c <= K; c++) {
          const skip = cur[c - 1];
          const t = prev[c - 1] + Math.abs(pk[j - 1] - ck[c - 1]);
          if (t <= skip) {
            cur[c] = t;
            take[(j - 1) * K + (c - 1)] = 1;
          } else {
            cur[c] = skip;
            take[(j - 1) * K + (c - 1)] = 0;
          }
        }
        const tmp = prev; prev = cur; cur = tmp;
      }
      let j = M, c = K;
      while (j > 0) {
        if (take[(j - 1) * K + (c - 1)]) {
          const cell = cellAt(c - 1);
          assign[cell] = bucket[j - 1];
          homeMask[cell] = 1;
          claimed[c - 1] = 1;
          j--;
        }
        c--;
      }
    }

    // Free cells: a random photo from the near-nearest set in RGB distance.
    for (let r = 0; r < K; r++) {
      if (claimed[r]) continue;
      const cell = cellAt(r);
      const cr = cellRGB[cell * 3];
      const cg = cellRGB[cell * 3 + 1];
      const cb = cellRGB[cell * 3 + 2];
      const q = ((cr >> 3) << 10) | ((cg >> 3) << 5) | (cb >> 3);
      const memoKey = (a << 15) | q;
      let cand = freeCand.get(memoKey);
      if (!cand) {
        const n = bucket.length;
        const byDist: [number, number][] = new Array(n);
        let best = Infinity;
        for (let j = 0; j < n; j++) {
          const p = bucket[j];
          const dr = rgbs[p * 3] - cr;
          const dg = rgbs[p * 3 + 1] - cg;
          const db = rgbs[p * 3 + 2] - cb;
          const d2 = dr * dr + dg * dg + db * db;
          byDist[j] = [d2, p];
          if (d2 < best) best = d2;
        }
        byDist.sort((x, y) => x[0] - y[0]);
        const lim = (Math.sqrt(best) + FREE_TOL) ** 2;
        let take = 0;
        while (take < n && (byDist[take][0] <= lim || take < MIN_CAND)) take++;
        cand = new Int32Array(take);
        for (let j = 0; j < take; j++) cand[j] = byDist[j][1];
        freeCand.set(memoKey, cand);
      }
      assign[cell] = cand[Math.floor(Math.random() * cand.length)];
    }
  }
  return { assign, homeMask, misfit };
}

self.addEventListener("message", (e: MessageEvent) => {
  const msg = e.data;
  if (msg.type === "init") {
    rgbs = new Uint8Array(msg.capacity * 3);
    rgbs.set(msg.rgbs, 0);
    lumas = new Uint8Array(msg.capacity);
    count = msg.count;
    for (let p = 0; p < count; p++) {
      lumas[p] = lumaOf(rgbs[p * 3], rgbs[p * 3 + 1], rgbs[p * 3 + 2]);
    }
    rebuildBuckets();
  } else if (msg.type === "addPhoto") {
    rgbs.set(msg.rgb, msg.idx * 3);
    lumas[msg.idx] = lumaOf(msg.rgb[0], msg.rgb[1], msg.rgb[2]);
    count = Math.max(count, msg.idx + 1);
    rebuildBuckets();
  } else if (msg.type === "build") {
    const t0 = performance.now();
    const { assign, homeMask, misfit } = build(msg.cellRGB, msg.G);
    (self as any).postMessage(
      {
        jobId: msg.jobId,
        assign,
        homeMask,
        misfit,
        buildMs: Math.round(performance.now() - t0),
      },
      [assign.buffer, homeMask.buffer]
    );
  }
});
