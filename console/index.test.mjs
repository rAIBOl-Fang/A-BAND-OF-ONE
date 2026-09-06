import assert from 'node:assert/strict';
import fs from 'node:fs';
import { TextEncoder } from 'node:util';
import { TextDecoder } from 'node:util';
import vm from 'node:vm';

const htmlPath = new URL('./index.html', import.meta.url);
const html = fs.readFileSync(htmlPath, 'utf8');

function loadCore() {
  const match = html.match(/<script data-abo-core>([\s\S]*?)<\/script>/);
  assert.ok(match, 'index.html must expose a data-abo-core script');
  const sandbox = { globalThis: {}, TextEncoder, TextDecoder, setTimeout, clearTimeout };
  vm.runInNewContext(match[1], sandbox, { filename: 'abo-console-core.js' });
  return sandbox.globalThis.AboConsoleCore;
}

const core = loadCore();

assert.equal(
  core.isSerialSelectionCancelled({ name: 'NotFoundError' }),
  true,
  'browser cancellation must be distinguishable from a serial failure'
);
assert.equal(
  core.isSerialSelectionCancelled({ name: 'NetworkError' }),
  false,
  'real serial errors must remain visible'
);

const validScore = {
  schema: 'abo.score',
  version: 1,
  id: 'red-test',
  title: '测试曲',
  bpm: 90,
  notes: [
    { n: 0, b: 0.25 },
    { n: 4, b: 1 },
    { n: 6, b: 2 }
  ]
};

const validResult = core.validateScore(validScore);
assert.equal(validResult.ok, true);
assert.deepEqual(JSON.parse(JSON.stringify(validResult.score)), validScore);
assert.deepEqual(JSON.parse(JSON.stringify(validResult.errors)), []);

for (const [name, mutate] of [
  ['schema', score => ({ ...score, schema: 'wrong' })],
  ['version', score => ({ ...score, version: 2 })],
  ['id', score => ({ ...score, id: '含中文' })],
  ['bpm', score => ({ ...score, bpm: 90.5 })],
  ['note count', score => ({ ...score, notes: [] })],
  ['n', score => ({ ...score, notes: [{ n: 7, b: 1 }] })],
  ['b', score => ({ ...score, notes: [{ n: 0, b: 0.3 }] })]
]) {
  assert.equal(core.validateScore(mutate(validScore)).ok, false, `invalid ${name} must be rejected`);
}

const crcScore = {
  schema: 'abo.score',
  version: 1,
  id: 'crc',
  title: 'A',
  bpm: 60,
  notes: [{ n: 0, b: 1 }]
};
assert.equal(core.crc32Hex(core.canonicalScoreBytes(crcScore)), 'a60bb9df');

const messages = core.buildScoreUpload(validScore, '7fa31c09');
assert.equal(messages[0].t, 'score_begin');
assert.equal(messages[0].v, 1);
assert.equal(messages[0].schema, 'abo.score');
assert.equal(messages[0].score_version, 1);
assert.equal(messages[0].tx, '7fa31c09');
assert.equal(messages.at(-1).t, 'score_commit');
assert.ok(messages.every(message => message.v === 1 && message.tx === '7fa31c09'));
assert.ok(messages.slice(1, -1).every(message => message.notes.length <= 16));

class FakeReader {
  constructor() {
    this.queue = [];
    this.pending = null;
    this.cancelled = false;
    this.released = false;
  }

  read() {
    if (this.queue.length > 0) return Promise.resolve(this.queue.shift());
    if (this.cancelled) return Promise.resolve({ value: undefined, done: true });
    return new Promise(resolve => { this.pending = resolve; });
  }

  push(value) {
    const result = { value, done: false };
    if (this.pending) {
      const resolve = this.pending;
      this.pending = null;
      resolve(result);
    } else {
      this.queue.push(result);
    }
  }

