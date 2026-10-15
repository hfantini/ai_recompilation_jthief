/*
 * The original's menu bar in HTML: Game and Help, with the Help commands its
 * v1.4 boxes can still answer. Exit is left out, since a page is closed by
 * the browser.
 */

export type Action =
  | 'new-game'
  | 'pause'
  | 'options'
  | 'high-scores'
  | 'instructions'
  | 'hints'
  | 'story'
  | 'about';

interface Item {
  label: string;
  action: Action;
  shortcut?: string;
}

// Each inner array is a section, separated from the next by a line.
const MENUS: { title: string; sections: Item[][] }[] = [
  {
    title: 'Game',
    sections: [
      [
        { label: 'New...', action: 'new-game', shortcut: 'F2' },
        { label: 'Pause', action: 'pause', shortcut: 'F3' },
      ],
      [
        { label: 'Options...', action: 'options' },
        { label: 'High Scores', action: 'high-scores' },
      ],
    ],
  },
  {
    title: 'Help',
    sections: [
      [
        { label: 'Instructions', action: 'instructions' },
        { label: 'Hints', action: 'hints' },
        { label: 'Our Story', action: 'story' },
      ],
      [{ label: 'About Jewel Thief...', action: 'about' }],
    ],
  },
];

export function createMenu(bar: HTMLElement, onAction: (action: Action) => void): void {
  let openMenu: HTMLElement | null = null;
  const setOpen = (menu: HTMLElement | null) => {
    openMenu?.classList.remove('open');
    openMenu = menu;
    openMenu?.classList.add('open');
  };

  for (const { title, sections } of MENUS) {
    const menu = document.createElement('div');
    const button = document.createElement('button');
    const list = document.createElement('div');

    menu.className = 'menu';
    button.className = 'menu-title';
    button.type = 'button';
    button.textContent = title;
    list.className = 'menu-items';
    sections.forEach((items, index) => {
      if (index > 0) {
        list.append(document.createElement('hr'));
      }
      for (const { label, action, shortcut } of items) {
        const item = document.createElement('button');
        const name = document.createElement('span');

        item.type = 'button';
        name.textContent = label;
        item.append(name);
        if (shortcut) {
          const key = document.createElement('kbd');

          key.textContent = shortcut;
          item.append(key);
        }
        item.addEventListener('click', () => {
          setOpen(null);
          onAction(action);
        });
        list.append(item);
      }
    });
    button.addEventListener('click', () => setOpen(openMenu === menu ? null : menu));
    // As in desktop menu bars, once a menu is open, hovering another opens that one.
    button.addEventListener('pointerenter', () => {
      if (openMenu !== null && openMenu !== menu) {
        setOpen(menu);
      }
    });
    menu.append(button, list);
    bar.append(menu);
  }
  document.addEventListener('pointerdown', (event) => {
    if (openMenu !== null && !openMenu.contains(event.target as Node)) {
      setOpen(null);
    }
  });
  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') {
      setOpen(null);
    }
  });
}
