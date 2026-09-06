import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./diagnose-clarinet-assets.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo-clarinet-diagnose-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'clarinet diagnostic page must expose a testable core script');

const context = { window: {}, Float32Array };
vm.runInNewContext(core[1], context);
const { estimateFundamental, expectedHz, centsFromHz, XI_CASES } = context.window.AboClarinetDiagnoseCore;

const sampleRate = 48000;
const samples = Float32Array.from({ length: sampleRate }, (_, index) => Math.sin(2 * Math.PI * 440 * index / sampleRate));
const frequency = estimateFundamental(samples, sampleRate, 300, 600);

assert.ok(Math.abs(frequency - 440) < 2, `440Hz test signal must estimate near 440Hz, got ${frequency}`);
assert.ok(Math.abs(centsFromHz(440, expectedHz('A4'))) < 0.01, 'A4 at 440Hz must be 0 cents from reference');
assert.equal(Array.from(XI_CASES, (item) => item.target + '<-' + item.root).join(','), 'B3<-A#3,B4<-A#4,B5<-A#5');
assert.ok(XI_CASES.every((item) => Math.abs(item.rate - Math.pow(2, 1 / 12)) < 1e-12), 'every xi A/B case must raise its root by exactly one semitone');

console.log('clarinet asset diagnostic tests passed');
