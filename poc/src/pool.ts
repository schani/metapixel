export const TILE = 64;
export const ATLAS_SIZE = 4096;
export const PER_ROW = ATLAS_SIZE / TILE; // 64
export const PER_ATLAS = PER_ROW * PER_ROW; // 4096
// RGBA8 is 4x the footprint of the old R8 atlas, so half the layers:
// 4 layers = ~340MB with mips, 16k photo capacity (15k seeds + headroom).
export const LAYERS = 4;
export const CAPACITY = PER_ATLAS * LAYERS; // 16384

interface Manifest {
  tileSize: number;
  atlasSize: number;
  count: number;
  atlases: number;
  ids: string[];
}

// The photo collection: color tile atlas array-texture on the GPU,
// average-RGB values on the CPU, plus per-photo "flat" (full) textures on
// demand.
export class Pool {
  gl: WebGL2RenderingContext;
  atlasTex: WebGLTexture;
  rgbs = new Uint8Array(CAPACITY * 3); // avg R,G,B per photo
  lumas = new Uint8Array(CAPACITY); // Rec.709 luma of the avg color
  count = 0;
  seedCount = 0; // photos below this index are seeds, above are webcam
  ids: string[] = [];
  private flatTexCache = new Map<number, WebGLTexture>(); // LRU via re-insertion
  private flatLoading = new Set<number>();
  private static readonly FLAT_CACHE_CAP = 512;
  private static readonly FLAT_LOAD_CAP = 16;
  private webcamPixels = new Map<number, ImageData>(); // 256px, for map builds
  private webcamFlat = new Map<number, HTMLCanvasElement>(); // 512px

  constructor(gl: WebGL2RenderingContext) {
    this.gl = gl;
    this.atlasTex = gl.createTexture()!;
  }

  private setAvg(idx: number, r: number, g: number, b: number): void {
    this.rgbs[idx * 3] = r;
    this.rgbs[idx * 3 + 1] = g;
    this.rgbs[idx * 3 + 2] = b;
    this.lumas[idx] = Math.round(0.2126 * r + 0.7152 * g + 0.0722 * b);
  }

  async init(): Promise<void> {
    const gl = this.gl;
    const manifest: Manifest = await (await fetch("/assets/manifest.json")).json();
    const rgbBuf = new Uint8Array(await (await fetch("/assets/rgb.bin")).arrayBuffer());
    this.rgbs.set(rgbBuf, 0);
    for (let i = 0; i < manifest.count; i++) {
      this.setAvg(i, rgbBuf[i * 3], rgbBuf[i * 3 + 1], rgbBuf[i * 3 + 2]);
    }
    this.count = this.seedCount = manifest.count;
    this.ids = manifest.ids;

    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.atlasTex);
    gl.texStorage3D(gl.TEXTURE_2D_ARRAY, 7, gl.RGBA8, ATLAS_SIZE, ATLAS_SIZE, LAYERS);

