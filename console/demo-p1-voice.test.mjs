import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const page = fs.readFileSync(new URL('./demo-p1-voice.html', import.meta.url), 'utf8');
const coreMatch = page.match(/<script id="abo-p1-voice-core">([\s\S]*?)<\/script>/);
assert.ok(coreMatch, 'P1 voice demo must expose a testable core script');

const context = { window: {} };
vm.createContext(context);
vm.runInContext(coreMatch[1], context);

const { mapTarget, releaseMillis, targetNotes } = context.window.AboP1VoiceDemoCore;

assert.deepEqual(JSON.parse(JSON.stringify(targetNotes(4))), ['C4', 'D4', 'E4', 'F4', 'G4', 'A4', 'B4']);
assert.deepEqual(JSON.parse(JSON.stringify(mapTarget('violin', 'G4'))), { root: 'G4', id: 'violin_g4', semitones: 0 });
assert.deepEqual(JSON.parse(JSON.stringify(mapTarget('violin', 'B5'))), { root: 'A5', id: 'violin_a5', semitones: 2 });
assert.deepEqual(JSON.parse(JSON.stringify(mapTarget('violin', 'C3'))), { root: 'G3', id: 'violin_g3', semitones: -7 });
assert.equal(releaseMillis('piano'), 120);
assert.equal(releaseMillis('violin'), 40);
assert.equal(releaseMillis('clarinet'), 40);

console.log('P1 voice demo mapping tests passed');
