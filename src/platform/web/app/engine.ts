import createJewelThief, { type JewelThiefModule } from '@wasm/jthief.js';

// Mirrors JtStatus in bridge.c.
export enum OpenStatus {
  Ok = 0,
  NotOriginal,
  Malformed,
  MissingBitmap,
  MissingCursor,
  OutOfMemory,
}

// Mirrors GAME_EVENT_* in game.h: what a tick or a mouse move did.
export enum GameEvent {
  LifeLost = 1 << 0,
  ExtraLife = 1 << 1,
  JewelTaken = 1 << 2,
  LevelUp = 1 << 3,
  GameOver = 1 << 4,
  GameCompleted = 1 << 5,
}

// Mirrors AssetsText in assets.h: offsets of texts in the original's data segment.
export enum Text {
  Title = 0x01d3,
  TradeJewels = 0x01df,
  PlayAgain = 0x026a,
  TryExpert = 0x0308,
}

export enum Speed {
  Beginner = 0,
  Intermediate = 1,
  Expert = 2,
}

export enum Cursor {
  Right = 0,
  Left = 1,
}

export interface StatusText {
  text: string;
  x: number;
  y: number;
}

export interface HighScore {
  name: string;
  score: number;
}

/** The "Scores" and "Names" values of SERVANT.INI. */
export interface HighScoreValues {
  scores: string;
  names: string;
}

export interface CursorImage {
  image: ImageData;
  hotspotX: number;
  hotspotY: number;
}

const STATUS_TEXT_COUNT = 3;
const HIGH_SCORES = 10;
const NULL_POINTER = 0;

// The original's strings are Windows-1252; the reverse table encodes what goes back to C.
const decoder = new TextDecoder('windows-1252');
const encoding = new Map<string, number>(
  Array.from({ length: 256 }, (_, byte) => [decoder.decode(new Uint8Array([byte])), byte]),
);
const UNMAPPED_CHARACTER = 0x3f; // '?'

function encode(text: string): Uint8Array {
  return Uint8Array.from(Array.from(text), (character) => encoding.get(character) ?? UNMAPPED_CHARACTER);
}

/** The game compiled to WebAssembly, behind a typed API. */
export class Engine {
  readonly width: number;
  readonly height: number;

  private constructor(private readonly wasm: JewelThiefModule) {
    this.width = wasm._jt_width();
    this.height = wasm._jt_height();
  }

  static async load(): Promise<Engine> {
    return new Engine(await createJewelThief());
  }

  /** After NotOriginal another file may be tried; any other failure leaves the engine unusable. */
  open(original: Uint8Array): OpenStatus {
    const pointer = this.wasm._malloc(original.length);

    this.heap.set(original, pointer);
    // bridge.c owns the buffer from here on, even on failure.
    return this.wasm._jt_open(pointer, original.length) as OpenStatus;
  }

  /** The bitmap or cursor a failed open() did not find. */
  missingName(): string {
    return this.readString(this.wasm._jt_missing_name());
  }

  /** `seed` plays the part of srand(time(NULL)): only its low 16 bits count. */
  start(seed: number, speed: Speed, cheat: boolean): void {
    this.wasm._jt_start(seed, speed, cheat);
  }

  newGame(speed: Speed, cheat: boolean): void {
    this.wasm._jt_new_game(speed, cheat);
  }

  /** Returns GameEvent flags. */
  tick(): number {
    return this.wasm._jt_tick();
  }

  /** Returns GameEvent flags. */
  mouseMove(x: number, y: number): number {
    return this.wasm._jt_mouse_move(x, y);
  }

  setFocus(focused: boolean): void {
    this.wasm._jt_set_focus(focused);
  }

  get level(): number {
    return this.wasm._jt_level();
  }

  /** The score as displayed, which is what the high scores hold. */
  get finalScore(): number {
    return this.wasm._jt_final_score();
  }

  cursorFacesLeft(x: number, y: number): boolean {
    return this.wasm._jt_cursor_faces_left(x, y) !== 0;
  }

  /** Returns whether god mode is now on. */
  toggleGodMode(): boolean {
    return this.wasm._jt_toggle_god_mode() !== 0;
  }

  changeLevel(delta: number): void {
    this.wasm._jt_debug_change_level(delta);
  }

  text(text: Text): string {
    return this.readString(this.wasm._jt_text(text));
  }

