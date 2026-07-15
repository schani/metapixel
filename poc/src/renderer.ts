import { G, CELLS, Camera, Knobs, Level, smoothstep } from "./types";
import { createProgram } from "./gl";
import { Pool, PER_ROW } from "./pool";

const MOSAIC_VS = `#version 300 es
layout(location=0) in vec2 aCorner;
layout(location=1) in vec4 aCell;  // cellX, cellY, photoIdx, tintB
uniform vec3 uRect;                // x, y, size in root space
uniform vec2 uCam;
uniform vec2 uHalf;                // h*aspect, h
out vec2 vUV;
flat out int vLayer;
out float vTintB;
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
  vTintB = aCell.w;
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
in float vTintB;
out vec4 outColor;
void main() {
  float l = texture(uAtlas, vec3(vUV, float(vLayer))).r;
  l = mix(l, vTintB, uTintW);
  if (uCurve.x >= 0.0) l = clamp((l - uCurve.x) / (uCurve.y - uCurve.x), 0.0, 1.0);
  outColor = vec4(vec3(l), uAlpha);
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

const FLAT_FS = `#version 300 es
precision mediump float;
uniform sampler2D uTex;
uniform float uAlpha;
uniform float uTintB;
uniform float uTintW;
uniform vec2 uCurve; // lo, hi; lo<0 disables
in vec2 vUV;
out vec4 outColor;
void main() {
  float l = mix(texture(uTex, vUV).r, uTintB, uTintW);
  if (uCurve.x >= 0.0) l = clamp((l - uCurve.x) / (uCurve.y - uCurve.x), 0.0, 1.0);
  outColor = vec4(vec3(l), uAlpha);
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

  constructor(gl: WebGL2RenderingContext, pool: Pool) {
    this.gl = gl;
    this.pool = pool;
    this.mosaicProg = createProgram(gl, MOSAIC_VS, MOSAIC_FS);
    this.flatProg = createProgram(gl, FLAT_VS, FLAT_FS);
    for (const n of ["uRect", "uCam", "uHalf", "uAtlas", "uTintW", "uAlpha", "uCurve"]) {
      this.uni[n] = gl.getUniformLocation(this.mosaicProg, n)!;
    }
    for (const n of ["uRect", "uCam", "uHalf", "uTex", "uAlpha", "uTintB", "uTintW", "uCurve"]) {
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
    const data = new Float32Array(CELLS * 4);
    for (let c = 0; c < CELLS; c++) {
      const o = c * 4;
      data[o] = c % G;
      data[o + 1] = Math.floor(c / G);
      data[o + 2] = map.assign[c];
      data[o + 3] = map.cellB[c] / 255;
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

  draw(
    levels: Level[],
    cam: Camera,
    knobs: Knobs,
    details?: Level[][]
  ): void {
    const gl = this.gl;
    const W = gl.drawingBufferWidth;
    const H = gl.drawingBufferHeight;
    gl.viewport(0, 0, W, H);
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
    gl.bindVertexArray(null);
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
      gl.vertexAttribPointer(1, 4, gl.FLOAT, false, 16, 0);
      gl.vertexAttribDivisor(1, 1);
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
        gl.uniform1f(this.funi.uTintB, level.tintB);
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
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      }
    }
    return tintW;
  }
}
