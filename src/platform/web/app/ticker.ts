/*
 * The game clock, with the semantics of the native ports' tickers: a fixed
 * period measured against absolute deadlines, so delays do not add up, and
 * late ticks are not caught up. requestAnimationFrame is not used because it
 * would round every tick to the display's frame rate.
 */
export class Ticker {
  private timer: number | undefined;
  private next = 0;

  constructor(
    private readonly periodMilliseconds: number,
    private readonly onTick: () => void,
  ) {}

  start(): void {
    this.next = performance.now() + this.periodMilliseconds;
    this.schedule();
  }

  stop(): void {
    window.clearTimeout(this.timer);
    this.timer = undefined;
  }

  private schedule(): void {
    this.timer = window.setTimeout(() => this.fire(), Math.max(0, this.next - performance.now()));
  }

  private fire(): void {
    const now = performance.now();

    if (now >= this.next) {
      this.onTick();
      this.next += this.periodMilliseconds;
      // After a stall, such as a background tab, resume from now rather than firing a burst.
      if (now - this.next > this.periodMilliseconds) {
        this.next = now + this.periodMilliseconds;
      }
    }
    this.schedule();
  }
}
