import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const pageUrl = new URL('./diagnose-clarinet-hil.html', import.meta.url);
assert.ok(existsSync(pageUrl), '单簧管真机问题隔离页尚未实现');

const html = await readFile(pageUrl, 'utf8');
const coreMatch = html.match(
  /<script id="abo-clarinet-hil-core">([\s\S]*?)<\/script>/,
);
assert.ok(coreMatch, '页面必须暴露可独立测试的单簧管诊断核心');

const context = { window: {} };
vm.createContext(context);
vm.runInContext(coreMatch[1], context);

const {
  ROOTS,
  FORMAL_MANIFEST_URL,
  ACCEPTED_MANIFEST_URL,
  buildRootCases,
  makePlaybackPlan,
  decodeEiad,
  decodePcm16le,
} = context.window.AboClarinetHilCore;

assert.deepEqual(
  Array.from(ROOTS, (root) => root.id),
  [
    'clarinet_d3', 'clarinet_f3', 'clarinet_as3',
    'clarinet_d4', 'clarinet_f4', 'clarinet_as4',
    'clarinet_d5', 'clarinet_f5', 'clarinet_as5',
  ],
  '诊断必须覆盖低、中、高三个音区的全部九枚根音',
);
assert.equal(
  FORMAL_MANIFEST_URL,
  '../firmware/course-motherboard/abo_assets/delivery/manifest.json',
);
assert.equal(
  ACCEPTED_MANIFEST_URL,
  '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/manifest.json',
);

const formalManifest = JSON.parse(await readFile(new URL(
  '../firmware/course-motherboard/abo_assets/delivery/manifest.json',
  import.meta.url,
)));
const acceptedManifest = JSON.parse(await readFile(new URL(
  '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/manifest.json',
  import.meta.url,
)));
const cases = buildRootCases(formalManifest, acceptedManifest);
assert.equal(cases.length, 9);

const d3 = cases[0];
assert.equal(d3.id, 'clarinet_d3');
assert.equal(d3.note, 'D3');
assert.equal(d3.rawUrl, '../firmware/course-motherboard/abo_assets/raw/clarinet-tonejs/clarinet_d3.wav');
assert.equal(d3.formal.url, '../firmware/course-motherboard/abo_assets/delivery/clarinet_d3.pcm16le');
assert.equal(d3.formal.sourceType, 'pcm16le');
assert.equal(d3.formal.totalSeconds, 0.8);
assert.equal(d3.formal.loopStart, 0.42);
assert.equal(d3.formal.loopEnd, 0.583);
assert.equal(d3.accepted.url, '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/clarinet_d3.eiad');
assert.equal(d3.accepted.totalSeconds, 0.8);
assert.equal(d3.accepted.loopStart, 0.42);
assert.equal(d3.accepted.loopEnd, 0.583);

assert.deepEqual(
  { ...makePlaybackPlan('raw_single', d3) },
  {
    sourceType: 'wav',
    url: d3.rawUrl,
    loop: false,
    loopStart: 0,
    loopEnd: 0,
  },
);
assert.deepEqual(
  { ...makePlaybackPlan('raw_formal_loop', d3) },
  {
    sourceType: 'wav',
    url: d3.rawUrl,
    loop: true,
    loopStart: 0.42,
    loopEnd: 0.583,
  },
);
assert.deepEqual(
  { ...makePlaybackPlan('formal_pcm', d3) },
  {
    sourceType: 'pcm16le',
    url: d3.formal.url,
    loop: true,
    loopStart: 0.42,
    loopEnd: 0.583,
  },
);
assert.deepEqual(
  { ...makePlaybackPlan('accepted_eiad', d3) },
  {
    sourceType: 'eiad',
    url: d3.accepted.url,
    loop: true,
    loopStart: 0.42,
    loopEnd: 0.583,
  },
);
assert.throws(
  () => makePlaybackPlan('unknown', d3),
  /unknown playback mode/,
);

const formalBytes = await readFile(new URL(
  '../firmware/course-motherboard/abo_assets/delivery/clarinet_d3.pcm16le',
  import.meta.url,
));
const formalDecoded = decodePcm16le(
  formalBytes.buffer.slice(
    formalBytes.byteOffset,
    formalBytes.byteOffset + formalBytes.byteLength,
  ),
);
assert.equal(formalDecoded.sampleRate, 48000);
assert.equal(formalDecoded.samples.length, 38400);

const acceptedBytes = await readFile(new URL(
  '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/clarinet_d3.eiad',
  import.meta.url,
));
const acceptedDecoded = decodeEiad(
  acceptedBytes.buffer.slice(
    acceptedBytes.byteOffset,
    acceptedBytes.byteOffset + acceptedBytes.byteLength,
  ),
);
assert.equal(acceptedDecoded.sampleRate, 48000);
assert.equal(acceptedDecoded.samples.length, 38400);

console.log('clarinet HIL isolation page tests passed');
