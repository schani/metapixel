import { G, CELLS, Camera, Knobs, Level, smoothstep } from "./types";
import { createProgram } from "./gl";
import { Pool, PER_ROW } from "./pool";

const MOSAIC_VS = `#version 300 es
layout(location=0) in vec2 aCorner;
layout(location=1) in vec3 aCell;  // cellX, cellY, photoIdx
layout(location=2) in vec3 aTint;  // cell target color
uniform vec3 uRect;                // x, y, size in root space
uniform vec2 uCam;
uniform vec2 uHalf;                // h*aspect, h
out vec2 vUV;
flat out int vLayer;
out vec3 vTint;
void main() {
  float grid = float(${G});
  vec2 world = uRect.xy + (aCell.xy + aCorner) / grid * uRect.z;
  vec2 ndc = (world - uCam) / uHalf * vec2(1.0, -1.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
  int idx = int(aCell.z + 0.5);
  vLayer = idx >> 12;
  int slot = idx & 4095;
  vec2 slotXY = vec2(float(slot & 63), float(slot >> 6));
  float perRow = float(${PER_ROW});
  vec2 inner = aCorner * (1.0 - 1.0 / 64.0) + 0.5 / 64.0;
  vUV = (slotXY + inner) / perRow;
  vTint = aTint;
}`;

const MOSAIC_FS = `#version 300 es
precision mediump float;
precision mediump sampler2DArray;
uniform sampler2DArray uAtlas;
uniform float uTintW;
uniform float uAlpha;
uniform vec2 uCurve; // lo, hi; lo<0 disables
in vec2 vUV;
flat in int vLayer;
in vec3 vTint;
out vec4 outColor;
void main() {
  vec3 col = texture(uAtlas, vec3(vUV, float(vLayer))).rgb;
  col = mix(col, vTint, uTintW);
  if (uCurve.x >= 0.0) col = clamp((col - uCurve.x) / (uCurve.y - uCurve.x), 0.0, 1.0);
  outColor = vec4(col, uAlpha);
}`;

const FLAT_VS = `#version 300 es
layout(location=0) in vec2 aCorner;
uniform vec3 uRect;
uniform vec2 uCam;
uniform vec2 uHalf;
out vec2 vUV;
void main() {
  vec2 world = uRect.xy + aCorner * uRect.z;
  vec2 ndc = (world - uCam) / uHalf * vec2(1.0, -1.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
  vUV = aCorner;
}`;

// Exact NxN box downsample from the supersampled framebuffer. The browser's
// own canvas downscale is bilinear (2x2 taps), which skips samples at 3:1
// and re-introduces beat patterns; this averages every covered sample.
const DOWN_VS = `#version 300 es
layout(location=0) in vec2 aCorner;
void main() {
  gl_Position = vec4(aCorner * 2.0 - 1.0, 0.0, 1.0);
}`;

const DOWN_FS = `#version 300 es
precision highp float;
uniform sampler2D uTex;
uniform int uSS;
out vec4 outColor;
void main() {
  ivec2 base = ivec2(gl_FragCoord.xy) * uSS;
  vec3 acc = vec3(0.0);
  for (int y = 0; y < uSS; y++) {
    for (int x = 0; x < uSS; x++) {
      acc += texelFetch(uTex, base + ivec2(x, y), 0).rgb;
    }
  }
  outColor = vec4(acc / float(uSS * uSS), 1.0);
}`;

const FLAT_FS = `#version 300 es
precision mediump float;
uniform sampler2D uTex;
uniform float uAlpha;
uniform vec3 uTint;
uniform float uTintW;
uniform vec2 uCurve; // lo, hi; lo<0 disables
in vec2 vUV;
out vec4 outColor;
void main() {
  vec3 col = mix(texture(uTex, vUV).rgb, uTint, uTintW);
  if (uCurve.x >= 0.0) col = clamp((col - uCurve.x) / (uCurve.y - uCurve.x), 0.0, 1.0);
  outColor = vec4(col, uAlpha);
}`;

export class Renderer {
  gl: WebGL2RenderingContext;
  pool: Pool;
  private mosaicProg: WebGLProgram;
  private flatProg: WebGLProgram;
  private quadBuf: WebGLBuffer;
  private vao: WebGLVertexArrayObject;
  private uni: Record<string, WebGLUniformLocation> = {};
  private funi: Record<string, WebGLUniformLocation> = {};
  private downProg: WebGLProgram;
  private downUni: { uTex: WebGLUniformLocation; uSS: WebGLUniformLocation };
  private fbo: WebGLFramebuffer | null = null;
  private fboTex: WebGLTexture | null = null;
  private fboW = 0;
  private fboH = 0;

