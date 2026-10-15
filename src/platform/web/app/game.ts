/*
 * The game flow around the rules, ported from the GTK entry point: what to
 * do after each tick and mouse move, the "Get ready" pauses, the end of a
 * game with its high scores, and the menu commands. Only the gameplay
 * follows the original; the dialogs are this port's own.
 */

import { beep } from './beep';
import { Cursors } from './cursors';
import * as dialogs from './dialogs';
import { Cursor, type Engine, GameEvent, Speed, Text } from './engine';
import type { Action } from './menu';
import { loadHighScores, loadSettings, saveHighScores, saveSettings } from './profile';
import type { Screen } from './screen';
import { Ticker } from './ticker';

/*
 * Windows 3.1 timers ran on the PC timer chip at 1193182 / 65536 Hz, so the
 * game's 5 ms timer really fired every ~54.9 ms; that is the game's pace.
 */
const TICK_MILLISECONDS = (65536 / 1193182) * 1000;

const TITLE = 'Jewel Thief';

export class Game {
  private readonly cursors: Cursors;
  private cursor = Cursor.Right;
  private readonly ticker = new Ticker(TICK_MILLISECONDS, () => this.handleTick());
  private focused: boolean | undefined;

  constructor(
    private readonly engine: Engine,
    private readonly screen: Screen,
    private readonly debug: boolean,
  ) {
    this.cursors = new Cursors(engine, screen);
  }

  /*
   * Startup as in the original, minus the shareware About screen: the game is
   * dealt before it shows, and the "Get ready" box may follow.
   */
  start(): void {
    const settings = loadSettings(this.engine);
    // Like srand(time(NULL)): only the low 16 bits of the time are used.
    const seed = Math.floor(Date.now() / 1000) & 0xffff;

    this.engine.start(seed, settings.speed, settings.cheat);
    dialogs.setMessageTitle(this.engine.text(Text.Title));
    this.showCursor(Cursor.Right);
    this.listen();
    this.updateTitle(false);
    this.updateFocus();
    this.draw();
    this.ticker.start();
    void this.waitIfEnabled();
  }

  /** A menu command; ignored under a dialog, as menus of a window under a modal dialog are. */
  run(action: Action): void {
    if (dialogs.dialogsOpen()) {
      return;
    }
    switch (action) {
      case 'new-game':
        void this.requestNewGame();
        break;
      case 'pause':
        void dialogs.pause();
        break;
      case 'options':
        void this.showOptions();
        break;
      case 'high-scores':
        loadHighScores(this.engine);
        void dialogs.highScores(this.engine.highScores());
        break;
      case 'instructions':
        void this.showHelp('Instructions', 'INSTRBOX');
        break;
      case 'hints':
        void this.showHelp('Hints', 'HINTSBOX');
        break;
      case 'story':
        void this.showHelp('Our Story', 'STORYBOX');
        break;
      case 'about':
        void dialogs.about();
        break;
    }
  }

  private listen(): void {
    const onMove = (event: PointerEvent) => this.handleMouseMove(event);
    const canvas = this.screen.canvas;

    // Windows sends a mouse move when the cursor enters the window, too.
    canvas.addEventListener('pointerenter', onMove);
    canvas.addEventListener('pointermove', onMove);
    this.screen.onScaleChange(() => {
      this.cursors.update();
      this.showCursor(this.cursor);
    });
    window.addEventListener('focus', () => this.updateFocus());
    window.addEventListener('blur', () => this.updateFocus());
    document.addEventListener('visibilitychange', () => this.updateFocus());
    dialogs.onDialogsChange(() => this.updateFocus());
    window.addEventListener('keydown', (event) => this.handleKey(event));
  }

  private draw(): void {
    const frame = this.engine.render();

    this.screen.draw(frame, this.engine.statusTexts());
  }

  private handleTick(): void {
    this.handleEvents(this.engine.tick());
    this.draw();
  }

  private handleMouseMove(event: PointerEvent): void {
    // The original's main window gets no mouse input under a modal dialog.
    if (dialogs.dialogsOpen()) {
      return;
    }
    const { x, y } = this.screen.toGamePoint(event);
    const events = this.engine.mouseMove(x, y);

    // The original turns the cursor before handling a jewel or a level-up.
    this.updateCursor(x, y);
    this.handleEvents(events);
  }

  /* The original turned the cursor inside the playfield only. */
  private updateCursor(x: number, y: number): void {
    this.showCursor(this.engine.cursorFacesLeft(x, y) ? Cursor.Left : Cursor.Right);
  }

