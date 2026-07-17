// Mosaic grid: G x G cells over a square photo. Cells vastly outnumber the
// photo pool, so every photo appears at least once and brightness-matching
// has room to reuse dark/bright photos freely.
export const G = 256;
export const CELLS = G * G; // 65536

export interface TileMap {
  assign: Int32Array; // CELLS photo indices
  cellRGB: Uint8Array; // CELLS*3 target average color (also the tint)
  // 1 = this cell is some photo's guaranteed ("home") appearance; 0 = free
  // dithered fill. Free cells are always safe to evict: their occupant has
  // a home elsewhere in this same mosaic.
  homeMask: Uint8Array;
}

export interface Rect {
  x: number;
  y: number;
  size: number;
}

export interface Camera {
  x: number;
  y: number;
  h: number; // vertical half-extent of the view, in root-space units
}

// A rendered square: either a chain level of the zoom, or a synthetic
// "detail" for a big neighbor tile — both drawn by the same code path.
export interface Level {
  photoIdx: number;
  rect: Rect; // position within current root space
  map: TileMap | null;
  vbo: WebGLBuffer | null;
  // Color of the parent-mosaic cell this level occupies. The flat overlay is
  // tinted with this (at the parent's tint weight) so the zoom target is
  // treated identically to every other tile.
  tint: [number, number, number]; // 0..1 each
}

export interface Knobs {
  // Global display tone curve ('c' key): linear stretch mapping the pool's
  // practical brightness range to full black..white, applied per channel.
  // Applied identically to every rendered pixel at every zoom level — tiles
  // and flat overlays alike — so a photo looks the same as a tile and as the
  // fullscreen zoom target.
  curveEnabled: boolean;
  curveLo: number; // 0..1 channel value mapped to 0
  curveHi: number; // 0..1 channel value mapped to 1
  // B&W mode ('b' key): desaturate every rendered pixel (Rec.709), applied
  // uniformly like the curve. Matching is unchanged — the same tiles, shown
  // as a black & white print.
  bw: boolean;
  tintEnabled: boolean; // master switch for tint blending ('t' key)
  tintLo: number; // tile px below which tint is full
  tintHi: number; // tile px above which tint is zero
  tintMax: number; // maximum tint weight
  flatLo: number; // level/viewport height fraction below which flat photo is opaque
  flatHi: number; // fraction above which flat photo is gone
}

export function smoothstep(lo: number, hi: number, x: number): number {
  const t = Math.min(1, Math.max(0, (x - lo) / (hi - lo)));
  return t * t * (3 - 2 * t);
}