  constructor(gl: WebGL2RenderingContext, pool: Pool) {
    this.gl = gl;
    this.pool = pool;
    this.mosaicProg = createProgram(gl, MOSAIC_VS, MOSAIC_FS);
    this.flatProg = createProgram(gl, FLAT_VS, FLAT_FS);
    this.downProg = createProgram(gl, DOWN_VS, DOWN_FS);
    this.downUni = {
      uTex: gl.getUniformLocation(this.downProg, "uTex")!,
      uSS: gl.getUniformLocation(this.downProg, "uSS")!,
    };
    for (const n of ["uRect", "uCam", "uHalf", "uAtlas", "uTintW", "uAlpha", "uCurve"]) {
      this.uni[n] = gl.getUniformLocation(this.mosaicProg, n)!;
    }
    for (const n of ["uRect", "uCam", "uHalf", "uTex", "uAlpha", "uTint", "uTintW", "uCurve"]) {
      this.funi[n] = gl.getUniformLocation(this.flatProg, n)!;
    }
    this.quadBuf = gl.createBuffer()!;
    gl.bindBuffer(gl.ARRAY_BUFFER, this.quadBuf);
    gl.bufferData(
      gl.ARRAY_BUFFER,
      new Float32Array([0, 0, 1, 0, 0, 1, 1, 1]),
      gl.STATIC_DRAW
    );
    this.vao = gl.createVertexArray()!;
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
  }

  // (Re)build a level's per-instance buffer from its tile map.
  buildLevelVBO(level: Level): void {
    const gl = this.gl;
    const map = level.map!;
    const data = new Float32Array(CELLS * 6);
    for (let c = 0; c < CELLS; c++) {
      const o = c * 6;
      data[o] = c % G;
      data[o + 1] = Math.floor(c / G);
      data[o + 2] = map.assign[c];
      data[o + 3] = map.cellRGB[c * 3] / 255;
      data[o + 4] = map.cellRGB[c * 3 + 1] / 255;
      data[o + 5] = map.cellRGB[c * 3 + 2] / 255;
    }
    if (!level.vbo) level.vbo = gl.createBuffer()!;
    gl.bindBuffer(gl.ARRAY_BUFFER, level.vbo);
    gl.bufferData(gl.ARRAY_BUFFER, data, gl.DYNAMIC_DRAW);
  }

  freeLevel(level: Level): void {
    if (level.vbo) {
      this.gl.deleteBuffer(level.vbo);
      level.vbo = null;
    }
  }

  freeBuffer(vbo: WebGLBuffer): void {
    this.gl.deleteBuffer(vbo);
  }

  // Cap the supersampled framebuffer so Retina fullscreen doesn't explode.
  private static readonly MAX_FBO_PIXELS = 28_000_000;

