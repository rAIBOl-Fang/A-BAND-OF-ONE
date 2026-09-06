import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./listen-clarinet-eiad.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo-eiad-listener-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'EIAD listener must expose a testable decoder core script');

const context = { window: {}, ArrayBuffer, DataView, Uint8Array, Int16Array, Math };
vm.runInNewContext(core[1], context);
const { decodeEiad } = context.window.AboEiadListenerCore;

const encoded = await readFile(new URL('../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/clarinet_d3.eiad', import.meta.url));
const decoded = decodeEiad(encoded.buffer.slice(encoded.byteOffset, encoded.byteOffset + encoded.byteLength));

assert.equal(decoded.sampleRate, 48000, 'EIAD header exposes the approved 48kHz rate');
assert.equal(decoded.samples.length, 38400, 'D3 preview decodes to the approved 0.8s window');
assert.equal(decoded.samples[0], decoded.firstPredictor, 'first decoded sample preserves the frame predictor');
assert.ok(decoded.samples.some((sample) => sample !== 0), 'decoded source contains audible PCM data');

console.log('clarinet EIAD listener decoder tests passed');