  private showCursor(cursor: Cursor): void {
    this.cursor = cursor;
    this.screen.canvas.style.cursor = this.cursors.css(cursor);
  }

  private handleEvents(events: number): void {
    if (events & GameEvent.LifeLost) {
      beep();
    }
    if (events & (GameEvent.GameOver | GameEvent.GameCompleted)) {
      void this.finishGame(events);
      return;
    }
    if (events !== 0) {
      this.draw();
    }
    // A new scenery comes every second level; the original may pause there.
    if (events & GameEvent.LevelUp && this.engine.level % 2 === 0) {
      void this.waitIfEnabled();
    }
  }

  /* The original's window is inactive under its dialogs, and so is the game then. */
  private updateFocus(): void {
    const focused = document.hasFocus() && !document.hidden && !dialogs.dialogsOpen();

    if (focused !== this.focused) {
      this.focused = focused;
      this.engine.setFocus(focused);
    }
  }

  private async waitIfEnabled(): Promise<void> {
    if (loadSettings(this.engine).levelWait) {
      await dialogs.wait();
    }
  }

  /* Settings are read again, as the original reads SERVANT.INI each time. */
  private newGame(): void {
    const settings = loadSettings(this.engine);

    this.engine.newGame(settings.speed, settings.cheat);
    this.draw();
    void this.waitIfEnabled();
  }

  private async finishGame(events: number): Promise<void> {
    this.draw();
    if (events & GameEvent.GameCompleted) {
      await this.showEnding();
    }
    await this.submitHighScore();
    await this.askPlayAgain();
  }

  /*
   * Asks for a name if the score makes the table, then shows the table. The
   * table is saved even when the score misses it, as the original does.
   */
  private async submitHighScore(): Promise<void> {
    const score = this.engine.finalScore;

    loadHighScores(this.engine);
    if (this.engine.highScoreRank(score) >= 0) {
      const name = await dialogs.newHighScore(score);

      // Read again, as the original does once the name is entered.
      loadHighScores(this.engine);
      this.engine.insertHighScore(this.engine.highScoreRank(score), name, score);
    }
    saveHighScores(this.engine);
    await dialogs.highScores(this.engine.highScores());
  }

  private async showEnding(): Promise<void> {
    const expert = loadSettings(this.engine).speed === Speed.Expert;

    await dialogs.message(expert ? this.engine.endingMessage() : this.engine.text(Text.TryExpert));
  }

  private async askPlayAgain(): Promise<void> {
    if (await dialogs.question(this.engine.text(Text.PlayAgain))) {
      this.newGame();
    }
  }

  private async showOptions(): Promise<void> {
    const settings = await dialogs.options(loadSettings(this.engine), () => {
      void this.showHelp('Instructions', 'INSTRBOX');
    });

    if (settings !== null) {
      saveSettings(this.engine, settings);
      this.newGame();
    }
  }

  /* The instructions, hints and story boxes, unused by v1.4 but kept in it. */
  private showHelp(title: string, templateName: string): Promise<void> {
    return dialogs.help(title, this.engine.dialogText(templateName));
  }

  private async requestNewGame(): Promise<void> {
    if (await dialogs.question(this.engine.text(Text.TradeJewels))) {
      this.newGame();
    }
  }

  private handleKey(event: KeyboardEvent): void {
    const action = this.keyAction(event.key);

    if (action === undefined) {
      return;
    }
    // F3 is also the browser's "find next", which must not steal the game's key.
    event.preventDefault();
    if (!dialogs.dialogsOpen()) {
      action();
    }
  }

  private keyAction(key: string): (() => void) | undefined {
    switch (key) {
      case 'F2':
        return () => this.run('new-game');
      case 'F3':
        return () => this.run('pause');
    }
    if (!this.debug) {
      return undefined;
    }
    switch (key) {
      case 'Home':
        return () => this.updateTitle(this.engine.toggleGodMode());
      case 'PageUp':
      case 'PageDown':
        return () => {
          this.engine.changeLevel(key === 'PageUp' ? 1 : -1);
          this.draw();
        };
    }
    return undefined;
  }

  private updateTitle(godMode: boolean): void {
    if (!this.debug) {
      document.title = TITLE;
      return;
    }
    document.title = godMode ? `${TITLE} [DEBUG: god mode]` : `${TITLE} [DEBUG]`;
  }
}
