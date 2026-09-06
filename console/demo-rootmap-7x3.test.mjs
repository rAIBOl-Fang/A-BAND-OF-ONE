import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./demo-rootmap-7x3.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo-rootmap-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'demo must expose a testable abo-rootmap-core script');

const context = { window: {} };
vm.runInNewContext(core[1], context);
const { ROOT_SETS, targets, mapTarget, maxAbsShift } = context.window.AboRootMapCore;

assert.equal(targets().length, 21, 'the demo covers all 21 diatonic target notes');
assert.equal(ROOT_SETS.three.length, 3, 'the comparison keeps the old three-root baseline');
assert.equal(ROOT_SETS.seven.length, 7, 'the candidate uses seven piano roots');
assert.equal(maxAbsShift('three'), 11, 'the old baseline reaches an eleven-semitone shift');
assert.equal(maxAbsShift('seven'), 3, 'the candidate caps every target at three semitones');
assert.equal(mapTarget('seven', 48).source.label, 'B2', 'C3 retains B2 as its closest source');
assert.equal(mapTarget('seven', 57).source.label, 'F#3', 'A3 uses F#3 rather than a distant C4 root');
assert.match(mapTarget('seven', 57).source.url, /F%233vH\.wav$/, 'sharp filenames are URL-encoded so # is not interpreted as a fragment');
assert.equal(mapTarget('seven', 67).source.label, 'F#4', 'G4 uses F#4 rather than a distant C5 root');
assert.equal(mapTarget('seven', 83).source.label, 'C6', 'B5 uses C6 rather than a distant C5 root');

console.log('7-root mapping demo tests passed');
