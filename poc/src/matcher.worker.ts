// Principled mosaic assignment in two phases:
//
// Phase A — histogram fitting: decide WHICH 65,536 tiles to use, ignoring
//   position. The must-have mass is fixed (every pool photo once = the pool's
//   own histogram); the free budget fills the target histogram's deficits,
//   mapped to the nearest achievable brightness. The leftover surplus is the
//   irreducible mismatch, reported per build.
//
// Phase B — placement: jittered rank matching, the exact solution of the 1-D
//   optimal-transport problem phase A sets up: sort cells by target
//   brightness (plus small random jitter), sort the multiset by luma, match
//   rank to rank. Monotone by construction — a darker tile can never end up
//   on a brighter cell than a brighter tile (no "crossings"), which greedy
//   online placement could not guarantee. The jitter converts would-be
//   contours at pool-supply gaps into dither noise and randomizes
//   tie-breaking within flat regions, so irreducible surplus tiles scatter
//   uniformly. Must-have photos are issued at random positions within their
//   brightness level, by the same rule as free duplicates.

const JITTER = 10; // +- luma jitter on cell targets before rank matching

let count = 0;
let lumas = new Uint8Array(0);

// Photo buckets by brightness.
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
  cellB: Uint8Array,
  G: number
): { assign: Int32Array; homeMask: Uint8Array; misfit: number /* EMD */ } {
  const cells = cellB.length;

  // ---- Phase A: the multiset. rem[l] = tiles of brightness l we will use.
  const T = new Int32Array(256);
  for (let c = 0; c < cells; c++) T[cellB[c]]++;

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

  // Must-have issuance order within each brightness level.
  const mustOrder = buckets.map((b) => {
    const copy = b.slice();
    shuffle(copy);
    return copy;
  });
  const mustPtr = new Int32Array(256);

  // ---- Phase B: jittered rank matching.
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
    const k = cellB[c] + JITTER + Math.round((Math.random() * 2 - 1) * JITTER);
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

  // Walk the multiset in luma order, consuming rank-matched cells. Within
  // each luma level, shuffle the cell block so must-have photos land at
  // random positions among that level's cells.
  let cursor = 0;
  const block: number[] = [];
  for (let a = 0; a < 256; a++) {
    let n = rem[a];
    if (n <= 0) continue;
    block.length = 0;
    while (n-- > 0) block.push(sortedCells[cursor++]);
    for (let i = block.length - 1; i > 0; i--) {
      const j = Math.floor(Math.random() * (i + 1));
      const t = block[i]; block[i] = block[j]; block[j] = t;
    }
    for (const c of block) {
      if (mustPtr[a] < mustOrder[a].length) {
        assign[c] = mustOrder[a][mustPtr[a]++];
        homeMask[c] = 1;
      } else {
        assign[c] = buckets[a][Math.floor(Math.random() * bucketLen[a])];
      }
    }
  }
  return { assign, homeMask, misfit };
}

self.addEventListener("message", (e: MessageEvent) => {
  const msg = e.data;
  if (msg.type === "init") {
    lumas = new Uint8Array(msg.capacity);
    lumas.set(msg.lumas, 0);
    count = msg.count;
    rebuildBuckets();
  } else if (msg.type === "addLuma") {
    lumas[msg.idx] = msg.luma;
    count = Math.max(count, msg.idx + 1);
    rebuildBuckets();
  } else if (msg.type === "build") {
    const t0 = performance.now();
    const { assign, homeMask, misfit } = build(msg.cellB, msg.G);
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
