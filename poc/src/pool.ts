export const TILE = 64;
export const ATLAS_SIZE = 4096;
export const PER_ROW = ATLAS_SIZE / TILE; // 64
export const PER_ATLAS = PER_ROW * PER_ROW; // 4096
export const LAYERS = 8; // R8 is cheap: 8 layers = 134MB, 32k photo capacity
export const CAPACITY = PER_ATLAS * LAYERS; // 32768

interface Manifest {
  tileSize: number;
  atlasSize: number;
  count: number;
  atlases: number;
  ids: string[];
}

// The photo collection: single-channel (luminance) tile atlas array-texture
// on the GPU, average-brightness values on the CPU, plus per-photo "flat"
// (full) textures on demand. Everything is B&W.
export class Pool {
  gl: WebGL2RenderingContext;
  atlasTex: WebGLTexture;
  lumas = new Uint8Array(CAPACITY);
  count = 0;
  seedCount = 0; // photos below this index are seeds, above are webcam
  ids: string[] = [];
  private flatTexCache = new Map<number, WebGLTexture>();
  private flatLoading = new Map<number, Promise<WebGLTexture>>();
  private webcamPixels = new Map<number, ImageData>(); // 256px, for map builds
  private webcamFlat = new Map<number, HTMLCanvasElement>(); // 512px

  constructor(gl: WebGL2RenderingContext) {
    this.gl = gl;
    this.atlasTex = gl.createTexture()!;
  }

  async init(): Promise<void> {
    const gl = this.gl;
    const manifest: Manifest = await (await fetch("/assets/manifest.json")).json();
    const lumaBuf = await (await fetch("/assets/luma.bin")).arrayBuffer();
    this.lumas.set(new Uint8Array(lumaBuf), 0);
    this.count = this.seedCount = manifest.count;
    this.ids = manifest.ids;

    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.atlasTex);
    gl.texStorage3D(gl.TEXTURE_2D_ARRAY, 7, gl.R8, ATLAS_SIZE, ATLAS_SIZE, LAYERS);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);

    const scratch = document.createElement("canvas");
    scratch.width = scratch.height = ATLAS_SIZE;
    const ctx = scratch.getContext("2d", { willReadFrequently: true })!;
    for (let i = 0; i < manifest.atlases; i++) {
      const blob = await (await fetch(`/assets/atlas${i}.jpg`)).blob();
      const bitmap = await createImageBitmap(blob, { colorSpaceConversion: "none" });
      ctx.drawImage(bitmap, 0, 0);
      bitmap.close();
      const rgba = ctx.getImageData(0, 0, ATLAS_SIZE, ATLAS_SIZE).data;
      const red = new Uint8Array(ATLAS_SIZE * ATLAS_SIZE);
      for (let p = 0; p < red.length; p++) red[p] = rgba[p * 4];
      gl.texSubImage3D(
        gl.TEXTURE_2D_ARRAY, 0, 0, 0, i,
        ATLAS_SIZE, ATLAS_SIZE, 1,
        gl.RED, gl.UNSIGNED_BYTE, red
      );
    }
    gl.generateMipmap(gl.TEXTURE_2D_ARRAY);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  }

  isWebcam(idx: number): boolean {
    return idx >= this.seedCount;
  }

  // Add a captured photo. canvas512: square grayscale hi-res; img256: pixels
  // for target analysis when this photo's own mosaic is built.
  addPhoto(canvas512: HTMLCanvasElement, img256: ImageData): number {
    if (this.count >= CAPACITY) throw new Error("pool full");
    const gl = this.gl;
    const idx = this.count++;

    let sum = 0;
    for (let p = 0; p < 256 * 256; p++) sum += img256.data[p * 4];
    this.lumas[idx] = Math.round(sum / (256 * 256));

    // Tile into the atlas.
    const tileCanvas = document.createElement("canvas");
    tileCanvas.width = tileCanvas.height = TILE;
    const tctx = tileCanvas.getContext("2d")!;
    tctx.drawImage(canvas512, 0, 0, TILE, TILE);
    const tile = tctx.getImageData(0, 0, TILE, TILE).data;
    const red = new Uint8Array(TILE * TILE);
    for (let p = 0; p < red.length; p++) red[p] = tile[p * 4];
    const layer = Math.floor(idx / PER_ATLAS);
    const slot = idx % PER_ATLAS;
    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.atlasTex);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texSubImage3D(
      gl.TEXTURE_2D_ARRAY, 0,
      (slot % PER_ROW) * TILE, Math.floor(slot / PER_ROW) * TILE, layer,
      TILE, TILE, 1, gl.RED, gl.UNSIGNED_BYTE, red
    );
    gl.generateMipmap(gl.TEXTURE_2D_ARRAY);

    this.webcamPixels.set(idx, img256);
    this.webcamFlat.set(idx, canvas512);
    this.ids.push(`webcam-${idx}`);
    return idx;
  }

  // 256px grayscale pixels of a photo (one per mosaic cell).
  async getPixels256(idx: number): Promise<ImageData> {
    const cached = this.webcamPixels.get(idx);
    if (cached) return cached;
    const bitmap = await this.fetchSeedBitmap(idx);
    const c = document.createElement("canvas");
    c.width = c.height = 256;
    const ctx = c.getContext("2d")!;
    ctx.filter = "grayscale(1)";
    ctx.drawImage(bitmap, 0, 0, 256, 256);
    return ctx.getImageData(0, 0, 256, 256);
  }

  // Full-photo texture for the flat overlay during zoom handoff.
  getFlatTex(idx: number): WebGLTexture | null {
    const cached = this.flatTexCache.get(idx);
    if (cached) return cached;
    if (!this.flatLoading.has(idx)) {
      this.flatLoading.set(idx, this.loadFlatTex(idx));
    }
    return null;
  }

  private async loadFlatTex(idx: number): Promise<WebGLTexture> {
    const gl = this.gl;
    let source: TexImageSource;
    if (this.isWebcam(idx)) {
      source = this.webcamFlat.get(idx)!; // already grayscale from capture
    } else {
      const bitmap = await this.fetchSeedBitmap(idx);
      const c = document.createElement("canvas");
      c.width = bitmap.width;
      c.height = bitmap.height;
      const ctx = c.getContext("2d")!;
      ctx.filter = "grayscale(1)";
      ctx.drawImage(bitmap, 0, 0);
      source = c;
    }
    const tex = gl.createTexture()!;
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source);
    gl.generateMipmap(gl.TEXTURE_2D);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    this.flatTexCache.set(idx, tex);
    return tex;
  }

  private async fetchSeedBitmap(idx: number): Promise<ImageBitmap> {
    const blob = await (await fetch(`/seed/${this.ids[idx]}.png`)).blob();
    return createImageBitmap(blob, { colorSpaceConversion: "none" });
  }
}
