import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const html = await readFile(new URL('./listen-strings-source-preview.html', import.meta.url), 'utf8');
const manifest = JSON.parse(await readFile(new URL('../firmware/course-motherboard/abo_assets/raw/strings-vsco-zoned/SOURCE_MANIFEST.json', import.meta.url), 'utf8'));
const core = html.match(/<script id="abo-strings-source-preview-core">([\s\S]*?)<\/script>/);
assert.ok(core, 'strings source preview must expose a testable mapping core script');

const context = { window: {} };
vm.runInNewContext(core[1], context);
const { mapTarget } = context.window.AboStringsSourcePreviewCore;

const c3 = mapTarget('C3');
assert.equal(c3.root, 'C3', 'C3 must use the cello C3 source');
assert.equal(c3.instrument, '大提琴', 'C3 uses the low-zone instrument');
assert.equal(c3.zone, '低音区 C3–B3', 'C3 belongs to the low zone');
assert.equal(c3.semitones, 0, 'C3 uses its source without pitch shift');
assert.equal(mapTarget('B3').root, 'C3', 'low range ends at B3 and keeps cello');
assert.equal(mapTarget('C4').instrument, '中提琴', 'C4 must switch to viola');
assert.equal(mapTarget('B4').root, 'C4', 'middle range ends at B4 and keeps viola');
assert.equal(mapTarget('C5').instrument, '小提琴', 'C5 must switch to violin');
assert.equal(mapTarget('B5').semitones, 11, 'high B5 is intentionally shifted from violin C5 by eleven semitones');
assert.equal(manifest.assets.length, 3, 'provenance manifest must enumerate exactly the three zoned string sources');
assert.equal(manifest.assets.find((asset) => asset.root_note === 'C3').instrument, 'Cello Section susvib');
assert.equal(manifest.assets.find((asset) => asset.root_note === 'C4').instrument, 'Viola Section susvib');
assert.equal(manifest.assets.find((asset) => asset.root_note === 'C5').instrument, 'Solo Violin Arco Vib');

console.log('strings source preview mapping tests passed');
