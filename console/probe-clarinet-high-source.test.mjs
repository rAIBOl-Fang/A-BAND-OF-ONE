import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./probe-clarinet-high-source.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo-clarinet-high-source-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'high-note source probe must expose a testable core script');

const context = { window: {}, Float32Array, Math };
vm.runInNewContext(core[1], context);
const { HIGH_NOTES, WINDOWS, sliceMono48k } = context.window.AboClarinetHighSourceCore;

assert.deepEqual(JSON.parse(JSON.stringify(HIGH_NOTES)), ['D5', 'F5', 'A#5'], 'probe is limited to the three reported high notes');
assert.deepEqual(JSON.parse(JSON.stringify(WINDOWS.map((item) => item.startSeconds))), [0, 1.5, 3, 4.5], 'probe compares the current window with three later candidates');

const source = Float32Array.from({ length: 44100 * 6 }, (_, index) => index / 44100);
const selected = sliceMono48k([source], 44100, 1.5, 0.8);
assert.equal(selected.length, 38400, 'every candidate exports an 0.8s 48kHz window');
assert.ok(Math.abs(selected[0] - 1.5) < 0.001, 'window export starts at the selected source time');

console.log('clarinet high-note source probe tests passed');
