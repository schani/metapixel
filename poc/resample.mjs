// Re-select the seed set from the full 70k FFHQ thumbnails, flattening the
// brightness histogram: keep everything from the sparse dark/bright tails,
// thin out the over-supplied midtones. Replaces ../seed-photos/.
//
// Usage: node resample.mjs <extracted-thumbs-dir>

import sharp from "sharp";
import { readdir, mkdir, copyFile, rm } from "node:fs/promises";
import path from "node:path";

const SRC = process.argv[2];
const DST = path.resolve(import.meta.dirname, "../seed-photos");
const TARGET = 15000;

const files = (await readdir(SRC)).filter((f) => f.endsWith(".png")).sort();
console.log(`${files.length} source photos`);

const lumas = new Float64Array(files.length);
let done = 0;
const CONCURRENCY = 32;
let next = 0;
await Promise.all(
  Array.from({ length: CONCURRENCY }, async () => {
    while (next < files.length) {
      const i = next++;
      const st = await sharp(path.join(SRC, files[i])).stats();
      const [r, g, b] = st.channels;
      lumas[i] = 0.2126 * r.mean + 0.7152 * g.mean + 0.0722 * b.mean;
      if (++done % 10000 === 0) console.log(`scanned ${done}/${files.length}`);
    }
  })
);

// 256 brightness bins.
const bins = Array.from({ length: 256 }, () => []);
for (let i = 0; i < files.length; i++) {
  bins[Math.min(255, Math.max(0, Math.round(lumas[i])))].push(i);
}

// Waterfill: find per-bin quota T such that sum(min(len, T)) >= TARGET.
let T = 0;
const total = (t) => bins.reduce((s, b) => s + Math.min(b.length, t), 0);
while (total(T) < TARGET) T++;
console.log(`per-bin quota: ${T} (yields ${total(T)})`);

// Take min(len, T) per bin (evenly strided within the bin), then trim the
// overshoot from the most-supplied bins.
let selected = [];
for (const bin of bins) {
  const take = Math.min(bin.length, T);
  for (let k = 0; k < take; k++) {
    selected.push(bin[Math.floor((k * bin.length) / take)]);
  }
}
selected = [...new Set(selected)];
while (selected.length > TARGET) selected.pop();
console.log(`selected ${selected.length}`);

// Report the resulting brightness distribution.
const hist = new Array(8).fill(0);
for (const i of selected) hist[Math.min(7, Math.floor(lumas[i] / 32))]++;
console.log(
  "brightness octiles (0=darkest):",
  hist.map((n, k) => `${k}:${n}`).join(" ")
);

await rm(DST, { recursive: true, force: true });
await mkdir(DST, { recursive: true });
let copied = 0;
next = 0;
const sel = selected.sort((a, b) => a - b);
await Promise.all(
  Array.from({ length: CONCURRENCY }, async () => {
    while (next < sel.length) {
      const i = sel[next++];
      await copyFile(path.join(SRC, files[i]), path.join(DST, files[i]));
      if (++copied % 5000 === 0) console.log(`copied ${copied}`);
    }
  })
);
await copyFile(path.join(SRC, "LICENSE.txt"), path.join(DST, "LICENSE.txt")).catch(() => {});
console.log(`done: ${sel.length} photos in ${DST}`);
