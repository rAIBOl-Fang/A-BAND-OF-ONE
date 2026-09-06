import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./demo-clarinet-probe.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo-clarinet-probe-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'clarinet probe demo must expose a testable core script');

const context = { window: {} };
vm.runInNewContext(core[1], context);
const { ROOTS, PLAYABLE_ROOTS, mapTarget } = context.window.AboClarinetProbeCore;

assert.equal(ROOTS.map((root) => root.note).join(','), 'D3,F3,A#3,D4,F4,A#4,D5,F5,A#5');
assert.equal(Array.from(PLAYABLE_ROOTS).join(','), 'D3,F3,A#3,D4,F4,A#4,D5,F5,A#5', 'all nine approved clarinet roots are playable in the full probe');

for (const target of ['C3', 'F#3', 'C4', 'F#4', 'C5', 'F#5', 'B5']) {
  const mapped = mapTarget(target);
  assert.ok(Math.abs(mapped.semitones) <= 3, `${target} must stay within ±3 semitones`);
}

assert.equal(mapTarget('C3').root.note, 'D3');
assert.equal(mapTarget('B5').root.note, 'A#5');

console.log('clarinet probe mapping tests passed');
