import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./listen-clarinet-high-ab.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo-clarinet-high-ab-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'high-note A/B listener must expose a testable core script');

const context = { window: {} };
vm.runInNewContext(core[1], context);
const { COMPARE_NOTES, MODES, loopBounds } = context.window.AboClarinetHighAbCore;

assert.deepEqual(JSON.parse(JSON.stringify(COMPARE_NOTES)), ['D5', 'F5', 'A#5'], 'A/B page is limited to reported high notes');
assert.deepEqual(JSON.parse(JSON.stringify(MODES)), ['one-shot', 'loop'], 'every candidate must be comparable as one-shot and loop');
assert.deepEqual(JSON.parse(JSON.stringify(loopBounds({ sample_rate_hz: 48000, loop_start_sample: 20160, loop_end_sample: 28656 }))), [0.42, 0.597], 'loop bounds are read from each bundle manifest');

console.log('clarinet high-note A/B listener tests passed');
