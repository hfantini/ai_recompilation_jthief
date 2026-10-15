import { Engine, OpenStatus } from './engine';
import { Game } from './game';
import { forgetOriginal, loadOriginal, saveOriginal } from './original-store';
import { createMenu } from './menu';
import { pickOriginal } from './picker';
import { Screen } from './screen';

const menuBar = document.querySelector<HTMLElement>('#menu-bar')!;
const stage = document.querySelector<HTMLElement>('.stage')!;
const canvas = document.querySelector<HTMLCanvasElement>('#game')!;
const picker = document.querySelector<HTMLDivElement>('#picker')!;
const pickerInput = document.querySelector<HTMLInputElement>('#picker-input')!;
const pickerMessage = document.querySelector<HTMLParagraphElement>('#picker-message')!;

// Keys the original never had, as --debug turns on in the native ports.
const debug = new URLSearchParams(location.search).has('debug');

function openError(engine: Engine, status: OpenStatus): string {
  switch (status) {
    case OpenStatus.NotOriginal:
      return 'That file is not the original Jewel Thief v1.4 (JWLTHIEF.EXE). Try another one.';
    case OpenStatus.Malformed:
      return 'The original JWLTHIEF.EXE could not be parsed.';
    case OpenStatus.MissingBitmap:
      return `Bitmap ${engine.missingName()} is missing from JWLTHIEF.EXE.`;
    case OpenStatus.MissingCursor:
      return `Cursor ${engine.missingName()} is missing from JWLTHIEF.EXE.`;
    default:
      return 'Out of memory.';
  }
}

/** Opens the cached original, or the one the user picks; false on an error the game cannot recover from. */
async function openOriginal(engine: Engine): Promise<boolean> {
  let original = await loadOriginal();
  let cached = original !== null;

  for (;;) {
    if (original === null) {
      picker.hidden = false;
      original = await pickOriginal(picker, pickerInput);
      cached = false;
    }
    const status = engine.open(original);

    if (status === OpenStatus.Ok) {
      if (!cached) {
        await saveOriginal(original);
      }
      picker.hidden = true;
      return true;
    }
    pickerMessage.textContent = openError(engine, status);
    if (status !== OpenStatus.NotOriginal) {
      picker.hidden = false;
      return false;
    }
    if (cached) {
      await forgetOriginal();
    }
    original = null;
  }
}

const engine = await Engine.load();

const screen = new Screen(canvas, stage, engine.width, engine.height);

if (await openOriginal(engine)) {
  const game = new Game(engine, screen, debug);

  createMenu(menuBar, (action) => game.run(action));
  game.start();
}
