import type { StatusText } from './engine';

/*
 * The game's screen. It shows the game at the largest whole number of device
 * pixels per game pixel, up to two, that fits its container: any other ratio, such as the
 * 1.25 of a 125% zoom, draws some game pixels one device pixel wide and others
 * two, which shows as jagged edges.
 *
 * The canvas is sized in device pixels: the frame is enlarged onto it without
 * smoothing, and the status texts are drawn at full resolution so they stay sharp.
 */

// Room kept around the game, in CSS pixels.
const MARGIN = 16;

// Larger scales make the game too big to take in at a glance.
const MAX_SCALE = 2;

/*
 * Close to the Windows 3.1 System font the original prints the status bar
 * with. TextOut places text by the top of the font's 16-pixel cell, whose
 * baseline sits 13 pixels down; drawing from that baseline keeps the text
 * where the original's was.
 */
const STATUS_FONT = 'bold 13px sans-serif';
const SYSTEM_FONT_ASCENT = 13;

export class Screen {
  /** Device pixels per game pixel. */
  scale = 1;
  /** Device pixels per CSS pixel, the browser's zoom included. */
  pixelRatio = 1;

  private readonly context: CanvasRenderingContext2D;
  private readonly frame = document.createElement('canvas');
  private readonly frameContext: CanvasRenderingContext2D;
  private readonly listeners = new Set<() => void>();
  private texts: StatusText[] = [];

  constructor(
    readonly canvas: HTMLCanvasElement,
    private readonly container: HTMLElement,
    readonly width: number,
    readonly height: number,
  ) {
    this.context = canvas.getContext('2d')!;
    this.frame.width = width;
    this.frame.height = height;
    this.frameContext = this.frame.getContext('2d')!;
    new ResizeObserver(() => this.fit()).observe(container);
    this.watchPixelRatio();
    this.fit();
  }

  /** Called whenever the scale changes. */
  onScaleChange(listener: () => void): void {
    this.listeners.add(listener);
  }

  draw(frame: ImageData, texts: StatusText[]): void {
    this.frameContext.putImageData(frame, 0, 0);
    this.texts = texts;
    this.present();
  }

  /** The game pixel under a pointer event, the screen possibly being scaled. */
  toGamePoint(event: PointerEvent): { x: number; y: number } {
    const bounds = this.canvas.getBoundingClientRect();

    return {
      x: Math.floor(((event.clientX - bounds.left) * this.width) / bounds.width),
      y: Math.floor(((event.clientY - bounds.top) * this.height) / bounds.height),
    };
  }

  private present(): void {
    const context = this.context;

    context.setTransform(1, 0, 0, 1, 0, 0);
    context.imageSmoothingEnabled = false;
    context.drawImage(this.frame, 0, 0, this.canvas.width, this.canvas.height);
    context.setTransform(this.scale, 0, 0, this.scale, 0, 0);
    context.font = STATUS_FONT;
    context.textBaseline = 'alphabetic';
    context.fillStyle = '#000';
    for (const { text, x, y } of this.texts) {
      context.fillText(text, x, y + SYSTEM_FONT_ASCENT);
    }
  }

  private fit(): void {
    // Includes the browser's zoom as well as the system's display scale.
    const ratio = window.devicePixelRatio;
    const width = (this.container.clientWidth - 2 * MARGIN) * ratio;
    const height = (this.container.clientHeight - 2 * MARGIN) * ratio;
    const fitting = Math.floor(Math.min(width / this.width, height / this.height));
    const scale = Math.max(1, Math.min(MAX_SCALE, fitting));

    const changed = scale !== this.scale || ratio !== this.pixelRatio;

    this.scale = scale;
    this.pixelRatio = ratio;
    this.canvas.width = this.width * scale;
    this.canvas.height = this.height * scale;
    this.canvas.style.width = `${this.canvas.width / ratio}px`;
    this.canvas.style.height = `${this.canvas.height / ratio}px`;
    this.present();
    if (changed) {
      this.listeners.forEach((listener) => listener());
    }
  }

  // Moving the window to a display of another scale changes the ratio without resizing it.
  private watchPixelRatio(): void {
    matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`).addEventListener(
      'change',
      () => {
        this.fit();
        this.watchPixelRatio();
      },
      { once: true },
    );
  }
}
