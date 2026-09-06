import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./demo-piano-c4-loop-probe.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo-piano-c4-loop-probe-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'C4 loop probe must expose a testable core script');
assert.match(html, /piano-c4-loop-probe\//);
assert.match(html, /source\.loopStart = asset\.loop_start_sample/);
assert.match(html, /source\.loopEnd = asset\.loop_end_sample/);
assert.match(html, /pointerdown/);
assert.match(html, /pointerup/);

const context = { window: {} };
vm.runInNewContext(core[1], context);
const { CANDIDATES, seconds } = context.window.AboPianoC4LoopProbeCore;
assert.deepEqual(JSON.parse(JSON.stringify(Object.keys(CANDIDATES))), ['piano_c4_baseline', 'piano_c4_sfz_loop', 'piano_c4_sfz_loop_b2']);
assert.equal(CANDIDATES.piano_c4_baseline.tone, '基线');
assert.equal(CANDIDATES.piano_c4_sfz_loop.tone, 'B');
assert.equal(CANDIDATES.piano_c4_sfz_loop_b2.tone, 'B2');
assert.equal(seconds(48000, 48000), '1.00 s');

console.log('C4 loop probe page tests passed');
