import { G, CELLS, TileMap } from "./types";
import { Pool } from "./pool";

// Target photos are analyzed at G px: one pixel = one cell's target color.
export const TARGET_SIZE = G; // 256

export function cellRGBFromImage(img: ImageData): Uint8Array {
  if (img.width !== TARGET_SIZE || img.height !== TARGET_SIZE) {
    throw new Error(`target image must be ${TARGET_SIZE}px square`);
  }
  const cellRGB = new Uint8Array(CELLS * 3);
  for (let c = 0; c < CELLS; c++) {
    cellRGB[c * 3] = img.data[c * 4];
    cellRGB[c * 3 + 1] = img.data[c * 4 + 1];
    cellRGB[c * 3 + 2] = img.data[c * 4 + 2];
  }
  return cellRGB;
}

export interface BuildResult {
  assign: Int32Array;
  homeMask: Uint8Array;
  misfit: number; // histogram EMD; /cells = avg brightness error per tile
  buildMs: number;
}

// Wraps the matcher worker with a promise-based build queue.
export class Matcher {
  private worker: Worker;
  private nextJob = 1;
  private pending = new Map<number, (r: BuildResult) => void>();

  constructor(pool: Pool, capacity: number) {
    this.worker = new Worker(new URL("./matcher.worker.ts", import.meta.url), {
      type: "module",
    });
    this.worker.postMessage({
      type: "init",
      rgbs: pool.rgbs.slice(0, pool.count * 3),
      count: pool.count,
      capacity,
    });
    this.worker.addEventListener("message", (e: MessageEvent) => {
      const { jobId, assign, homeMask, misfit, buildMs } = e.data;
      this.pending.get(jobId)?.({ assign, homeMask, misfit, buildMs });
      this.pending.delete(jobId);
    });
  }

  addPhoto(idx: number, rgb: Uint8Array): void {
    this.worker.postMessage({ type: "addPhoto", idx, rgb: rgb.slice() });
  }

  // bw: match on luminosity only (chroma ignored), for B&W mode.
  build(cellRGB: Uint8Array, bw: boolean): Promise<BuildResult> {
    const jobId = this.nextJob++;
    return new Promise((resolve) => {
      this.pending.set(jobId, resolve);
      this.worker.postMessage({ type: "build", jobId, cellRGB: cellRGB.slice(), G, bw });
    });
  }
}

// Cells within a 2-cell margin of the mosaic border are bad zoom targets:
// the camera view around them pokes outside the parent and can never rebase.
export function isInterior(cell: number): boolean {
  const cx = cell % G;
  const cy = Math.floor(cell / G);
  return cx >= 2 && cx <= G - 3 && cy >= 2 && cy <= G - 3;
}

// Give photo `idx` a home in an existing map by claiming the free cell with
// the nearest target color — luminosity only in B&W mode (free cells'
// occupants always have homes elsewhere, so eviction never removes a photo's
// last appearance). Prefers interior cells so the new photo is a valid zoom
// target.
export function insertIntoMap(
  map: TileMap,
  idx: number,
  pool: Pool,
  bw: boolean
): boolean {
  let freeSpot = -1;
  for (let c = 0; c < CELLS; c++) {
    if (map.assign[c] === idx) {
      if (map.homeMask[c]) return false;
      if (freeSpot < 0 || isInterior(c)) freeSpot = c;
    }
  }
  if (freeSpot >= 0) {
    // Already present by natural matching — just make that spot permanent.
    map.homeMask[freeSpot] = 1;
    return false;
  }
  const r = pool.rgbs[idx * 3];
  const g = pool.rgbs[idx * 3 + 1];
  const b = pool.rgbs[idx * 3 + 2];
  const luma = pool.lumas[idx];
  let best = Infinity;
  let bc = -1;
  let bestBorder = Infinity;
  let bcBorder = -1;
  for (let c = 0; c < CELLS; c++) {
    if (map.homeMask[c]) continue;
    let d: number;
    if (bw) {
      const cl =
        0.2126 * map.cellRGB[c * 3] +
        0.7152 * map.cellRGB[c * 3 + 1] +
        0.0722 * map.cellRGB[c * 3 + 2];
      d = Math.abs(cl - luma);
    } else {
      const dr = map.cellRGB[c * 3] - r;
      const dg = map.cellRGB[c * 3 + 1] - g;
      const db = map.cellRGB[c * 3 + 2] - b;
      d = dr * dr + dg * dg + db * db;
    }
    if (isInterior(c)) {
      if (d < best) {
        best = d;
        bc = c;
      }
    } else if (d < bestBorder) {
      bestBorder = d;
      bcBorder = c;
    }
  }
  if (bc < 0) bc = bcBorder;
  if (bc < 0) return false; // no free cells left (pool outgrew the grid)
  map.assign[bc] = idx;
  map.homeMask[bc] = 1;
  return true;
}
