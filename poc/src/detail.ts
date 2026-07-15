import { G, Camera, Level, TileMap } from "./types";
import { Pool } from "./pool";
import { Matcher, cellBFromImage } from "./mosaic";
import { Renderer } from "./renderer";

// Neighbor detail: as the camera dives, tiles it merely passes get the same
// fidelity ladder as the zoom target — a sharp flat photo once they're big
// enough on screen, and their own mosaic once they approach viewport scale.
// Details are synthetic Level objects hanging off each chain level, rendered
// by the exact same code path as the chain: no special cases.

const MOSAIC_TIER_FRAC = 0.22; // cell height/viewport fraction to build its mosaic
const FLAT_TIER_FRAC = 0.04; // below this, the atlas tile alone suffices
const MAP_CACHE_CAP = 48;
const MAX_DETAILS_PER_LEVEL = 360;

interface CacheEntry {
  map: TileMap;
  vbo: WebGLBuffer;
  touch: number;
}

export class DetailManager {
  private mapCache = new Map<number, CacheEntry>();
  private pendingBuild = new Set<number>();
  private queue: number[] = [];
  private building = false;
  private frame = 0;

  constructor(
    private pool: Pool,
    private matcher: Matcher,
    private renderer: Renderer
  ) {}

  detailCount = 0;

  update(levels: Level[], cam: Camera, aspect: number): Level[][] {
    this.frame++;
    const out: Level[][] = levels.map(() => []);
    for (let i = 0; i < levels.length; i++) {
      const level = levels[i];
      if (!level.map) continue;
      const cellSize = level.rect.size / G;
      const tileFrac = cellSize / (2 * cam.h);
      if (tileFrac < FLAT_TIER_FRAC) continue;

      // Visible cell range of this level.
      const cx0 = Math.max(0, Math.floor((cam.x - cam.h * aspect - level.rect.x) / cellSize));
      const cx1 = Math.min(G - 1, Math.floor((cam.x + cam.h * aspect - level.rect.x) / cellSize));
      const cy0 = Math.max(0, Math.floor((cam.y - cam.h - level.rect.y) / cellSize));
      const cy1 = Math.min(G - 1, Math.floor((cam.y + cam.h - level.rect.y) / cellSize));
      if (cx0 > cx1 || cy0 > cy1) continue;

      // The next chain level renders its own cell — skip it here.
      let exclude = -1;
      const next = levels[i + 1];
      if (next) {
        const ex = Math.round((next.rect.x - level.rect.x) / cellSize);
        const ey = Math.round((next.rect.y - level.rect.y) / cellSize);
        if (ex >= 0 && ex < G && ey >= 0 && ey < G) exclude = ey * G + ex;
      }

      const wantMosaic = tileFrac >= MOSAIC_TIER_FRAC;
      let n = 0;
      for (let cy = cy0; cy <= cy1 && n < MAX_DETAILS_PER_LEVEL; cy++) {
        for (let cx = cx0; cx <= cx1 && n < MAX_DETAILS_PER_LEVEL; cx++) {
          const cell = cy * G + cx;
          if (cell === exclude) continue;
          const idx = level.map.assign[cell];
          const entry = this.mapCache.get(idx);
          if (entry) entry.touch = this.frame;
          else if (wantMosaic) this.requestBuild(idx);
          out[i].push({
            photoIdx: idx,
            rect: {
              x: level.rect.x + cx * cellSize,
              y: level.rect.y + cy * cellSize,
              size: cellSize,
            },
            map: entry ? entry.map : null,
            vbo: entry ? entry.vbo : null,
            tintB: level.map.cellB[cell] / 255,
          });
          n++;
        }
      }
    }
    this.detailCount = out.reduce((s, d) => s + d.length, 0);
    this.pump();
    return out;
  }

  private requestBuild(idx: number): void {
    if (this.pendingBuild.has(idx) || this.mapCache.has(idx)) return;
    this.pendingBuild.add(idx);
    this.queue.push(idx);
  }

  // One background build at a time; chain builds stay responsive.
  private pump(): void {
    if (this.building || this.queue.length === 0) return;
    this.building = true;
    const idx = this.queue.shift()!;
    void (async () => {
      try {
        const img = await this.pool.getPixels256(idx);
        const cellB = cellBFromImage(img);
        const { assign, homeMask } = await this.matcher.build(cellB);
        const map: TileMap = { assign, cellB, homeMask };
        const scratch: Level = {
          photoIdx: idx,
          rect: { x: 0, y: 0, size: 1 },
          map,
          vbo: null,
          tintB: 0,
        };
        this.renderer.buildLevelVBO(scratch);
        this.mapCache.set(idx, { map, vbo: scratch.vbo!, touch: this.frame });
        while (this.mapCache.size > MAP_CACHE_CAP) {
          let oldKey = -1;
          let oldTouch = Infinity;
          for (const [k, e] of this.mapCache) {
            if (e.touch < oldTouch) {
              oldTouch = e.touch;
              oldKey = k;
            }
          }
          this.renderer.freeBuffer(this.mapCache.get(oldKey)!.vbo);
          this.mapCache.delete(oldKey);
        }
      } catch (err) {
        console.warn("detail build failed", idx, err);
      } finally {
        this.pendingBuild.delete(idx);
        this.building = false;
      }
    })();
  }
}
