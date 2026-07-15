# metapixel live

An interactive photomosaic art installation: visitors are photographed with a
webcam, and a big screen shows an endless zoom through recursive B&W
photomosaics — every photo is a mosaic of all the other photos, and the camera
dives from face to face forever. Freshly captured visitors are woven into
every mosaic within seconds and the camera flies to them.

This is a browser-only proof of concept (Vite + TypeScript + WebGL2, no
backend). Spiritual successor to metapixel, the classic C photomosaic
generator in the repository root.

## Design principles

**Nothing is faked.** Every mosaic genuinely contains every photo in the pool
at least once (the grid has 65,536 cells, comfortably more than the pool), so
any photo can always be the next zoom target. Zoom targets get no special
visual treatment — the tile you fly into is color-treated identically to its
neighbors and simply happens to fill the screen. New captures claim a
"free" cell whose occupant provably appears elsewhere in the same mosaic, so
nobody's last appearance is ever evicted.

**The mosaic is a halftone print.** The installation is black & white. Tiles
are matched on average brightness only (at 4–8 px on screen, within-tile
detail is invisible) and the whole image is treated as a dithering problem:

- **Phase A — histogram fitting**: decide *which* 65,536 tiles to use. The
  mandatory mass (every pool photo once) is fixed; the free budget fills the
  target histogram's deficits at the nearest achievable brightness. The
  1-D earth-mover's distance between the result and the target is logged as
  "average brightness error per tile" (~2–3 for typical photos, ~25+ for
  extreme ones — the irreducible cost of the everyone-appears constraint).
- **Phase B — jittered rank matching**: decide *where* they go. Sort cells by
  target brightness plus ±10 random jitter, sort the tile multiset by luma,
  match rank to rank. This is the exact 1-D optimal-transport solution:
  monotone, so brightness "crossings" (a swap that would improve the image)
  are impossible. The jitter turns contours at pool-supply gaps into dither
  noise and scatters surplus tiles uniformly within flat regions.

  (Historical note: three generations of error-diffusion placement — greedy
  with repair, scattered homes + serpentine Floyd–Steinberg, random-order
  diffusion — all produced concentration artifacts of one kind or another.
  Greedy online placement provably cannot avoid crossings; rank matching
  ends that line of bugs by construction.)

**Uniform display transform.** A global tone curve ('c') stretches the pool's
practical brightness range (p1..p99 ≈ 41..193) to full black..white,
identically for every pixel at every zoom level — the "print medium" for the
whole show. Optional tint blending ('t', off by default) is likewise applied
uniformly, never per-target.

## Architecture

| module | role |
| --- | --- |
| `preprocess.mjs` | seed photos → grayscale 4096² JPEG atlases (64px tiles) + `luma.bin` (1 byte avg brightness per photo) |
| `resample.mjs` | select seeds from the full FFHQ set by brightness-histogram flattening (maximizes dark/bright tails) |
| `src/pool.ts` | photo collection: R8 array-texture atlas (8 layers = 32,768 capacity), brightness values, flat textures, webcam additions |
| `src/matcher.worker.ts` | phase A + phase B assignment in a Web Worker (~5–20 ms per mosaic) |
| `src/mosaic.ts` | target analysis (256px, 1px per cell), worker wrapper, live insertion |
| `src/renderer.ts` | instanced WebGL2 tile renderer, flat-photo overlays for zoom handoff, tone curve |
| `src/detail.ts` | neighbor fidelity: tiles the camera zooms past get sharp flats (≥4% of viewport) and their own mosaics (≥22%), by the same rules as the target; LRU-cached maps/VBOs |
| `src/choreo.ts` | endless-zoom state machine: target picking (uniform over photos, not tiles), camera flights, coordinate rebasing (≤3 live levels) |
| `src/capture.ts` | webcam self-view, countdown, mirrored square grayscale capture |
| `src/main.ts` | boot, keyboard, HUD |

Key invariant: zoom targets must be *interior* cells (2-cell border margin),
otherwise the widescreen camera view never fits inside the parent level and
coordinate rebasing starves.

## Setup

Photos and generated assets are not in git. To bootstrap:

```sh
cd poc
npm install

# 1. Get the FFHQ 128px thumbnails (~2.1 GB, 70k faces, CC-licensed):
#    https://huggingface.co/datasets/nuwandaa/ffhq128 (thumbnails128x128.zip)
#    Unzip somewhere, then select 15k seeds with flattened brightness:
node resample.mjs /path/to/thumbnails128x128     # writes ../seed-photos/

# 2. Build atlases + brightness data:
npm run preprocess                                # writes public/assets/

# 3. Serve the seed photos and run:
ln -s ../../seed-photos public/seed
npm run dev
```

FFHQ licensing: individual images are under permissive CC/public-domain
licenses; the dataset is CC BY-NC-SA 4.0 — fine for a non-commercial
installation, with attribution (see `seed-photos/LICENSE.txt`).

## Controls

| key | action |
| --- | --- |
| `space` | countdown + capture — you join every mosaic, camera flies to you |
| `p` | pause/resume the zoom |
| `c` | toggle global tone curve; `5`/`6` black point, `7`/`8` white point |
| `t` | toggle tint blending; `1`/`2` strength, `3`/`4` fade distance |
| `h` | toggle HUD |
| `f` | fullscreen |

## Known limits / next steps

- Extreme-brightness targets carry irreducible error (~25 avg) rendered as a
  smooth tonal lift. The planned fix is exposure-variant prints (−2/−1/+1 EV
  copies as first-class pool entries) — this requires deciding that *any
  print* of a photo satisfies its presence constraint, else variants make
  the constraint heavier instead of lighter.
- Seed photos are 128px: soft when a tile grows toward fullscreen. The FFHQ
  ids are preserved so the 1024px originals (or a 512px re-encode) can be
  fetched to make flats genuinely crisp.
- Single machine, webcam capture only; phone capture + a small ingest server
  is the natural production split (the architecture doesn't change).
