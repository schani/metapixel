import { Knobs } from "./types";
import { Pool, CAPACITY } from "./pool";
import { Matcher } from "./mosaic";
import { Renderer } from "./renderer";
import { Choreo } from "./choreo";
import { Capture } from "./capture";
import { DetailManager } from "./detail";

const canvas = document.getElementById("glcanvas") as HTMLCanvasElement;
const loading = document.getElementById("loading")!;
const hud = document.getElementById("hud")!;
const toast = document.getElementById("toast")!;
const video = document.getElementById("self") as HTMLVideoElement;
const countdown = document.getElementById("countdown")!;
const hint = document.getElementById("hint")!;

// Tint defaults to OFF: at 256x256 the Floyd–Steinberg dithering carries the
// image legibility that tint used to provide, and does it honestly.
const knobs: Knobs = {
  // Defaults from the measured pool brightness percentiles (p1=41, p99=193).
  curveEnabled: true,
  curveLo: 41 / 255,
  curveHi: 193 / 255,
  tintEnabled: false,
  tintLo: 48,
  tintHi: 280,
  tintMax: 0.75,
  flatLo: 0.35,
  flatHi: 0.85,
};

let hudVisible = true;
let paused = false;
// Supersampling factor: render internally above native resolution and let the
// browser downscale. Averaging samples per output pixel suppresses the moiré
// beat between the tile grid and the pixel grid. 's' toggles for A/B.
let superSample = 2;

async function boot() {
  const gl = canvas.getContext("webgl2")!;
  if (!gl) {
    loading.textContent = "WebGL2 not available";
    return;
  }

  function resize() {
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    canvas.width = Math.round(canvas.clientWidth * dpr);
    canvas.height = Math.round(canvas.clientHeight * dpr);
  }
  window.addEventListener("resize", resize);
  resize();

  loading.textContent = "loading faces…";
  const pool = new Pool(gl);
  await pool.init();
  const matcher = new Matcher(pool, CAPACITY);
  const renderer = new Renderer(gl, pool);
  const choreo = new Choreo(pool, matcher, renderer);
  const detail = new DetailManager(pool, matcher, renderer);

  loading.textContent = "building first mosaic…";
  const rootIdx = Math.floor(Math.random() * pool.seedCount);
  await choreo.start(rootIdx);
  loading.style.display = "none";

  const capture = new Capture(video, countdown);
  const haveCam = await capture.start();
  hint.textContent = haveCam
    ? "press SPACE to join the mosaic"
    : "no webcam — idle wander mode";
  capture.onPhoto = (c512, img256) => {
    const idx = pool.addPhoto(c512, img256);
    matcher.addPhoto(idx, pool.rgbs.subarray(idx * 3, idx * 3 + 3));
    choreo.onNewPhoto(idx);
    showToast("✓ you’re in — watch for yourself!");
  };

  // Toggles fire on keyup: a held key (or a synthetic key-repeat flood from a
  // test harness) produces many keydowns but exactly one keyup.
  window.addEventListener("keyup", (e) => {
    if (e.key === "f") {
      void document.documentElement.requestFullscreen();
    } else if (e.key === "h") {
      hudVisible = !hudVisible;
      hud.style.display = hudVisible ? "block" : "none";
    } else if (e.key === "t") {
      knobs.tintEnabled = !knobs.tintEnabled;
    } else if (e.key === "c") {
      knobs.curveEnabled = !knobs.curveEnabled;
    } else if (e.key === "p") {
      paused = !paused;
    } else if (e.key === "s") {
      superSample = (superSample % 3) + 1; // 1 -> 2 -> 3 -> 1
    }
  });

  window.addEventListener("keydown", (e) => {
    if (e.code === "Space") {
      e.preventDefault();
      void capture.snap();
    } else if (e.key === "5") knobs.curveLo = Math.max(0, knobs.curveLo - 0.02);
    else if (e.key === "6") knobs.curveLo = Math.min(knobs.curveHi - 0.05, knobs.curveLo + 0.02);
    else if (e.key === "7") knobs.curveHi = Math.max(knobs.curveLo + 0.05, knobs.curveHi - 0.02);
    else if (e.key === "8") knobs.curveHi = Math.min(1, knobs.curveHi + 0.02);
    else if (e.key === "1") knobs.tintMax = Math.max(0, knobs.tintMax - 0.05);
    else if (e.key === "2") knobs.tintMax = Math.min(1, knobs.tintMax + 0.05);
    else if (e.key === "3") knobs.tintHi = Math.max(60, knobs.tintHi - 20);
    else if (e.key === "4") knobs.tintHi += 20;
  });

  (window as any).__app = { pool, choreo, matcher }; // debug/verification hook

  let lastT = performance.now();
  let frames = 0;
  let fps = 0;
  let fpsT = lastT;
  function frame(t: number) {
    const dt = Math.min(0.1, (t - lastT) / 1000);
    lastT = t;
    frames++;
    if (t - fpsT > 1000) {
      fps = (frames * 1000) / (t - fpsT);
      frames = 0;
      fpsT = t;
    }
    if (!paused) choreo.update(dt, canvas.width / canvas.height);
    const details = detail.update(
      choreo.levels,
      choreo.cam,
      canvas.width / canvas.height
    );
    renderer.draw(choreo.levels, choreo.cam, knobs, details, superSample);
    if (hudVisible) {
      hud.textContent =
        `fps ${fps.toFixed(0)}  pool ${pool.count}  ` +
        `levels ${choreo.levels.length}  queue ${choreo.queue.length}  ` +
        `h ${choreo.cam.h.toExponential(2)}  ${choreo.state}  det ${detail.detailCount}  ` +
        `tint ${knobs.tintEnabled ? "on" : "OFF"} ` +
        `curve ${knobs.curveEnabled ? `${Math.round(knobs.curveLo * 255)}..${Math.round(knobs.curveHi * 255)}` : "OFF"}  ` +
        `ss ${superSample}x` +
        (paused ? "  PAUSED" : "");
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}

function showToast(text: string) {
  toast.textContent = text;
  toast.classList.add("show");
  setTimeout(() => toast.classList.remove("show"), 4000);
}

void boot();
