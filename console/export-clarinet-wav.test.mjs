import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./export-clarinet-wav.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo-clarinet-wav-export-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'WAV export page must expose a testable core script');

const context = { window: {}, Float32Array, Math };
vm.runInNewContext(core[1], context);
const { EXPORTS, toMono48k } = context.window.AboClarinetWavExportCore;

assert.equal(EXPORTS.map((item) => item.output).join(','), 'clarinet_d3.wav,clarinet_f3.wav,clarinet_as3.wav,clarinet_d4.wav,clarinet_f4.wav,clarinet_as4.wav,clarinet_d5.wav,clarinet_f5.wav,clarinet_as5.wav');
assert.ok(EXPORTS.every((item) => item.seconds === 0.8), 'every exported root uses the approved 0.8s raw window');

const source = Float32Array.from({ length: 44100 }, (_, index) => index / 44100);
const converted = toMono48k([source], 44100, 0.8);
assert.equal(converted.length, 38400, '0.8s at 48kHz is exactly 38,400 samples');
assert.ok(Math.abs(converted[24000] - 0.5) < 0.001, 'linear resampling preserves the midpoint of a ramp');

console.log('clarinet WAV export tests passed');
