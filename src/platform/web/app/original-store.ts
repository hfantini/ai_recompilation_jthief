// Keeps the user's JWLTHIEF.EXE in IndexedDB, so it is picked only once per browser.

const DATABASE = 'jthief';
const STORE = 'files';
const KEY = 'JWLTHIEF.EXE';

function openDatabase(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DATABASE, 1);

    request.onupgradeneeded = () => request.result.createObjectStore(STORE);
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

async function run<T>(mode: IDBTransactionMode, action: (store: IDBObjectStore) => IDBRequest<T>): Promise<T> {
  const database = await openDatabase();

  try {
    return await new Promise((resolve, reject) => {
      const request = action(database.transaction(STORE, mode).objectStore(STORE));

      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
  } finally {
    database.close();
  }
}

/** Storage may be unavailable, as in private windows: the game then asks every time. */
export async function loadOriginal(): Promise<Uint8Array | null> {
  try {
    const stored = await run<ArrayBuffer | undefined>('readonly', (store) => store.get(KEY));
    return stored ? new Uint8Array(stored) : null;
  } catch {
    return null;
  }
}

export async function saveOriginal(original: Uint8Array): Promise<void> {
  try {
    await run('readwrite', (store) => store.put(original.slice().buffer, KEY));
  } catch {
    // Not cached; the next visit asks again.
  }
}

export async function forgetOriginal(): Promise<void> {
  try {
    await run('readwrite', (store) => store.delete(KEY));
  } catch {
    // Nothing to forget.
  }
}
