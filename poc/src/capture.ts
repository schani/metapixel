// Webcam capture: self-view video, spacebar → countdown → square snap.
export class Capture {
  private video: HTMLVideoElement;
  private countdownEl: HTMLElement;
  private busy = false;
  onPhoto: (canvas512: HTMLCanvasElement, img256: ImageData) => void = () => {};

  constructor(video: HTMLVideoElement, countdownEl: HTMLElement) {
    this.video = video;
    this.countdownEl = countdownEl;
  }

  async start(): Promise<boolean> {
    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        video: { width: { ideal: 1280 }, height: { ideal: 720 } },
        audio: false,
      });
      this.video.srcObject = stream;
      await this.video.play();
      return true;
    } catch (err) {
      console.warn("webcam unavailable:", err);
      return false;
    }
  }

  get ready(): boolean {
    return this.video.readyState >= 2 && !this.busy;
  }

  async snap(): Promise<void> {
    if (!this.ready) return;
    this.busy = true;
    for (const n of ["3", "2", "1"]) {
      this.countdownEl.textContent = n;
      this.countdownEl.classList.add("show");
      await sleep(700);
      this.countdownEl.classList.remove("show");
      await sleep(50);
    }
    // Flash.
    this.countdownEl.textContent = "";
    document.body.classList.add("flash");
    setTimeout(() => document.body.classList.remove("flash"), 180);

    // Center-square crop, mirrored (people expect their mirror image).
    const vw = this.video.videoWidth;
    const vh = this.video.videoHeight;
    const s = Math.min(vw, vh);
    const sx = (vw - s) / 2;
    const sy = (vh - s) / 2;
    const c512 = document.createElement("canvas");
    c512.width = c512.height = 512;
    const ctx = c512.getContext("2d")!;
    ctx.filter = "grayscale(1)"; // the installation is B&W
    ctx.translate(512, 0);
    ctx.scale(-1, 1);
    ctx.drawImage(this.video, sx, sy, s, s, 0, 0, 512, 512);

    const c256 = document.createElement("canvas");
    c256.width = c256.height = 256;
    const ctx256 = c256.getContext("2d")!;
    ctx256.drawImage(c512, 0, 0, 256, 256);
    const img256 = ctx256.getImageData(0, 0, 256, 256);

    this.busy = false;
    this.onPhoto(c512, img256);
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}
