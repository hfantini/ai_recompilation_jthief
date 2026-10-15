// Asks the user for their JWLTHIEF.EXE, by file dialog or drag and drop.

/** Resolves with the bytes of the first file the user picks or drops on `dropZone`. */
export function pickOriginal(dropZone: HTMLElement, input: HTMLInputElement): Promise<Uint8Array> {
  return new Promise((resolve) => {
    const finish = async (file: File | undefined) => {
      if (!file) {
        return;
      }
      cleanUp();
      resolve(new Uint8Array(await file.arrayBuffer()));
    };
    const onChange = () => finish(input.files?.[0]);
    const onDragOver = (event: DragEvent) => {
      event.preventDefault();
      dropZone.classList.add('dragging');
    };
    const onDragLeave = () => dropZone.classList.remove('dragging');
    const onDrop = (event: DragEvent) => {
      event.preventDefault();
      dropZone.classList.remove('dragging');
      finish(event.dataTransfer?.files[0]);
    };
    const cleanUp = () => {
      input.removeEventListener('change', onChange);
      dropZone.removeEventListener('dragover', onDragOver);
      dropZone.removeEventListener('dragleave', onDragLeave);
      dropZone.removeEventListener('drop', onDrop);
      input.value = '';
    };

    input.addEventListener('change', onChange);
    dropZone.addEventListener('dragover', onDragOver);
    dropZone.addEventListener('dragleave', onDragLeave);
    dropZone.addEventListener('drop', onDrop);
  });
}
