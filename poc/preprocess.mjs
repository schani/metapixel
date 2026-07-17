// Preprocess seed photos into tile atlases + matching descriptors.
//
// Output (public/assets/):
//   atlas<N>.jpg     4096x4096 color JPEG, 64x64 grid of 64px tiles
//   rgb.bin          count*3 bytes, per photo: average R, G, B
//   manifest.json    { tileSize, atlasSize, count, atlases, ids }

import sharp from "sharp";
import { readdir, writeFile, mkdir } from "node:fs/promises";
import path from "node:path";

const SEED_DIR = path.resolve(import.meta.dirname, "../seed-photos");
const OUT_DIR = path.resolve(import.meta.dirname, "public/assets");
const TILE = 64;
const ATLAS = 4096;
const PER_ROW = ATLAS / TILE; // 64
const PER_ATLAS = PER_ROW * PER_ROW; // 4096
const DESC_GRID = 4;

const files = (await readdir(SEED_DIR))
  .filter((f) => f.endsWith(".png"))
  .sort();
console.log(`${files.length} seed photos`);

const count = files.length;
const numAtlases = Math.ceil(count / PER_ATLAS);
const atlases = Array.from(
  { length: numAtlases },
  () => Buffer.alloc(ATLAS * ATLAS * 3)
);
const rgbs = Buffer.alloc(count * 3);

let done = 0;
async function processOne(i) {
  const file = path.join(SEED_DIR, files[i]);
  const rgb = await sharp(file)
    .resize(TILE, TILE, { fit: "cover" })
    .raw()
    .toBuffer();
  let sumR = 0, sumG = 0, sumB = 0;
  for (let p = 0; p < TILE * TILE; p++) {
    sumR += rgb[p * 3];
    sumG += rgb[p * 3 + 1];
    sumB += rgb[p * 3 + 2];
  }
  rgbs[i * 3] = Math.round(sumR / (TILE * TILE));
  rgbs[i * 3 + 1] = Math.round(sumG / (TILE * TILE));
  rgbs[i * 3 + 2] = Math.round(sumB / (TILE * TILE));

  const atlas = atlases[Math.floor(i / PER_ATLAS)];
  const slot = i % PER_ATLAS;
  const ox = (slot % PER_ROW) * TILE;
  const oy = Math.floor(slot / PER_ROW) * TILE;
  for (let y = 0; y < TILE; y++) {
    rgb.copy(
      atlas,
      ((oy + y) * ATLAS + ox) * 3,
      y * TILE * 3,
      (y + 1) * TILE * 3
    );
  }

  if (++done % 1000 === 0) console.log(`${done}/${count}`);
}

const CONCURRENCY = 32;
let next = 0;
await Promise.all(
  Array.from({ length: CONCURRENCY }, async () => {
    while (next < count) await processOne(next++);
  })
);

await mkdir(OUT_DIR, { recursive: true });
for (let a = 0; a < numAtlases; a++) {
  const out = path.join(OUT_DIR, `atlas${a}.jpg`);
  await sharp(atlases[a], { raw: { width: ATLAS, height: ATLAS, channels: 3 } })
    .jpeg({ quality: 88 })
    .toFile(out);
  console.log(`wrote ${out}`);
}
await writeFile(path.join(OUT_DIR, "rgb.bin"), rgbs);
await writeFile(
  path.join(OUT_DIR, "manifest.json"),
  JSON.stringify({
    tileSize: TILE,
    atlasSize: ATLAS,
    count,
    atlases: numAtlases,
    descGrid: DESC_GRID,
    ids: files.map((f) => f.replace(/\.png$/, "")),
  })
);
console.log("done");