    const scratch = document.createElement("canvas");
    scratch.width = scratch.height = ATLAS_SIZE;
    const ctx = scratch.getContext("2d", { willReadFrequently: true })!;
    for (let i = 0; i < manifest.atlases; i++) {
      const blob = await (await fetch(`/assets/atlas${i}.jpg`)).blob();
      const bitmap = await createImageBitmap(blob, { colorSpaceConversion: "none" });
      ctx.drawImage(bitmap, 0, 0);
      bitmap.close();
      const rgba = ctx.getImageData(0, 0, ATLAS_SIZE, ATLAS_SIZE).data;
      gl.texSubImage3D(
        gl.TEXTURE_2D_ARRAY, 0, 0, 0, i,
        ATLAS_SIZE, ATLAS_SIZE, 1,
        gl.RGBA, gl.UNSIGNED_BYTE, rgba
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

  // Add a captured photo. canvas512: square hi-res; img256: pixels for
  // target analysis when this photo's own mosaic is built.
  addPhoto(canvas512: HTMLCanvasElement, img256: ImageData): number {
    if (this.count >= CAPACITY) throw new Error("pool full");
    const gl = this.gl;
    const idx = this.count++;

    let sumR = 0, sumG = 0, sumB = 0;
    for (let p = 0; p < 256 * 256; p++) {
      sumR += img256.data[p * 4];
      sumG += img256.data[p * 4 + 1];
      sumB += img256.data[p * 4 + 2];
    }
    const n = 256 * 256;
    this.setAvg(idx, Math.round(sumR / n), Math.round(sumG / n), Math.round(sumB / n));

    // Tile into the atlas.
    const tileCanvas = document.createElement("canvas");
    tileCanvas.width = tileCanvas.height = TILE;
    const tctx = tileCanvas.getContext("2d")!;
    tctx.drawImage(canvas512, 0, 0, TILE, TILE);
    const tile = tctx.getImageData(0, 0, TILE, TILE).data;
    const layer = Math.floor(idx / PER_ATLAS);
    const slot = idx % PER_ATLAS;
    gl.bindTexture(gl.TEXTURE_2D_ARRAY, this.atlasTex);
    gl.texSubImage3D(
      gl.TEXTURE_2D_ARRAY, 0,
      (slot % PER_ROW) * TILE, Math.floor(slot / PER_ROW) * TILE, layer,
      TILE, TILE, 1, gl.RGBA, gl.UNSIGNED_BYTE, tile
    );
    gl.generateMipmap(gl.TEXTURE_2D_ARRAY);

    this.webcamPixels.set(idx, img256);
    this.webcamFlat.set(idx, canvas512);
    this.ids.push(`webcam-${idx}`);
    return idx;
  }

  // 256px pixels of a photo (one per mosaic cell).
  async getPixels256(idx: number): Promise<ImageData> {
    const cached = this.webcamPixels.get(idx);
    if (cached) return cached;
    const bitmap = await this.fetchSeedBitmap(idx);
    const c = document.createElement("canvas");
    c.width = c.height = 256;
    const ctx = c.getContext("2d")!;
    ctx.drawImage(bitmap, 0, 0, 256, 256);
    return ctx.getImageData(0, 0, 256, 256);
  }

  // Full-photo texture for flat overlays. Call every frame you use it: a get
  // marks the texture recently-used, protecting it from LRU eviction.
  getFlatTex(idx: number): WebGLTexture | null {
    const cached = this.flatTexCache.get(idx);
    if (cached) {
      this.flatTexCache.delete(idx);
      this.flatTexCache.set(idx, cached);
      return cached;
    }
    if (!this.flatLoading.has(idx) && this.flatLoading.size < Pool.FLAT_LOAD_CAP) {
      this.flatLoading.add(idx);
      void this.loadFlatTex(idx).finally(() => this.flatLoading.delete(idx));
    }
    return null;
  }

  private async loadFlatTex(idx: number): Promise<WebGLTexture> {
    const gl = this.gl;
    let source: TexImageSource;
    if (this.isWebcam(idx)) {
      source = this.webcamFlat.get(idx)!;
    } else {
      source = await this.fetchSeedBitmap(idx);
    }
    const tex = gl.createTexture()!;
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source);
    gl.generateMipmap(gl.TEXTURE_2D);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    this.flatTexCache.set(idx, tex);
    while (this.flatTexCache.size > Pool.FLAT_CACHE_CAP) {
      const oldest = this.flatTexCache.keys().next().value!;
      gl.deleteTexture(this.flatTexCache.get(oldest)!);
      this.flatTexCache.delete(oldest);
    }
    return tex;
  }

  private async fetchSeedBitmap(idx: number): Promise<ImageBitmap> {
    const blob = await (await fetch(`/seed/${this.ids[idx]}.png`)).blob();
    return createImageBitmap(blob, { colorSpaceConversion: "none" });
  }
}
