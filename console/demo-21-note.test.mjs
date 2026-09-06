import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./demo-21-note.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo21-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'demo must expose a testable abo21-core script');

const context = { window: {} };
vm.runInNewContext(core[1], context);
const { notesForOctave, mapTarget, listInstruments, instrumentVolume, instrumentLayers, pianoCalibration, setPianoCalibration, resetPianoCalibration } = context.window.Abo21Core;

assert.deepEqual(
  [...notesForOctave(3), ...notesForOctave(4), ...notesForOctave(5)].map((note) => note.label),
  ['C3', 'D3', 'E3', 'F3', 'G3', 'A3', 'B3', 'C4', 'D4', 'E4', 'F4', 'G4', 'A4', 'B4', 'C5', 'D5', 'E5', 'F5', 'G5', 'A5', 'B5'],
  'the demo must expose exactly 21 diatonic target notes across three octaves',
);

assert.equal(mapTarget(48).source.label, 'B2', 'C3 uses the nearby B2 source');
assert.equal(mapTarget(55).source.label, 'C4', 'G3 uses the nearby C4 source');
assert.equal(mapTarget(69).source.label, 'C5', 'A4 uses the nearby C5 source');
assert.equal(mapTarget(83).source.label, 'C5', 'B5 remains reachable from C5');
assert.ok(Math.abs(mapTarget(62).rate - Math.pow(2, 2 / 12)) < 1e-12, 'D4 raises C4 by two semitones');

assert.deepEqual(
  Array.from(listInstruments(), (instrument) => instrument.id),
  ['piano', 'violin', 'flute'],
  'the demo must expose piano, solo violin, and flute in S8 order',
);
assert.equal(mapTarget('violin', 48).source.label, 'G3', 'C3 uses the violin G3 root');
assert.match(mapTarget('violin', 60).source.url, /violin-vsco2/, 'violin maps only to imported violin WAV files');
assert.equal(mapTarget('flute', 60).source.label, 'C4', 'C4 uses the flute C4 root');
assert.equal(mapTarget('flute', 48).source.label, 'C4', 'flute low range skips the unusably quiet C3 root');
assert.match(mapTarget('flute', 83).source.url, /flute-vsco2/, 'flute maps only to imported flute WAV files');
assert.equal(instrumentVolume('flute'), 1, 'flute keeps full direct-media volume');
assert.equal(instrumentVolume('piano'), 1, 'piano returns to normal direct-media volume');
assert.equal(instrumentLayers('piano'), 1, 'piano uses one direct-media voice');
assert.equal(instrumentLayers('violin'), 1, 'violin uses one direct-media voice');
assert.equal(instrumentLayers('flute'), 3, 'flute uses three direct-media voices for the local-file demo');
assert.deepEqual(JSON.parse(JSON.stringify(pianoCalibration(48))), { layers: 1, volume: 1 }, 'piano notes start with a neutral calibration');
setPianoCalibration(48, 2, 0.65);
assert.equal(instrumentLayers('piano', 48), 2, 'piano calibration controls each target note independently');
assert.equal(instrumentVolume('piano', 48), 0.65, 'piano calibration stores per-layer volume');
resetPianoCalibration(48);
assert.deepEqual(JSON.parse(JSON.stringify(pianoCalibration(48))), { layers: 1, volume: 1 }, 'reset restores a neutral piano calibration');

console.log('21-note mapping tests passed');
