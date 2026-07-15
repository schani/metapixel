import { G, Camera, Level, Rect, TileMap } from "./types";
import { Pool } from "./pool";
import { Matcher, cellBFromImage, insertIntoMap, isInterior } from "./mosaic";
import { Renderer } from "./renderer";

const FLY_SECONDS = 16; // each level is a 256x zoom now
const DWELL_SECONDS = 1.6;
// Camera height when "arrived" at a photo: slightly more than half the photo,
// so the whole face is visible with a margin.
const ARRIVE_FACTOR = 0.55;

interface Flight {
  t: number;
  dur: number;
  x0: number; y0: number; h0: number;
  x1: number; y1: number; h1: number;
}

// Drives the endless zoom: which photo is next, camera flights into its
// tile, and rebasing the coordinate system when a level fills the view.
export class Choreo {
  levels: Level[] = [];
  queue: number[] = []; // freshly captured photos waiting for their moment
  cam: Camera = { x: 0.5, y: 0.5, h: ARRIVE_FACTOR };
  state: "boot" | "dwell" | "fly" = "boot";
  onArrive: (photoIdx: number) => void = () => {};
  private flight: Flight | null = null;
  private dwellT = 0;
  private aspect = 16 / 9;

  constructor(
    private pool: Pool,
    private matcher: Matcher,
    private renderer: Renderer
  ) {}

  async start(rootIdx: number): Promise<void> {
    const root = this.makeLevel(rootIdx, { x: 0, y: 0, size: 1 });
    this.levels.push(root);
    await this.buildMap(root);
    this.state = "dwell";
    this.dwellT = DWELL_SECONDS;
  }

  onNewPhoto(idx: number): void {
    this.queue.push(idx);
    // Make it visible (and reachable) immediately in all live mosaics.
    for (const level of this.levels) {
      if (level.map && insertIntoMap(level.map, idx, this.pool.lumas)) {
        this.renderer.buildLevelVBO(level);
      }
    }
  }

  update(dt: number, aspect: number): void {
    this.aspect = aspect;
    if (this.state === "dwell") {
      this.dwellT -= dt;
      const leaf = this.levels[this.levels.length - 1];
      if (this.dwellT <= 0 && leaf.map) this.pickNext();
    } else if (this.state === "fly" && this.flight) {
      const f = this.flight;
      f.t = Math.min(f.dur, f.t + dt);
      const p0 = f.t / f.dur;
      const p = p0 * p0 * (3 - 2 * p0); // ease in-out
      const h = f.h0 * Math.pow(f.h1 / f.h0, p);
      const denom = f.h0 - f.h1;
      const w = Math.abs(denom) < 1e-9 ? p : (f.h0 - h) / denom;
      this.cam.x = f.x0 + (f.x1 - f.x0) * w;
      this.cam.y = f.y0 + (f.y1 - f.y0) * w;
      this.cam.h = h;
      if (f.t >= f.dur) {
        this.flight = null;
        this.state = "dwell";
        this.dwellT = DWELL_SECONDS;
        this.onArrive(this.levels[this.levels.length - 1].photoIdx);
      }
    }
    this.maybeRebase();
  }

  private makeLevel(photoIdx: number, rect: Rect, tintB = 0.5): Level {
    return { photoIdx, rect, map: null, vbo: null, tintB };
  }

  private async buildMap(level: Level): Promise<void> {
    const img = await this.pool.getPixels256(level.photoIdx);
    const cellB = cellBFromImage(img);
    const before = this.pool.count;
    const { assign, homeMask, misfit, buildMs } = await this.matcher.build(cellB);
    const map: TileMap = { assign, cellB, homeMask };
    // Photos captured while the build was running.
    for (let idx = before; idx < this.pool.count; idx++) {
      insertIntoMap(map, idx, this.pool.lumas);
    }
    level.map = map;
    this.renderer.buildLevelVBO(level);
    console.log(
      `mosaic for #${level.photoIdx} built in ${buildMs}ms, ` +
        `avg brightness error ${(misfit / assign.length).toFixed(1)}`
    );
  }

