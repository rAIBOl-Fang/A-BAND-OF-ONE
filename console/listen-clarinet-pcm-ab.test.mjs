import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const pageUrl = new URL('./listen-clarinet-pcm-ab.html', import.meta.url);
assert.ok(existsSync(pageUrl), 'PCM A/B 页面尚未实现');

const html = await readFile(pageUrl, 'utf8');
const coreMatch = html.match(
  /<script id="abo-clarinet-pcm-ab-core">([\s\S]*?)<\/script>/,
);
assert.ok(coreMatch, 'PCM A/B 页面必须暴露可独立测试的核心');

const context = { window: {} };
vm.createContext(context);
vm.runInContext(coreMatch[1], context);

const core = context.window.AboClarinetPcmAbCore;
assert.ok(core, 'PCM A/B 核心未导出');
assert.deepEqual(
  Array.from(core.ROOTS, (root) => root.id),
  [
    'clarinet_d3', 'clarinet_f3', 'clarinet_as3',
    'clarinet_d4', 'clarinet_f4', 'clarinet_as4',
    'clarinet_d5', 'clarinet_f5', 'clarinet_as5',
  ],
);
assert.deepEqual(
  Array.from(core.MODES, (mode) => mode.id),
  ['pcm_single', 'pcm_loop', 'eiad_single', 'eiad_loop'],
);
assert.equal(
  core.PCM_MANIFEST_URL,
  '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-pcm-preview/manifest.json',
);
assert.equal(
  core.ACCEPTED_MANIFEST_URL,
  '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/manifest.json',
);
assert.equal(html.includes('clarinet-tonejs-high-ab-preview'), false);

const pcmManifest = JSON.parse(await readFile(new URL(
  '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-pcm-preview/manifest.json',
  import.meta.url,
)));
const acceptedManifest = JSON.parse(await readFile(new URL(
  '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/manifest.json',
  import.meta.url,
)));

const d3 = core.ROOTS[0];
assert.deepEqual(
  { ...core.makePlaybackPlan('pcm_single', d3, pcmManifest, acceptedManifest) },
  {
    sourceType: 'wav',
    url: '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-pcm-preview/clarinet_d3.pcm.wav',
    loop: false,
    loopStart: 0,
    loopEnd: 0,
  },
);
assert.deepEqual(
  { ...core.makePlaybackPlan('pcm_loop', d3, pcmManifest, acceptedManifest) },
  {
    sourceType: 'wav',
    url: '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-pcm-preview/clarinet_d3.pcm.wav',
    loop: true,
    loopStart: 0.42,
    loopEnd: 0.583,
  },
);
assert.deepEqual(
  { ...core.makePlaybackPlan('eiad_single', d3, pcmManifest, acceptedManifest) },
  {
    sourceType: 'eiad',
    url: '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/clarinet_d3.eiad',
    loop: false,
    loopStart: 0,
    loopEnd: 0,
  },
);
assert.deepEqual(
  { ...core.makePlaybackPlan('eiad_loop', d3, pcmManifest, acceptedManifest) },
  {
    sourceType: 'eiad',
    url: '../firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/clarinet_d3.eiad',
    loop: true,
    loopStart: 0.42,
    loopEnd: 0.583,
  },
);
assert.throws(
  () => core.makePlaybackPlan('unknown', d3, pcmManifest, acceptedManifest),
  /unknown playback mode/,
);

console.log('clarinet PCM A/B page tests passed');
