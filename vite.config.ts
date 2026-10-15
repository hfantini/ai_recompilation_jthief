import { fileURLToPath } from 'node:url';
import { defineConfig } from 'vite';

const repository = (path: string) => fileURLToPath(new URL(path, import.meta.url));

// The app lives with the other platform layers; the WebAssembly module comes
// from the CMake `wasm` preset, outside the app's root.
export default defineConfig({
  root: repository('./src/platform/web/app'),
  resolve: {
    alias: { '@wasm': repository('./build/wasm') },
  },
  server: {
    fs: { allow: [repository('.')] },
  },
  build: {
    outDir: repository('./bin/web'),
    emptyOutDir: true,
  },
});