  private pickNext(): void {
    const leaf = this.levels[this.levels.length - 1];
    const map = leaf.map!;
    let cell = -1;

    if (this.queue.length > 0) {
      const idx = this.queue.shift()!;
      insertIntoMap(map, idx, this.pool.lumas); // no-op if already present
      const cells = [];
      for (let c = 0; c < map.assign.length; c++) {
        if (map.assign[c] === idx && isInterior(c)) cells.push(c);
      }
      if (cells.length === 0) {
        // Present only at the border: fall back to any placement.
        for (let c = 0; c < map.assign.length; c++) {
          if (map.assign[c] === idx) cells.push(c);
        }
      }
      if (cells.length > 0) {
        cell = cells[Math.floor(Math.random() * cells.length)];
      }
    }
    if (cell < 0) {
      // Idle wander: pick a random PHOTO uniformly — picking a random tile
      // would bias toward over-represented photos. Every photo is guaranteed
      // to appear somewhere in the mosaic, so zoom to one of its cells.
      let idx = -1;
      const nWebcam = this.pool.count - this.pool.seedCount;
      for (let tries = 0; tries < 20 && idx < 0; tries++) {
        const cand =
          nWebcam > 0 && Math.random() < 0.6
            ? this.pool.seedCount + Math.floor(Math.random() * nWebcam)
            : Math.floor(Math.random() * this.pool.count);
        if (cand !== leaf.photoIdx) idx = cand;
      }
      if (idx >= 0) {
        const cells = [];
        for (let c = 0; c < map.assign.length; c++) {
          if (map.assign[c] === idx && isInterior(c)) cells.push(c);
        }
        if (cells.length === 0) {
          for (let c = 0; c < map.assign.length; c++) {
            if (map.assign[c] === idx) cells.push(c);
          }
        }
        if (cells.length > 0) {
          cell = cells[Math.floor(Math.random() * cells.length)];
        }
      }
      // Fallback (should not happen: every photo appears at least once).
      if (cell < 0) cell = Math.floor(Math.random() * map.assign.length);
    }

    const cx = cell % G;
    const cy = Math.floor(cell / G);
    const rect: Rect = {
      x: leaf.rect.x + (cx / G) * leaf.rect.size,
      y: leaf.rect.y + (cy / G) * leaf.rect.size,
      size: leaf.rect.size / G,
    };
    const child = this.makeLevel(map.assign[cell], rect, map.cellB[cell] / 255);
    this.levels.push(child);
    void this.buildMap(child);

    this.flight = {
      t: 0,
      dur: FLY_SECONDS,
      x0: this.cam.x, y0: this.cam.y, h0: this.cam.h,
      x1: rect.x + rect.size / 2,
      y1: rect.y + rect.size / 2,
      h1: rect.size * ARRIVE_FACTOR,
    };
    this.state = "fly";
  }

  // When the view is fully inside levels[1], make it the new root.
  private maybeRebase(): void {
    while (this.levels.length >= 2) {
      const r = this.levels[1].rect;
      const hx = this.cam.h * this.aspect;
      const inside =
        this.cam.x - hx >= r.x &&
        this.cam.x + hx <= r.x + r.size &&
        this.cam.y - this.cam.h >= r.y &&
        this.cam.y + this.cam.h <= r.y + r.size;
      if (!inside) break;

      const s = r.size;
      const tx = (v: number) => (v - r.x) / s;
      const ty = (v: number) => (v - r.y) / s;
      this.cam.x = tx(this.cam.x);
      this.cam.y = ty(this.cam.y);
      this.cam.h /= s;
      if (this.flight) {
        const f = this.flight;
        f.x0 = tx(f.x0); f.y0 = ty(f.y0); f.h0 /= s;
        f.x1 = tx(f.x1); f.y1 = ty(f.y1); f.h1 /= s;
      }
      const dropped = this.levels.shift()!;
      this.renderer.freeLevel(dropped);
      for (const level of this.levels) {
        level.rect = {
          x: (level.rect.x - r.x) / s,
          y: (level.rect.y - r.y) / s,
          size: level.rect.size / s,
        };
      }
    }
  }
}
