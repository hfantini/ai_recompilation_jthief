/*
 * The game's dialogs, as modal <dialog> elements over the game. A modal
 * dialog makes the rest of the page inert, so the canvas gets no mouse input
 * while one is open, as the original's main window got none under its modal
 * dialogs. Their layout is this port's own; the long texts come from the
 * original.
 */

import { type HighScore, Speed } from './engine';
import type { Settings } from './profile';

const WAIT_MILLISECONDS = 3000;
const NAME_LENGTH = 25;

let openCount = 0;
let messageTitle = 'Jewel Thief';
const listeners = new Set<() => void>();

/** Called whenever a dialog opens or closes. */
export function onDialogsChange(listener: () => void): void {
  listeners.add(listener);
}

export function dialogsOpen(): boolean {
  return openCount > 0;
}

/** The caption of messages and questions: the original's own title, once it is loaded. */
export function setMessageTitle(title: string): void {
  messageTitle = title;
}

interface Button<T> {
  label: string;
  /** Called on click; returning undefined keeps the dialog open. */
  value: () => T | undefined;
  /** Answers Enter and takes the focus. */
  isDefault?: boolean;
}

interface DialogOptions<T> {
  title: string;
  body: Node[];
  buttons: Button<T>[];
  /** What Escape returns; undefined makes the dialog impossible to dismiss. */
  cancel?: () => T;
  className?: string;
}

interface OpenDialog<T> {
  result: Promise<T>;
  close: (value: T) => void;
}

function element<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  text?: string,
  className?: string,
): HTMLElementTagNameMap[K] {
  const created = document.createElement(tag);

  if (text !== undefined) {
    // The original's texts break lines with CR LF.
    created.textContent = text.replaceAll('\r', '');
  }
  if (className !== undefined) {
    created.className = className;
  }
  return created;
}

function notify(): void {
  listeners.forEach((listener) => listener());
}

function show<T>({ title, body, buttons, cancel, className }: DialogOptions<T>): OpenDialog<T> {
  const dialog = element('dialog', undefined, className ? `dialog ${className}` : 'dialog');
  const actions = element('div', undefined, 'actions');
  let resolve!: (value: T) => void;
  const result = new Promise<T>((done) => (resolve = done));
  let defaultButton: HTMLButtonElement | undefined;
  const close = (value: T) => {
    if (!dialog.open) {
      return;
    }
    dialog.close();
    dialog.remove();
    openCount--;
    notify();
    resolve(value);
  };

  for (const { label, value, isDefault } of buttons) {
    const button = element('button', label);

    button.type = 'button';
    button.addEventListener('click', () => {
      const answer = value();

      if (answer !== undefined) {
        close(answer);
      }
    });
    if (isDefault) {
      defaultButton = button;
      button.autofocus = true;
    }
    actions.append(button);
  }
  dialog.addEventListener('cancel', (event) => {
    event.preventDefault();
    if (cancel) {
      close(cancel());
    }
  });
  dialog.addEventListener('keydown', (event) => {
    if (event.key === 'Enter' && !(event.target instanceof HTMLButtonElement) && defaultButton) {
      event.preventDefault();
      defaultButton.click();
    }
  });
  dialog.append(element('h2', title), ...body);
  if (buttons.length > 0) {
    dialog.append(actions);
  }
  document.body.append(dialog);
  dialog.showModal();
  openCount++;
  notify();
  return { result, close };
}

function okButton(): Button<true> {
  return { label: 'OK', value: () => true, isDefault: true };
}

/** A box with an OK button. */
function inform(title: string, text: string): Promise<void> {
  return show({ title, body: [element('p', text)], buttons: [okButton()], cancel: () => true }).result.then(
    () => undefined,
  );
}

export function message(text: string): Promise<void> {
  return inform(messageTitle, text);
}

export function question(text: string): Promise<boolean> {
  return show({
    title: messageTitle,
    body: [element('p', text)],
    buttons: [
      { label: 'No', value: () => false },
      { label: 'Yes', value: () => true, isDefault: true },
    ],
    cancel: () => false,
  }).result;
}