  draw(
    levels: Level[],
    cam: Camera,
    knobs: Knobs,
    details?: Level[][],
    superSample = 1
  ): void {
    const gl = this.gl;
    const W = gl.drawingBufferWidth;
    const H = gl.drawingBufferHeight;
    let ss = Math.max(1, Math.floor(superSample));
    while (ss > 1 && W * ss * H * ss > Renderer.MAX_FBO_PIXELS) ss--;

    if (ss > 1) {
      this.ensureFBO(W * ss, H * ss);
      gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
      gl.viewport(0, 0, this.fboW, this.fboH);
    } else {
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.viewport(0, 0, W, H);
    }
    gl.clearColor(0.02, 0.02, 0.03, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    const halfX = cam.h * (W / H);
    const curveLo = knobs.curveEnabled ? knobs.curveLo : -1;

    gl.bindVertexArray(this.vao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.quadBuf);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);

    // Tint weight of the previous (parent) level's mosaic — flat overlays are
    // tinted at their parent's weight, exactly like the tiles they cover.
    // Size heuristics (tilePx) use logical pixels (H), not FBO pixels.
    let parentTintW = 0;
    for (let i = 0; i < levels.length; i++) {
      const tintW = this.drawLevel(levels[i], cam, knobs, parentTintW, H, halfX, curveLo);
      const dts = details?.[i];
      if (dts) {
        for (const d of dts) {
          this.drawLevel(d, cam, knobs, tintW, H, halfX, curveLo);
        }
      }
      parentTintW = tintW;
    }

    if (ss > 1) {
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.viewport(0, 0, W, H);
      gl.useProgram(this.downProg);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, this.fboTex);
      gl.uniform1i(this.downUni.uTex, 0);
      gl.uniform1i(this.downUni.uSS, ss);
      gl.disable(gl.BLEND);
      gl.bindBuffer(gl.ARRAY_BUFFER, this.quadBuf);
      gl.enableVertexAttribArray(0);
      gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
      gl.vertexAttribDivisor(1, 0);
      gl.disableVertexAttribArray(1);
      gl.vertexAttribDivisor(2, 0);
      gl.disableVertexAttribArray(2);
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      gl.enable(gl.BLEND);
    }
    gl.bindVertexArray(null);
  }

  private ensureFBO(w: number, h: number): void {
    const gl = this.gl;
    if (this.fbo && this.fboW === w && this.fboH === h) return;
    if (this.fboTex) gl.deleteTexture(this.fboTex);
    if (this.fbo) gl.deleteFramebuffer(this.fbo);
    this.fboTex = gl.createTexture()!;
    gl.bindTexture(gl.TEXTURE_2D, this.fboTex);
    gl.texStorage2D(gl.TEXTURE_2D, 1, gl.RGBA8, w, h);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    this.fbo = gl.createFramebuffer()!;
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
    gl.framebufferTexture2D(
      gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.fboTex, 0
    );
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    this.fboW = w;
    this.fboH = h;
  }

  // Draw one square (chain level or neighbor detail): mosaic if available,
  // then the flat photo overlay. Returns the square's own mosaic tint weight.
  private drawLevel(
    level: Level,
    cam: Camera,
    knobs: Knobs,
    parentTintW: number,
    H: number,
    halfX: number,
    curveLo: number
  ): number {
    const gl = this.gl;
    // Vertical screen px per tile, and level height as viewport fraction.
    const tilePx = ((level.rect.size / G) * H) / (2 * cam.h);
    const frac = level.rect.size / (2 * cam.h);
    const tintW = knobs.tintEnabled
      ? knobs.tintMax * (1 - smoothstep(knobs.tintLo, knobs.tintHi, tilePx))
      : 0;

    if (level.map && level.vbo && tilePx > 0.5) {
      gl.useProgram(this.mosaicProg);
      gl.uniform3f(this.uni.uRect, level.rect.x, level.rect.y, level.rect.size);
      gl.uniform2f(this.uni.uCam, cam.x, cam.y);
      gl.uniform2f(this.uni.uHalf, halfX, cam.h);
      gl.uniform1f(this.uni.uTintW, tintW);
      gl.uniform1f(this.uni.uAlpha, 1);
      gl.uniform2f(this.uni.uCurve, curveLo, knobs.curveHi);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.pool.atlasTex);
      gl.uniform1i(this.uni.uAtlas, 0);
      gl.bindBuffer(gl.ARRAY_BUFFER, level.vbo);
      gl.enableVertexAttribArray(1);
      gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 24, 0);
      gl.vertexAttribDivisor(1, 1);
      gl.enableVertexAttribArray(2);
      gl.vertexAttribPointer(2, 3, gl.FLOAT, false, 24, 12);
      gl.vertexAttribDivisor(2, 1);
      gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, CELLS);
    }

    // Flat photo overlay: fades in once the square is big enough on screen
    // that the atlas tile goes soft (the same rule for zoom targets and
    // neighbors alike), and dissolves into the mosaic near fullscreen.
    const flatAlpha =
      smoothstep(0.04, 0.075, frac) *
      (1 - smoothstep(knobs.flatLo, knobs.flatHi, frac));
    if (flatAlpha > 0.01) {
      const flatTex = this.pool.getFlatTex(level.photoIdx);
      if (flatTex) {
        gl.useProgram(this.flatProg);
        gl.uniform3f(this.funi.uRect, level.rect.x, level.rect.y, level.rect.size);
        gl.uniform2f(this.funi.uCam, cam.x, cam.y);
        gl.uniform2f(this.funi.uHalf, halfX, cam.h);
        gl.uniform1f(this.funi.uAlpha, flatAlpha);
        gl.uniform3f(this.funi.uTint, level.tint[0], level.tint[1], level.tint[2]);
        gl.uniform1f(this.funi.uTintW, parentTintW);
        gl.uniform2f(this.funi.uCurve, curveLo, knobs.curveHi);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, flatTex);
        gl.uniform1i(this.funi.uTex, 0);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.quadBuf);
        gl.enableVertexAttribArray(0);
        gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
        gl.vertexAttribDivisor(1, 0);
        gl.disableVertexAttribArray(1);
        gl.vertexAttribDivisor(2, 0);
        gl.disableVertexAttribArray(2);
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      }
    }
    return tintW;
  }
}
