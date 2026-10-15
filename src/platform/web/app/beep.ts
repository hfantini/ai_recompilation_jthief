// Stands in for MessageBeep, which the original plays when the thief is caught.

const FREQUENCY_HZ = 880;
const DURATION_SECONDS = 0.12;
const VOLUME = 0.15;

let audio: AudioContext | undefined;

/** Browsers keep audio muted until the page gets a click or a key press. */
export function beep(): void {
  audio ??= new AudioContext();
  if (audio.state === 'suspended') {
    void audio.resume();
  }
  const oscillator = audio.createOscillator();
  const gain = audio.createGain();
  const end = audio.currentTime + DURATION_SECONDS;

  oscillator.type = 'square';
  oscillator.frequency.value = FREQUENCY_HZ;
  gain.gain.value = VOLUME;
  oscillator.connect(gain).connect(audio.destination);
  oscillator.start();
  oscillator.stop(end);
}