export function pause(): Promise<void> {
  return show({
    title: 'Paused',
    body: [element('p', 'Paused...')],
    buttons: [{ label: 'Play', value: () => true, isDefault: true }],
    cancel: () => true,
  }).result.then(() => undefined);
}

/** "Get ready...", closing itself after 3 seconds; the original shows an hourglass meanwhile. */
export function wait(): Promise<void> {
  const { result, close } = show<void>({
    title: 'Get ready',
    body: [element('p', 'Get ready...')],
    buttons: [],
    className: 'waiting',
  });

  setTimeout(() => close(), WAIT_MILLISECONDS);
  return result;
}

/** Resolves with the new settings, or null if the player cancels. */
export function options(current: Settings, onHelp: () => void): Promise<Settings | null> {
  const speeds = element('fieldset');
  const choices: [Speed, string][] = [
    [Speed.Beginner, 'Beginner'],
    [Speed.Intermediate, 'Intermediate'],
    [Speed.Expert, 'Expert'],
  ];
  const checkbox = (label: string, checked: boolean) => {
    const row = element('label', undefined, 'option');
    const input = element('input');

    input.type = 'checkbox';
    input.checked = checked;
    row.append(input, label);
    return { row, input };
  };
  const levelWait = checkbox('Pause after each level', current.levelWait);
  const cheat = checkbox('Cheat mode: the thief can leave the playing field', current.cheat);
  const selectedSpeed = () =>
    Number(speeds.querySelector<HTMLInputElement>('input:checked')?.value ?? Speed.Intermediate) as Speed;

  speeds.append(element('legend', 'Speed'));
  for (const [speed, label] of choices) {
    const row = element('label', undefined, 'option');
    const input = element('input');

    input.type = 'radio';
    input.name = 'speed';
    input.value = String(speed);
    input.checked = speed === current.speed;
    row.append(input, label);
    speeds.append(row);
  }
  return show<Settings | null>({
    title: 'Options',
    body: [speeds, levelWait.row, cheat.row],
    buttons: [
      // The original's Help button opened WinHelp; the instructions box stands in.
      { label: 'Help', value: () => (onHelp(), undefined) },
      { label: 'Cancel', value: () => null },
      {
        label: 'OK',
        value: () => ({ speed: selectedSpeed(), levelWait: levelWait.input.checked, cheat: cheat.input.checked }),
        isDefault: true,
      },
    ],
    cancel: () => null,
  }).result;
}

export function highScores(table: HighScore[]): Promise<void> {
  const rows = element('table', undefined, 'scores');

  table.forEach(({ name, score }, index) => {
    const row = element('tr');

    row.append(element('td', `${index + 1}.`), element('td', String(score)), element('td', name));
    rows.append(row);
  });
  return show({ title: 'High Scores', body: [rows], buttons: [okButton()], cancel: () => true }).result.then(
    () => undefined,
  );
}

/** Resolves with the name entered, empty if the player cancels. */
export function newHighScore(score: number): Promise<string> {
  const input = element('input', undefined, 'name');

  input.maxLength = NAME_LENGTH;
  input.autofocus = true;
  return show({
    title: 'New High Score',
    body: [element('p', `Your score of ${score} is in the top ten.`), element('p', 'Enter your name:'), input],
    buttons: [
      { label: 'Cancel', value: () => '' },
      { label: 'OK', value: () => input.value, isDefault: true },
    ],
    cancel: () => '',
  }).result;
}

export function about(): Promise<void> {
  return inform(
    'About Jewel Thief',
    'Jewel Thief v1.4\nOriginal game © 1991–1993 ServantWare.\n\n' +
      'Browser port. The graphics and texts are read from your own copy of JWLTHIEF.EXE, ' +
      'which never leaves your browser.',
  );
}

/** One of the original's help boxes; nothing shows if the original lacks it. */
export function help(title: string, text: string): Promise<void> {
  return text === '' ? Promise.resolve() : inform(title, text);
}