  async cancel() {
    this.cancelled = true;
    if (this.pending) {
      const resolve = this.pending;
      this.pending = null;
      resolve({ value: undefined, done: true });
    }
  }

  releaseLock() {
    this.released = true;
  }
}

class FakeWriter {
  constructor(onWrite) {
    this.onWrite = onWrite;
    this.writes = [];
    this.released = false;
  }

  async write(value) {
    this.writes.push(new TextDecoder().decode(value));
    await this.onWrite(JSON.parse(this.writes.at(-1)));
  }

  releaseLock() {
    this.released = true;
  }
}

function createFakeSerial(onMessage) {
  const reader = new FakeReader();
  let writer;
  const port = {
    readable: { getReader: () => reader },
    writable: { getWriter: () => writer },
    writer: null,
    opened: false,
    closed: false,
    async open(options) { this.opened = options; },
    async close() { this.closed = true; }
  };
  writer = new FakeWriter(async message => onMessage(message, reader));
  port.writer = writer;
  return {
    port,
    reader,
    requested: null,
    async requestPort(options) { this.requested = options; return port; }
  };
}

const encoder = new TextEncoder();
const serialStates = [];
const serialMessages = [];
const serial = createFakeSerial(async (message, reader) => {
  if (message.t === 'ping') {
    const hello = '{"t":"hello","v":1,"product":"abo","protocol":1}\n{"t":"st';
    const state = 'ate","v":1}\n';
    reader.push(encoder.encode(hello));
    reader.push(encoder.encode(state));
  } else {
    reader.push(encoder.encode(JSON.stringify({ t: 'ack', v: 1, tx: message.tx, cmd: message.t }) + '\n'));
  }
});
const serialManager = core.createSerialManager({
  serial,
  handshakeTimeoutMs: 30,
  onState: value => serialStates.push(value),
  onMessage: value => serialMessages.push(value)
});

await serialManager.connect();
assert.deepEqual(JSON.parse(serial.requested.filters[0].usbVendorId), 0x303a);
assert.deepEqual(JSON.parse(JSON.stringify(serial.port.opened)), { baudRate: 115200 });
assert.deepEqual(serialStates, ['connecting', 'connected']);
assert.equal(serialMessages.some(message => message.t === 'hello'), true);
await serialManager.setMode('score');
await serialManager.uploadScore(validScore);
await serialManager.disconnect();
assert.equal(serial.reader.cancelled, true);
assert.equal(serial.reader.released, true);
assert.equal(serial.port.writer.released, true);
assert.equal(serial.port.closed, true);

const timeoutSerial = createFakeSerial(async () => {});
const timeoutManager = core.createSerialManager({
  serial: timeoutSerial,
  handshakeTimeoutMs: 10,
  transactionTimeoutMs: 10,
  onState: () => {}
});
await assert.rejects(timeoutManager.connect(), /握手超时/);
assert.equal(timeoutSerial.port.closed, true);

const mismatchSerial = createFakeSerial(async (message, reader) => {
  if (message.t === 'ping') {
    reader.push(encoder.encode('{"t":"hello","v":1,"product":"abo","protocol":1}\n{"t":"state","v":1}\n'));
  } else {
    reader.push(encoder.encode(JSON.stringify({ t: 'ack', v: 1, tx: 'deadbeef', cmd: message.t }) + '\n'));
  }
});
const mismatchManager = core.createSerialManager({
  serial: mismatchSerial,
  handshakeTimeoutMs: 30,
  transactionTimeoutMs: 10,
  onState: () => {}
});
await mismatchManager.connect();
await assert.rejects(mismatchManager.uploadScore(validScore), /回执超时/);
await mismatchManager.disconnect();

