(function () {
  'use strict';

  if (window.mermaid) {
    window.mermaid.initialize({ startOnLoad: true, theme: 'neutral', securityLevel: 'loose' });
  }

  var NOTE_NAMES = ['do', 're', 'mi', 'fa', 'sol', 'la', 'si'];
  var INST_NAMES = ['钢琴', '弦乐', '单簧管'];
  var BASE_FREQ = [261.63, 293.66, 329.63, 349.23, 392.0, 440.0, 493.88];
  var OCT_FACTOR = [0.5, 1, 2];

  // 小星星：n = 唱名索引 0-6，b = 拍数
  var SCORE = [
    { n: 0, b: 1 }, { n: 0, b: 1 }, { n: 4, b: 1 }, { n: 4, b: 1 },
    { n: 5, b: 1 }, { n: 5, b: 1 }, { n: 4, b: 2 },
    { n: 3, b: 1 }, { n: 3, b: 1 }, { n: 2, b: 1 }, { n: 2, b: 1 },
    { n: 1, b: 1 }, { n: 1, b: 1 }, { n: 0, b: 2 }
  ];

  var S = {
    phase: 'standby',
    mode: 'score',
    inst: 0,
    oct: 1,
    bpm: 90,
    notesDone: 0,
    errors: 0,
    elapsed: 0,
    since: 0,
    holdTimer: null,
    holdKey: -1,
    voices: {},
    knobAngle: 0,
    tick: null,
    breathe: null,
    celeb: null
  };

  var $ = function (id) { return document.getElementById(id); };
  var pkRow = $('pkRow'), pkKnob = $('pkKnob'), pkKnobRot = $('pkKnobRot'),
      pkLeds = $('pkLeds'), pkSheet = $('pkSheet'), psMode = $('psMode'),
      psInst = $('psInst'), psOct = $('psOct'), psBpm = $('psBpm'),
      psBpmVal = $('psBpmVal'), stProg = $('stProg'), stErr = $('stErr'),
      stTime = $('stTime'), psStatus = $('psStatus'), psReset = $('psReset'),
      pkKnobCap = $('pkKnobCap'), psModeHint = $('psModeHint');

  var keys = [], leds = [], blocks = [];

  /* ---------- 声音：Web Audio 三音色示意 ---------- */
  var AC = window.AudioContext || window.webkitAudioContext;
  var actx = null, master = null;

  function ensureCtx() {
    if (!AC) return null;
    if (!actx) {
      actx = new AC();
      master = actx.createGain();
      master.gain.value = 0.55;
      var comp = actx.createDynamicsCompressor();
      master.connect(comp);
      comp.connect(actx.destination);
    }
    if (actx.state === 'suspended') actx.resume();
    return actx;
  }

  function makeVoice(inst, f) {
    var ctx = ensureCtx();
    if (!ctx) return { stop: function () {} };
    var t = ctx.currentTime;
    var out = ctx.createGain();
    out.gain.value = 0;
    out.connect(master);
    var nodes = [];
    function addOsc(type, freq, gain, dest) {
      var o = ctx.createOscillator();
      o.type = type;
      o.frequency.value = freq;
      var g = ctx.createGain();
      g.gain.value = gain;
      o.connect(g);
      g.connect(dest);
      o.start(t);
      nodes.push(o);
      return o;
    }
    if (inst === 0) {
      // 钢琴：谐波叠加 + 快起缓衰
      addOsc('sine', f, 0.55, out);
      addOsc('sine', f * 2, 0.2, out);
      addOsc('sine', f * 3, 0.07, out);
      out.gain.setValueAtTime(0, t);
      out.gain.linearRampToValueAtTime(0.85, t + 0.008);
      out.gain.exponentialRampToValueAtTime(0.3, t + 0.4);
    } else if (inst === 1) {
      // 弦乐：双锯齿 + 低通 + 颤音
      var filt = ctx.createBiquadFilter();
      filt.type = 'lowpass';
      filt.frequency.value = 2400;
      filt.Q.value = 0.6;
      var inner = ctx.createGain();
      inner.gain.value = 1;
      filt.connect(inner);
      inner.connect(out);
      var o1 = addOsc('sawtooth', f, 0.45, filt);
      var o2 = addOsc('sawtooth', f * 1.007, 0.45, filt);
      var lfo = ctx.createOscillator();
      lfo.frequency.value = 5.2;
      var lg = ctx.createGain();
      lg.gain.value = 4;
      lfo.connect(lg);
      lg.connect(o1.frequency);
      lg.connect(o2.frequency);
      lfo.start(t);
      nodes.push(lfo);
      out.gain.setValueAtTime(0, t);
      out.gain.linearRampToValueAtTime(0.5, t + 0.16);
    } else {
      // 单簧管：原型仅作交互占位；正式版替换为可追溯单簧管采样
      addOsc('sine', f, 0.72, out);
      addOsc('sine', f * 2, 0.1, out);
      out.gain.setValueAtTime(0, t);
      out.gain.linearRampToValueAtTime(0.68, t + 0.07);
    }
    var stopped = false;
    return {
      stop: function () {
        if (stopped) return;
        stopped = true;
        var rt = ctx.currentTime;
        var rel = inst === 0 ? 0.3 : (inst === 1 ? 0.4 : 0.28);
        var cur = Math.max(out.gain.value, 0.0012);
        out.gain.cancelScheduledValues(rt);
        out.gain.setValueAtTime(cur, rt);
        out.gain.exponentialRampToValueAtTime(0.0008, rt + rel);
        nodes.forEach(function (o) {
          try { o.stop(rt + rel + 0.06); } catch (e) {}
        });
      }
    };
  }

  function noteOn(idx) {
    ensureCtx();
    if (S.voices[idx]) S.voices[idx].stop();
    S.voices[idx] = makeVoice(S.inst, BASE_FREQ[idx] * OCT_FACTOR[S.oct]);
  }
  function noteOff(idx) {
    if (S.voices[idx]) {
      S.voices[idx].stop();
      delete S.voices[idx];
    }
  }
  function stopAllVoices() {
    Object.keys(S.voices).forEach(function (k) { S.voices[k].stop(); });
    S.voices = {};
  }

  function arpeggio() {
    var seq = [[0, 0], [2, 0], [4, 0], [0, 1]];
    seq.forEach(function (s, i) {
      setTimeout(function () {
        var v = makeVoice(0, BASE_FREQ[s[0]] * OCT_FACTOR[S.oct] * (s[1] ? 2 : 1));
        setTimeout(function () { v.stop(); }, 550);
      }, i * 110);
    });
  }

  /* ---------- DOM 构建 ---------- */
  function buildKeys() {
    for (var i = 0; i < 8; i++) {
      var b = document.createElement('button');
      b.type = 'button';
      b.className = 'pk' + (i === 7 ? ' s8' : '');
      if (i === 7) {
        b.innerHTML = '<b>' + INST_NAMES[S.inst] + '</b><span>S8 · 乐器</span>';
      } else {
        b.innerHTML = '<b>' + NOTE_NAMES[i] + '</b><span>S' + (i + 1) + '</span>';
      }
      (function (idx) {
        b.addEventListener('pointerdown', function (e) {
          e.preventDefault();
          if (idx === 7) { cycleInst(); return; }
          keyDown(idx);
        });
        b.addEventListener('pointerup', function () { keyUp(idx); });
        b.addEventListener('pointerleave', function () { keyUp(idx); });
      })(i);
      pkRow.appendChild(b);
      keys.push(b);
    }
    for (var j = 0; j < 5; j++) {
      var d = document.createElement('div');
      d.className = 'pk-led';
      pkLeds.appendChild(d);
      leds.push(d);
    }
    SCORE.forEach(function (nt) {
      var s = document.createElement('div');
      s.className = 'sb';
      s.style.width = (26 + nt.b * 20) + 'px';
      s.textContent = NOTE_NAMES[nt.n];
      pkSheet.appendChild(s);
      blocks.push(s);
    });
  }

  /* ---------- UI 同步 ---------- */
  function setStatus(t) { psStatus.textContent = t; }

  function updateModeUI() {
    var btns = psMode.querySelectorAll('button');
    btns.forEach(function (b) {
      b.classList.toggle('on', b.dataset.mode === S.mode);
    });
    psMode.classList.toggle('lock', S.phase === 'playing' || S.phase === 'paused');
    psModeHint.textContent = (S.phase === 'playing' || S.phase === 'paused')
      ? '演奏中锁定'
      : '旋钮 / 点选切换';
  }

  function updateInstUI() {
    var pills = psInst.children;
    for (var i = 0; i < pills.length; i++) {
      pills[i].classList.toggle('on', i === S.inst);
    }
    keys[7].querySelector('b').textContent = INST_NAMES[S.inst];
  }

  function updateOctUI() {
    var pills = psOct.children;
    for (var i = 0; i < pills.length; i++) {
      pills[i].classList.toggle('on', i === S.oct);
    }
  }

  function updateKnobCaption() {
    var cap = {
      standby: '按压 · 开始弹奏　|　滚轮 · 切换模式',
      playing: '按压 · 暂停　|　滚轮 · 调音区',
      paused: '按压 · 继续　|　滚轮 · 锁定',
      finished: '按压 · 回到待机　|　滚轮 · 锁定'
    };
    pkKnobCap.textContent = cap[S.phase];
  }

  function updateStats() {
    stErr.textContent = S.errors;
    stTime.textContent = (S.elapsed / 1000).toFixed(1) + 's';
    stProg.textContent = (S.mode === 'free') ? '—' : (S.notesDone + ' / ' + SCORE.length);
  }

  function updateSheetProgress() {
    blocks.forEach(function (el, i) {
      el.classList.toggle('done', i < S.notesDone);
      el.classList.toggle('cur', S.mode === 'score' && S.phase !== 'finished' && i === S.notesDone);
    });
  }

  function clearKeyClasses() {
    keys.forEach(function (k) {
      k.classList.remove('hint', 'on', 'err');
    });
  }

  /* ---------- LED 光效 ---------- */
  function clearLeds() {
    leds.forEach(function (l) {
      l.classList.remove('lit');
      l.style.background = '';
      l.style.boxShadow = '';
    });
  }

  function pulseLeds(noteIdx) {
    var hue = 258 - noteIdx * 26;
    leds.forEach(function (l, i) {
      l.style.background = 'hsl(' + hue + ', 68%, ' + (38 + i * 8) + '%)';
      l.style.boxShadow = '0 0 10px hsla(' + hue + ', 68%, 60%, 0.75)';
      l.classList.add('lit');
    });
    setTimeout(clearLeds, 320);
  }

  function startBreath() {
    if (S.breathe) return;
    S.breathe = setInterval(function () {
      leds[0].style.background = 'hsla(258, 60%, 62%, 0.45)';
      leds[0].classList.add('lit');
      setTimeout(function () {
        leds[0].classList.remove('lit');
        leds[0].style.background = '';
      }, 850);
    }, 1700);
  }
  function stopBreath() {
    if (S.breathe) { clearInterval(S.breathe); S.breathe = null; }
    clearLeds();
  }

  function celebrate() {
    var step = 0;
    S.celeb = setInterval(function () {
      var hue = step * 30;
      leds.forEach(function (l, i) {
        l.style.background = 'hsl(' + ((hue + i * 14) % 360) + ', 75%, 60%)';
        l.style.boxShadow = '0 0 12px hsla(' + ((hue + i * 14) % 360) + ', 75%, 60%, 0.8)';
        l.classList.add('lit');
      });
      step++;
      if (step >= 12) {
        clearInterval(S.celeb);
        S.celeb = null;
        setTimeout(clearLeds, 400);
      }
    }, 130);
  }

  /* ---------- 计时 ---------- */
  function startTick() {
    if (S.tick) return;
    S.tick = setInterval(function () {
      if (S.phase !== 'playing') return;
      stTime.textContent = ((S.elapsed + performance.now() - S.since) / 1000).toFixed(1) + 's';
    }, 100);
  }
  function stopTick() {
    if (S.tick) { clearInterval(S.tick); S.tick = null; }
  }

  /* ---------- 状态机 ---------- */
  function beatMs(beats) { return 60 / S.bpm * beats * 1000; }

  function applyHint() {
    var target = SCORE[S.notesDone].n;
    keys[target].classList.add('hint');
  }

  function startPlaying() {
    stopBreath();
    S.phase = 'playing';
    S.notesDone = 0;
    S.errors = 0;
    S.elapsed = 0;
    S.since = performance.now();
    clearKeyClasses();
    if (S.mode === 'score') {
      applyHint();
      updateSheetProgress();
      setStatus('跟谱弹奏 · 第 1 音 · 待按 ' + NOTE_NAMES[SCORE[0].n]);
    } else {
      updateSheetProgress();
      setStatus('自由演奏 · 按键即发声，S8 换乐器，滚轮调音区');
    }
    updateStats();
    startTick();
    updateModeUI();
    updateKnobCaption();
  }

  function judge(idx) {
    if (S.holdTimer) {
      setStatus('当前音还在响 · 等蓝色结束');
      return;
    }
    var target = SCORE[S.notesDone].n;
    if (idx === target) {
      var beats = SCORE[S.notesDone].b;
      keys[idx].classList.remove('hint');
      keys[idx].classList.add('on');
      noteOn(idx);
      pulseLeds(idx);
      S.holdKey = idx;
      S.holdTimer = setTimeout(function () {
        keys[idx].classList.remove('on');
        S.holdTimer = null;
        S.holdKey = -1;
        noteOff(idx);
        S.notesDone++;
        updateSheetProgress();
        updateStats();
        if (S.notesDone >= SCORE.length) {
          finishSong();
        } else {
          applyHint();
          setStatus('跟谱弹奏 · 第 ' + (S.notesDone + 1) + ' 音 · 待按 ' + NOTE_NAMES[SCORE[S.notesDone].n]);
        }
      }, beatMs(beats));
    } else {
      S.errors++;
      keys[idx].classList.add('err');
      setTimeout(function () { keys[idx].classList.remove('err'); }, 320);
      updateStats();
      setStatus('弹错 · 时间轴暂停 · 待按 ' + NOTE_NAMES[SCORE[S.notesDone].n]);
    }
  }

  function finishSong() {
    S.phase = 'finished';
    S.elapsed += performance.now() - S.since;
    stopTick();
    updateSheetProgress();
    updateStats();
    stProg.textContent = SCORE.length + ' / ' + SCORE.length;
    setStatus('本曲完成：弹错 ' + S.errors + ' 次 · 用时 ' + (S.elapsed / 1000).toFixed(1) + ' 秒');
    celebrate();
    arpeggio();
    updateModeUI();
    updateKnobCaption();
  }

  function pause() {
    S.phase = 'paused';
    S.elapsed += performance.now() - S.since;
    stopAllVoices();
    if (S.holdTimer) {
      clearTimeout(S.holdTimer);
      S.holdTimer = null;
    }
    if (S.holdKey >= 0) {
      keys[S.holdKey].classList.remove('on');
      S.holdKey = -1;
    }
    keys.forEach(function (k) { k.classList.remove('hint'); });
    stopTick();
    updateStats();
    updateModeUI();
    updateKnobCaption();
    setStatus('已暂停 · 按压旋钮继续（当前音将重新提示）');
  }

  function resume() {
    S.phase = 'playing';
    S.since = performance.now();
    startTick();
    if (S.mode === 'score') {
      applyHint();
      setStatus('继续 · 第 ' + (S.notesDone + 1) + ' 音 · 待按 ' + NOTE_NAMES[SCORE[S.notesDone].n]);
    } else {
      setStatus('自由演奏 · 继续随心弹');
    }
    updateKnobCaption();
  }

  function toStandby() {
    S.phase = 'standby';
    if (S.holdTimer) {
      clearTimeout(S.holdTimer);
      S.holdTimer = null;
    }
    S.holdKey = -1;
    clearKeyClasses();
    stopAllVoices();
    S.notesDone = 0;
    S.errors = 0;
    S.elapsed = 0;
    stopTick();
    if (S.celeb) { clearInterval(S.celeb); S.celeb = null; }
    updateSheetProgress();
    updateStats();
    updateModeUI();
    updateKnobCaption();
    startBreath();
    setStatus('待机 · 选择模式后按压旋钮开始');
  }

  function knobPress() {
    if (S.phase === 'standby') startPlaying();
    else if (S.phase === 'playing') pause();
    else if (S.phase === 'paused') resume();
    else toStandby();
  }

  function rotate(dir) {
    S.knobAngle += dir * 45;
    pkKnobRot.style.transform = 'rotate(' + S.knobAngle + 'deg)';
    if (S.phase === 'standby') {
      S.mode = S.mode === 'score' ? 'free' : 'score';
      updateModeUI();
      setStatus('待机 · 模式：' + (S.mode === 'score' ? '跟谱弹奏' : '自由演奏'));
    } else if (S.phase === 'playing') {
      S.oct = (S.oct + dir + 3) % 3;
      updateOctUI();
      setStatus('音区 → ' + ['低', '中', '高'][S.oct] + '（下一次按键生效）');
    } else {
      setStatus('当前状态下旋钮已锁定');
    }
  }

  function cycleInst() {
    S.inst = (S.inst + 1) % 3;
    updateInstUI();
    setStatus('乐器 → ' + INST_NAMES[S.inst] + '（下一次按键生效）');
  }

  /* ---------- 输入 ---------- */
  function keyDown(idx) {
    ensureCtx();
    if (S.phase === 'playing') {
      if (S.mode === 'score') {
        judge(idx);
      } else {
        noteOn(idx);
        keys[idx].classList.add('on');
        pulseLeds(idx);
      }
    } else if (S.phase === 'standby') {
      setStatus('待机 · 按压旋钮开始后再弹');
    } else if (S.phase === 'paused') {
      setStatus('已暂停 · 按压旋钮继续');
    } else if (S.phase === 'finished') {
      setStatus('曲终 · 按压旋钮回到待机');
    }
  }

  function keyUp(idx) {
    if (S.phase === 'playing' && S.mode === 'free') {
      noteOff(idx);
      keys[idx].classList.remove('on');
    }
  }

  pkKnob.addEventListener('click', knobPress);
  pkKnob.addEventListener('wheel', function (e) {
    e.preventDefault();
    rotate(e.deltaY < 0 ? 1 : -1);
  }, { passive: false });

  psMode.querySelectorAll('button').forEach(function (b) {
    b.addEventListener('click', function () {
      if (S.phase === 'playing' || S.phase === 'paused') {
        setStatus('演奏中 · 模式切换已锁定');
        return;
      }
      S.mode = b.dataset.mode;
      updateModeUI();
      setStatus('待机 · 模式：' + (S.mode === 'score' ? '跟谱弹奏' : '自由演奏'));
    });
  });

  Array.prototype.forEach.call(psInst.children, function (el, i) {
    el.addEventListener('click', function () {
      S.inst = i;
      updateInstUI();
      setStatus('乐器 → ' + INST_NAMES[S.inst]);
    });
  });

  Array.prototype.forEach.call(psOct.children, function (el, i) {
    el.addEventListener('click', function () {
      S.oct = i;
      updateOctUI();
      setStatus('音区 → ' + ['低', '中', '高'][S.oct]);
    });
  });

  psBpm.addEventListener('input', function () {
    S.bpm = +psBpm.value;
    psBpmVal.textContent = S.bpm;
  });

  psReset.addEventListener('click', toStandby);

  document.addEventListener('keydown', function (e) {
    if (e.repeat) return;
    if (e.code === 'Space') {
      e.preventDefault();
      ensureCtx();
      knobPress();
      return;
    }
    if (e.key >= '1' && e.key <= '8') {
      var i = +e.key - 1;
      ensureCtx();
      if (i === 7) cycleInst();
      else keyDown(i);
    }
    if (e.key === 'ArrowUp') { e.preventDefault(); rotate(1); }
    if (e.key === 'ArrowDown') { e.preventDefault(); rotate(-1); }
  });

  document.addEventListener('keyup', function (e) {
    if (e.key >= '1' && e.key <= '7') keyUp(+e.key - 1);
  });

  /* ---------- 初始化 ---------- */
  buildKeys();
  updateInstUI();
  updateOctUI();
  updateModeUI();
  updateKnobCaption();
  updateStats();
  updateSheetProgress();
  startBreath();
  setStatus('待机 · 选择模式后按压旋钮开始');
})();
