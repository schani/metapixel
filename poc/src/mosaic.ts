import { G, CELLS, TileMap } from "./types";
import { Pool } from "./pool";

// Target photos are analyzed at G px: one pixel = one cell's target brightness.
export const TARGET_SIZE = G; // 256

export function cellBFromImage(img: ImageData): Uint8Array {
  if (img.width !== TARGET_SIZE || img.height !== TARGET_SIZE) {
    throw new Error(`target image must be ${TARGET_SIZE}px square`);
  }
  const cellB = new Uint8Array(CELLS);
  for (let c = 0; c < CELLS; c++) {
    cellB[c] = img.data[c * 4]; // grayscale input: R == G == B
  }
  return cellB;
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
      lumas: pool.lumas.slice(0, pool.count),
      count: pool.count,
      capacity,
    });
    this.worker.addEventListener("message", (e: MessageEvent) => {
      const { jobId, assign, homeMask, misfit, buildMs } = e.data;
      this.pending.get(jobId)?.({ assign, homeMask, misfit, buildMs });
      this.pending.delete(jobId);
    });
  }

  addLuma(idx: number, luma: number): void {
    this.worker.postMessage({ type: "addLuma", idx, luma });
  }

  build(cellB: Uint8Array): Promise<BuildResult> {
    const jobId = this.nextJob++;
    return new Promise((resolve) => {
      this.pending.set(jobId, resolve);
      this.worker.postMessage({ type: "build", jobId, cellB: cellB.slice(), G });
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
// the nearest target brightness (free cells' occupants always have homes
// elsewhere, so eviction never removes a photo's last appearance). Prefers
// interior cells so the new photo is a valid zoom target.
export function insertIntoMap(
  map: TileMap,
  idx: number,
  lumas: Uint8Array
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
  const b = lumas[idx];
  let best = 0x7fffffff;
  let bc = -1;
  let bestBorder = 0x7fffffff;
  let bcBorder = -1;
  for (let c = 0; c < CELLS; c++) {
    if (map.homeMask[c]) continue;
    const d = Math.abs(map.cellB[c] - b);
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
