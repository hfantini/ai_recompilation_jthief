import { Cursor, type CursorImage, type Engine } from './engine';
import type { Screen } from './screen';

/*
 * The original's cursors as CSS cursor values, enlarged like the game so the
 * thief keeps its size next to the guards. image-set() tells the browser the
 * image's resolution, so it shows it one image pixel per device pixel.
 * Browsers may fall back to the default cursor for large cursors near the
 * edges of the screen.
 */
export class Cursors {
  private readonly images: Record<Cursor, CursorImage>;
  private values!: Record<Cursor, string>;

  constructor(engine: Engine, private readonly screen: Screen) {
    this.images = { [Cursor.Right]: engine.cursor(Cursor.Right), [Cursor.Left]: engine.cursor(Cursor.Left) };
    this.update();
  }

  /** Builds the values again for the screen's current scale. */
  update(): void {
    this.values = { [Cursor.Right]: this.toCss(Cursor.Right), [Cursor.Left]: this.toCss(Cursor.Left) };
  }

  css(cursor: Cursor): string {
    return this.values[cursor];
  }

  private toCss(cursor: Cursor): string {
    const { image, hotspotX, hotspotY } = this.images[cursor];
    const { scale, pixelRatio } = this.screen;
    const source = document.createElement('canvas');
    const enlarged = document.createElement('canvas');
    const context = enlarged.getContext('2d')!;

    source.width = image.width;
    source.height = image.height;
    source.getContext('2d')!.putImageData(image, 0, 0);
    enlarged.width = image.width * scale;
    enlarged.height = image.height * scale;
    context.imageSmoothingEnabled = false;
    context.drawImage(source, 0, 0, enlarged.width, enlarged.height);
    // The hotspot is in CSS pixels of the displayed image.
    const x = Math.round((hotspotX * scale) / pixelRatio);
    const y = Math.round((hotspotY * scale) / pixelRatio);

    const url = `url(${enlarged.toDataURL('image/png')})`;
    const value = `image-set(${url} ${pixelRatio}x) ${x} ${y}, default`;

    // Without image-set(), the image shows one pixel per CSS pixel: larger under zoom, but still there.
    return CSS.supports('cursor', value) ? value : `${url} ${x} ${y}, default`;
  }
}
