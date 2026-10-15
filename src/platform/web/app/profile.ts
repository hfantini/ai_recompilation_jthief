import type { Engine, Speed } from './engine';

/*
 * SERVANT.INI in localStorage: the same keys and values as the original's
 * [Jewel Thief] section, parsed and formatted by the original's rules in C.
 */

const PREFIX = 'jthief.';

export interface Settings {
  speed: Speed;
  levelWait: boolean;
  cheat: boolean;
}

// Storage may be unavailable, as in private windows: keys then read as missing.
function read(key: string): string | null {
  try {
    return localStorage.getItem(PREFIX + key);
  } catch {
    return null;
  }
}

function write(key: string, value: string): void {
  try {
    localStorage.setItem(PREFIX + key, value);
  } catch {
    // Not saved; the game goes on with what it has.
  }
}

/** Read each time it is needed, as the original reads SERVANT.INI. */
export function loadSettings(engine: Engine): Settings {
  return {
    speed: engine.parseSpeed(read('Speed')),
    levelWait: engine.parseLevelWait(read('LevelWait')),
    cheat: engine.parseCheat(read('Cheat')),
  };
}

export function saveSettings(engine: Engine, settings: Settings): void {
  write('LevelWait', engine.flagValue(settings.levelWait));
  write('Cheat', engine.flagValue(settings.cheat));
  write('Speed', engine.speedValue(settings.speed));
}

/** Loads the table into the engine, which keeps the original's rules for it. */
export function loadHighScores(engine: Engine): void {
  engine.loadHighScores(read('Scores'), read('Names'));
}

export function saveHighScores(engine: Engine): void {
  const { scores, names } = engine.highScoreValues();

  write('Scores', scores);
  write('Names', names);
}