assert.equal(/item\.innerHTML\s*=\s*`[\s\S]*\$\{score\.title\}/.test(html), false, 'imported score titles must not be interpolated into innerHTML');
const connectSection = html.slice(html.indexOf('/* ---------- Web Serial 连接 ---------- */'));
assert.match(
  connectSection,
  /await SerialManager\.connect\(\);\s*showError\(''\);/,
  'a successful connection must clear a stale global error'
);

/*
 * P3 Task 4 RED contract.
 *
 * These assertions deliberately describe the web/device behavior that is
 * still missing.  Keep them here before changing index.html so the first run
 * records a real, reproducible failure rather than treating a visual change
 * as an implementation proof.
 */
const redFailures = [];
const redRequire = (condition, message) => {
  if (!condition) redFailures.push(message);
};

const helpMatch = html.match(/<[^>]*id=["']ai-score-help["'][^>]*>[\s\S]*?<\/aside>/i);
const helpHtml = helpMatch ? helpMatch[0] : '';
const helpText = helpHtml.replace(/<[^>]+>/g, '');
redRequire(Boolean(helpMatch), '导入区必须存在稳定的 #ai-score-help 帮助卡');
redRequire(/只输出\s*JSON/.test(helpHtml), '#ai-score-help 必须包含“只输出 JSON”');
redRequire(/UTF-8/i.test(helpHtml), '#ai-score-help 必须说明 UTF-8 文件');
redRequire(/人工核对/.test(helpHtml), '#ai-score-help 必须提醒导入前人工核对');
redRequire(!/fetch\s*\(|XMLHttpRequest|api[_-]?key/i.test(helpHtml), '#ai-score-help 不得调用网络或收集 API Key');
redRequire(/只识别主旋律/.test(helpText), 'AI 提示词必须声明只识别主旋律');
redRequire(/只取高音谱表/.test(helpText), 'AI 提示词必须声明钢琴大谱表只取高音谱表');
redRequire(/忽略低音伴奏、和弦及伴奏升降号/.test(helpText), 'AI 提示词必须声明忽略低音伴奏、和弦及伴奏升降号');
redRequire(/schema 固定为\s*["“]abo\.score["”]/.test(helpText), 'AI 提示词必须固定 schema 为 abo.score');
redRequire(/version 为 1/.test(helpText), 'AI 提示词必须固定 version 为 1');
redRequire(/id 使用 ASCII/.test(helpText), 'AI 提示词必须要求 id 使用 ASCII');
redRequire(/bpm 为 60[–-]180 的整数/.test(helpText), 'AI 提示词必须声明 BPM 范围');
redRequire(/四分音符为 1/.test(helpText) && /0\.25 的倍数/.test(helpText), 'AI 提示词必须声明音符拍长规则');
redRequire(/未标注 BPM 时使用 90/.test(helpText), 'AI 提示词必须声明未标注 BPM 时使用 90');
redRequire(/删除休止的空拍/.test(helpText), 'AI 提示词必须声明删除休止的空拍');
redRequire(/主旋律升降音或节拍不确定/.test(helpText), 'AI 提示词必须要求确认主旋律升降音和不确定节拍');
redRequire(/不要 Markdown、解释或额外字段/.test(helpText), 'AI 提示词必须禁止 Markdown、解释和额外字段');
redRequire(!/遇到无法辨认、和弦、升降音或休止符时列出并确认/.test(helpText), 'AI 提示词不得保留旧识别规则');
const libraryPosition = html.indexOf('id="library-list"');
const helpPosition = html.indexOf('id="ai-score-help"');
redRequire(libraryPosition >= 0 && helpPosition >= 0 && libraryPosition < helpPosition, '会话曲库必须位于 AI 生成说明上方');

redRequire(typeof core.validateBoardState === 'function', '协议核心必须提供 state 合法性校验入口');
if (typeof core.validateBoardState === 'function') {
  const validBoardState = {
    t: 'state', v: 1, mode: 'score', phase: 'playing',
    subphase: 'holding',
    score_id: 'red-test', score_crc32: '7fa31c09',
    cursor: 1, note_ticks: 12, note_total_ticks: 96
  };
  redRequire(core.validateBoardState(validBoardState).ok === true, '合法 state 应通过校验');
  redRequire(core.validateBoardState({ ...validBoardState, note_ticks: 97 }).ok === false, '越界 note_ticks 必须被拒绝');
  redRequire(core.validateBoardState({ ...validBoardState, note_total_ticks: '96' }).ok === false, '错误类型的 state 字段必须被拒绝');
  redRequire(core.validateBoardState({ ...validBoardState, subphase: 'bad' }).ok === false, '错误 subphase 必须被拒绝');
}

redRequire(typeof core.computeScoreVisualPosition === 'function', '协议核心必须提供谱面视觉位置计算入口');
redRequire(typeof core.shouldResetScoreViewport === 'function', '协议核心必须提供谱面视口复位判定入口');
if (typeof core.computeScoreVisualPosition === 'function') {
  const position = core.computeScoreVisualPosition({
    viewportWidth: 1000,
    paddingLeft: 20,
    noteCenters: [90, 142, 212],
    cursor: 1,
    ratio: 0.5,
    endX: 250
  });
  assert.equal(position.cursorX, 177, '活动音符必须插值到下一音符中心');
  assert.equal(position.translateX, 303, '活动音符必须围绕中央固定指针计算谱带位移');
}
if (typeof core.shouldResetScoreViewport === 'function') {
  assert.equal(
    core.shouldResetScoreViewport({ previousPhase: 'finished', previousCursor: 42, nextPhase: 'standby', nextCursor: 0 }),
    true,
    '权威 standby + cursor=0 必须复位视口'
  );
  assert.equal(
    core.shouldResetScoreViewport({ previousPhase: 'playing', previousCursor: 12, nextPhase: 'standby', nextCursor: 12 }),
    false,
    '暂停到 standby 且游标未归零不得丢失当前位置'
  );
}

const volumeMessages = [];
const volumeBoardMessages = [];
const volumeSerial = createFakeSerial(async (message, reader) => {
  volumeMessages.push(message);
  if (message.t === 'ping') {
    reader.push(encoder.encode('{"t":"hello","v":1,"product":"abo","protocol":1}\n{"t":"state","v":1,"phase":"standby","volume":70}\n'));
  } else if (message.t === 'set_volume') {
    reader.push(encoder.encode(JSON.stringify({ t: 'ack', v: 1, tx: message.tx, cmd: 'set_volume' }) + '\n'));
    reader.push(encoder.encode(JSON.stringify({ t: 'state', v: 1, phase: 'standby', volume: message.value }) + '\n'));
  }
});
const volumeManager = core.createSerialManager({
  serial: volumeSerial,
  handshakeTimeoutMs: 30,
  transactionTimeoutMs: 30,
  onMessage: message => volumeBoardMessages.push(message),
  onState: () => {}
});
redRequire(typeof volumeManager.setVolume === 'function', 'SerialManager 必须提供 setVolume(value)');
if (typeof volumeManager.setVolume === 'function') {
  await volumeManager.connect();
  await volumeManager.setVolume(42);
  const ackIndex = volumeBoardMessages.findIndex(message => message.t === 'ack' && message.cmd === 'set_volume');
  const stateIndex = volumeBoardMessages.findIndex(message => message.t === 'state' && message.volume === 42);
  redRequire(ackIndex >= 0 && stateIndex > ackIndex, 'setVolume 必须等待 matching ACK 后接收 state 确认');
  await volumeManager.disconnect();
}

function createManualClock() {
  let now = 0;
  let nextId = 1;
  const timers = new Map();
  return {
    setTimeout(fn, delay) {
      const id = nextId++;
      timers.set(id, { at: now + delay, fn });
      return id;
    },
    clearTimeout(id) {
      timers.delete(id);
    },
    advance(milliseconds) {
      now += milliseconds;
      while (true) {
        const due = [...timers.entries()]
          .filter(([, timer]) => timer.at <= now)
          .sort((a, b) => a[1].at - b[1].at);
        if (due.length === 0) break;
        for (const [id, timer] of due) {
          if (!timers.has(id)) continue;
          timers.delete(id);
          timer.fn();
        }
      }
    }
  };
}

redRequire(typeof core.createVolumeCoordinator === 'function', '网页核心必须提供可测试的音量协调器');
if (typeof core.createVolumeCoordinator === 'function') {
  const staleClock = createManualClock();
  const staleSent = [];
  const staleVolume = core.createVolumeCoordinator({
    initialVolume: 70,
    debounceMs: 80,
    setTimeout: staleClock.setTimeout,
    clearTimeout: staleClock.clearTimeout,
    sendVolume: async value => { staleSent.push(value); }
  });
  staleVolume.input(0);
  staleClock.advance(20);
  staleVolume.onBoardState(70);
  redRequire(staleVolume.snapshot().value === 0, '旧 state=70 不得覆盖用户目标 0');
  redRequire(staleVolume.snapshot().desired === 0, '旧 state=70 不得改写 desired=0');
  staleClock.advance(60);
  await Promise.resolve();
  redRequire(staleSent[0] === 0, '防抖结束必须发送用户目标 0');

  const inflightClock = createManualClock();
  const inflightSent = [];
  let finishFirstRequest;
  const firstRequest = new Promise(resolve => { finishFirstRequest = resolve; });
  const inflightVolume = core.createVolumeCoordinator({
    initialVolume: 70,
    debounceMs: 80,
    setTimeout: inflightClock.setTimeout,
    clearTimeout: inflightClock.clearTimeout,
    sendVolume: async value => {
      inflightSent.push(value);
      if (value === 0) await firstRequest;
    }
  });
  inflightVolume.input(0);
  inflightClock.advance(80);
  await Promise.resolve();
  inflightVolume.input(30);
  inflightClock.advance(80);
  await Promise.resolve();
  finishFirstRequest();
  await new Promise(resolve => setTimeout(resolve, 0));
  redRequire(inflightSent.join(',') === '0,30', '在途请求结束后必须继续发送最新音量目标');

  const errorClock = createManualClock();
  let volumeError = null;
  const failedVolume = core.createVolumeCoordinator({
    initialVolume: 70,
    debounceMs: 80,
    setTimeout: errorClock.setTimeout,
    clearTimeout: errorClock.clearTimeout,
    sendVolume: async () => { throw new Error('volume_failed'); },
    onError: error => { volumeError = error; }
  });
  failedVolume.input(0);
  errorClock.advance(80);
  await new Promise(resolve => setTimeout(resolve, 0));
  redRequire(volumeError && volumeError.message === 'volume_failed', '音量下发失败必须通知网页错误提示');
  redRequire(failedVolume.snapshot().value === 70, '音量下发失败必须回退到已确认值');
}

let volumeStateReader;
const ackOnlySerial = createFakeSerial(async (message, reader) => {
  volumeStateReader = reader;
  if (message.t === 'ping') {
    reader.push(encoder.encode('{"t":"hello","v":1,"product":"abo","protocol":1}\n{"t":"state","v":1,"phase":"standby","volume":70}\n'));
  } else if (message.t === 'set_volume') {
    reader.push(encoder.encode(JSON.stringify({ t: 'ack', v: 1, tx: message.tx, cmd: 'set_volume' }) + '\n'));
  }
});
const ackOnlyManager = core.createSerialManager({
  serial: ackOnlySerial,
  handshakeTimeoutMs: 30,
  transactionTimeoutMs: 30,
  onState: () => {}
});
await ackOnlyManager.connect();
let volumeSettled = false;
const volumeWait = ackOnlyManager.setVolume(42).then(() => { volumeSettled = true; });
await new Promise(resolve => setTimeout(resolve, 5));
redRequire(volumeSettled === false, 'setVolume 只收到 ACK 时不得提前完成');
volumeStateReader.push(encoder.encode('{"t":"state","v":1,"phase":"standby","volume":42}\n'));
await volumeWait;
redRequire(volumeSettled === true, 'setVolume 收到匹配 state 后才算完成');
await ackOnlyManager.disconnect();

const s9Views = (html.match(/data-control=["']s9["']/gi) || []).length;
redRequire(s9Views >= 2, '跟谱与自由视图必须各有一个 data-control="s9" 旋钮视图');
redRequire((html.match(/class=["']encoder-control["']/gi) || []).length >= 2, '两处旋钮必须使用独立的 encoder-control 布局容器');
redRequire(/class=["']knob-state["']/.test(html), '旋钮动态状态必须独立成行');
redRequire(/translateY\(-52px\)/.test(html), '旋钮刻度必须贴近外圈，不能穿过 S9');
redRequire(/case\s+["']input["']|msg\.t\s*===\s*["']input["']/.test(html), '网页必须消费板端 input 事件');
redRequire(/input[\s\S]{0,300}s9[\s\S]{0,300}pressed/i.test(html), '真实 s9 pressed 必须触发按压反馈');
redRequire(/s9[\s\S]{0,300}(pressed|按压)[\s\S]{0,300}(black|黑)/i.test(html), 'S9 pressed 反馈必须显示黑色与按压文案');
redRequire(/120\s*ms|120ms/.test(html), 'S9 真实按压反馈必须保证至少 120ms 可见');

redRequire(/id=["']completion-modal["']/i.test(html), '曲终必须使用页面内 completion modal');
redRequire(/id=["']completion-modal["'][\s\S]{0,500}role=["']dialog["']/i.test(html), '曲终 modal 必须声明 dialog 语义');
redRequire(!/\balert\s*\(/.test(html), '曲终不得继续使用原生 alert()');
redRequire(/按任意键返回待机/.test(html), '曲终提示必须说明按任意键返回待机');
redRequire(/transport[\s\S]{0,200}reset|reset[\s\S]{0,200}transport/.test(html), '曲终电脑键/鼠标确认必须发送 transport reset');
redRequire(/addEventListener\(["']keydown["']/.test(html), '曲终必须支持电脑键关闭/确认');

const defaultScoreMatch = html.match(/const DEFAULT_SCORE = \{[\s\S]*?\n      \};/);
const defaultScoreNotes = defaultScoreMatch ? (defaultScoreMatch[0].match(/\{\s*n\s*:/g) || []).length : 0;
redRequire(defaultScoreNotes === 42, `内置《小星星》必须为完整 42 音（当前检测到 ${defaultScoreNotes} 音）`);
redRequire(/function\s+renderScore\s*\(/.test(html), '内置谱与导入谱必须共用 renderScore 渲染器');
redRequire(/note\.b[\s\S]{0,160}(width|flexBasis)|(?:width|flexBasis)[\s\S]{0,160}note\.b/.test(html), '谱面布局宽度必须按 note.b 拍数计算');
redRequire(/score-stage[\s\S]{0,260}overflow\s*:\s*hidden/i.test(html), '谱面容器必须隐藏溢出并由活动谱带移动');
redRequire(/id=["']score-playhead["']/i.test(html), '谱面区必须存在唯一的中央固定指针');
redRequire(/score-playhead[\s\S]{0,500}(?:left\s*:\s*50%|transform|position\s*:\s*absolute)/i.test(html), '中央指针必须固定在谱面容器中');
redRequire((html.match(/class=["'][^"']*score-track/gi) || []).length >= 2, '五线谱与简谱都必须使用独立活动谱带');
redRequire(/translate3d\s*\(/.test(html), '活动谱带必须使用 translate3d 平滑移动');
redRequire(/function\s+renderScoreMotion\s*\(/.test(html), '谱面必须通过 renderScoreMotion 统一更新');
redRequire(/state\.notation[\s\S]{0,700}(?:staff|jianpu)[\s\S]{0,700}score-track/i.test(html), '谱面几何必须按当前记谱法选择活动轨道');
redRequire(/scrollLeft\s*=\s*0/.test(html), '权威复位必须清除旧谱面视口位置');
redRequire(/note_ticks|note_total_ticks/.test(html), '网页必须消费板端音符内拍长进度');
redRequire(/phase[\s\S]{0,220}(waiting|standby|holding)/i.test(html), '游标必须按板端 phase 冻结或前进');
redRequire(!/setInterval[\s\S]{0,300}(?:cursor|noteTicks|score)/i.test(html), '网页不得用定时器推进真实谱面游标');

const boardMessageSection = html.slice(
  html.indexOf('function handleBoardMessage'),
  html.indexOf('const SerialManager')
);
const inputCase = boardMessageSection.match(/case ['"]input['"]:([\s\S]*?)case ['"]judge['"]:/);
redRequire(Boolean(inputCase), '板端 input 分支必须保持可定位的处理边界');
redRequire(Boolean(inputCase && !/requestCompletionReset\s*\(/.test(inputCase[1])), '实体曲终 input 不得重复发送 transport reset');
redRequire(/function\s+handleCompletionInput\s*\(\s*msg\s*\)/.test(html), '曲终实体 input 必须经过独立处理函数');
redRequire(/function\s+handleCompletionResult\s*\(\s*msg\s*\)/.test(html), '曲终 result 必须经过独立处理函数');
redRequire(/state\.completionPending\s*&&\s*msg\.phase\s*===\s*['"]standby['"]/.test(boardMessageSection), '收到 standby 必须关闭曲终完成状态，不得依赖 resetRequested');
redRequire(/handleCompletionResult\s*\(\s*msg\s*\)/.test(boardMessageSection), 'result 分支必须处理 result/state 到达顺序竞态');
redRequire(/handleCompletionInput\s*\(\s*msg\s*\)/.test(boardMessageSection), 'input 分支必须消费实体曲终按键而不是再次复位');
redRequire(/state\.phase\s*===\s*['"]standby['"][\s\S]{0,240}hideCompletionModal\s*\(\s*\)/.test(html), '已处于 standby 时收到 result 不得重新弹出完成卡片');

redRequire(/set_volume/.test(html), '网页必须发送 set_volume 指令');
redRequire(/80\s*ms|80ms/.test(html), '音量滑块必须使用 80ms 防抖');
redRequire(/pending[\s\S]{0,240}volume|volume[\s\S]{0,240}pending/i.test(html), '音量必须有 pending 请求与断线清理状态');
redRequire(/msg\.volume/.test(html), '网页必须以板端 state.volume 作为音量确认值');
redRequire(/\.abo\.score\.json/.test(html), '文件选择必须明确支持 .abo.score.json');
redRequire(/class=["']ai-step-desc["']/.test(html), 'AI 帮助卡必须将使用说明单独分段');
redRequire(/class=["']ai-prompt-text["']/.test(html), 'AI 提示词必须有独立的可识别提示区域');
redRequire(/class=["']ai-score-help-footnote["']/.test(html), 'AI 帮助卡必须将保存与人工核对说明单独分段');
redRequire(/class=["']ai-help-steps["']/.test(html), 'A 方案帮助卡必须使用步骤容器');
redRequire((html.match(/class=["']ai-help-step["']/gi) || []).length === 3, 'A 方案帮助卡必须包含 3 个编号步骤');
redRequire(/class=["']ai-prompt-block["']/.test(html), 'A 方案必须使用深色提示词块');
redRequire(/id=["']ai-prompt-copy["']/.test(html) && /clipboard\.writeText/.test(html), 'AI 提示词块必须提供复制操作');

if (redFailures.length > 0) {
  console.error(`P3 Task 4 RED: ${redFailures.length} web contracts are not implemented`);
  for (const failure of redFailures) console.error(`- ${failure}`);
  process.exitCode = 1;
} else {
  console.log('P3 Task 4 web contracts passed');
}