  /** The message shown after completing the game on Expert. */
  endingMessage(): string {
    return this.readString(this.wasm._jt_ending_message());
  }

  /** A null value stands for a missing SERVANT.INI key. */
  parseSpeed(value: string | null): Speed {
    return this.withString(value, (pointer) => this.wasm._jt_parse_speed(pointer));
  }

  parseLevelWait(value: string | null): boolean {
    return this.withString(value, (pointer) => this.wasm._jt_parse_level_wait(pointer)) !== 0;
  }

  parseCheat(value: string | null): boolean {
    return this.withString(value, (pointer) => this.wasm._jt_parse_cheat(pointer)) !== 0;
  }

  /** The SERVANT.INI value for a speed, as the options dialog writes it. */
  speedValue(speed: Speed): string {
    return this.readString(this.wasm._jt_speed_value(speed));
  }

  flagValue(flag: boolean): string {
    return this.readString(this.wasm._jt_flag_value(flag));
  }

  /** The STATIC texts of one of the original's dialogs, such as "INSTRBOX". */
  dialogText(name: string): string {
    return this.withString(name, (pointer) => this.readString(this.wasm._jt_dialog_text(pointer)));
  }

  /** Null values stand for missing keys and take the original's defaults. */
  loadHighScores(scores: string | null, names: string | null): void {
    this.withString(scores, (scoresPointer) =>
      this.withString(names, (namesPointer) => this.wasm._jt_scores_load(scoresPointer, namesPointer)),
    );
  }

  /** Place the score would take in the loaded table (0 is the best), or -1. */
  highScoreRank(score: number): number {
    return this.wasm._jt_scores_rank(score);
  }

  insertHighScore(rank: number, name: string, score: number): void {
    this.withString(name, (pointer) => this.wasm._jt_scores_insert(rank, pointer, score));
  }

  highScores(): HighScore[] {
    return Array.from({ length: HIGH_SCORES }, (_, index) => ({
      name: this.readString(this.wasm._jt_scores_name(index)),
      score: this.wasm._jt_scores_score(index),
    }));
  }

  /** The loaded table, formatted as the original writes it. */
  highScoreValues(): HighScoreValues {
    const scores = this.readString(this.wasm._jt_scores_value());

    return { scores, names: this.readString(this.wasm._jt_names_value()) };
  }

  render(): ImageData {
    const pointer = this.wasm._jt_render();
    const pixels = new Uint8ClampedArray(this.heap.buffer, pointer, this.width * this.height * 4);

    return new ImageData(pixels.slice(), this.width, this.height);
  }

  /** The level, lives and score texts of the last render(). */
  statusTexts(): StatusText[] {
    return Array.from({ length: STATUS_TEXT_COUNT }, (_, index) => ({
      text: this.readString(this.wasm._jt_status_text(index)),
      x: this.wasm._jt_status_x(index),
      y: this.wasm._jt_status_y(index),
    }));
  }

  cursor(cursor: Cursor): CursorImage {
    const width = this.wasm._jt_cursor_width(cursor);
    const height = this.wasm._jt_cursor_height(cursor);
    const pointer = this.wasm._jt_cursor_rgba(cursor);
    const pixels = new Uint8ClampedArray(this.heap.buffer, pointer, width * height * 4);

    return {
      image: new ImageData(pixels.slice(), width, height),
      hotspotX: this.wasm._jt_cursor_hotspot_x(cursor),
      hotspotY: this.wasm._jt_cursor_hotspot_y(cursor),
    };
  }

  // Memory growth replaces the buffer, so views must not be kept across calls.
  private get heap(): Uint8Array {
    return this.wasm.HEAPU8;
  }

  private readString(pointer: number): string {
    const heap = this.heap;
    const end = heap.indexOf(0, pointer);

    return decoder.decode(heap.subarray(pointer, end));
  }

  private withString<T>(text: string | null, use: (pointer: number) => T): T {
    if (text === null) {
      return use(NULL_POINTER);
    }
    const bytes = encode(text);
    const pointer = this.wasm._malloc(bytes.length + 1);

    try {
      this.heap.set(bytes, pointer);
      this.heap[pointer + bytes.length] = 0;
      return use(pointer);
    } finally {
      this.wasm._free(pointer);
    }
  }
}
