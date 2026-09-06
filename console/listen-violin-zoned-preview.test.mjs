import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./listen-violin-zoned-preview.html', import.meta.url), 'utf8');
const core = html.match(/<script id="abo-violin-zoned-preview-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'violin comparison preview must expose a testable mapping core script');

const context = { window: {} };
vm.runInNewContext(core[1], context);
const { ROOTS, mapTarget } = context.window.AboViolinZonedPreviewCore;

assert.equal(ROOTS.map((root) => root.root).join(','), 'G3,C4,E4,G4,C5,E5,A5', 'seven approved same-source roots are available');
assert.equal(mapTarget('C3').root, 'G3', 'low zone must use the same-source violin G3 root');
assert.equal(mapTarget('C3').semitones, -7, 'C3 must disclose the unavoidable -7-semitone violin simulation');
assert.equal(mapTarget('B3').root, 'C4', 'B3 switches to the nearest C4 source');
assert.equal(mapTarget('C4').root, 'C4', 'middle zone starts from C4');
assert.equal(mapTarget('G4').root, 'G4', 'middle sol uses its own G4 source');
assert.equal(mapTarget('A4').semitones, 2, 'middle la stays within two semitones of G4');
assert.equal(mapTarget('B4').root, 'C5', 'middle xi uses C5 rather than an unstable C4 upshift');
assert.equal(mapTarget('E5').root, 'E5', 'high E5 uses its own source');
assert.equal(mapTarget('B5').semitones, 2, 'high B5 stays within two semitones of A5');

console.log('same-source violin zoned preview mapping tests passed');
