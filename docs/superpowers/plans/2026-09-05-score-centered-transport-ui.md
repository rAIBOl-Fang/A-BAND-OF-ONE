# Fixed-Center Score Transport UI Implementation Plan

> 状态：已完成。历史实施计划，仅供追溯，不再作为当前待办。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让五线谱与简谱共用一个固定在谱面中央的节拍指针，由板端真实按键与拍长状态驱动谱带平滑移动，并在曲终复位、重新装载或第二轮开始前可靠回到第一音。

**Architecture:** `state.cursor / state.subphase / state.note_ticks / state.note_total_ticks / state.keys` 继续是唯一演奏真相；网页不建立第二时钟。谱面从“游标在长画布内移动、超过阈值再跳动滚动”改为“中央指针固定、当前可见谱带用 CSS transform 平滑位移”，五线谱和简谱分别读取自己的实际音符中心坐标。曲终回到 `standby + cursor=0`、装载新谱与切换记谱法时走同一个即时复位入口。

**Tech Stack:** 单文件 HTML/CSS/JavaScript、Web Serial 换行 JSON、Node.js `node:test`/静态契约测试、ESP32-S3 现有 P2 状态协议（本计划不改固件）。

**Spec:** `flow/plan.md`“P3 Web／键盘协同”、`docs/Web控制台功能框架.md` §3.3、`docs/superpowers/plans/2026-09-05-p3-web-device-feedback-review.md` §3.3，以及 2026-09-05 用户真机反馈。

## Global Constraints

- 本计划先解决网页呈现；不修改 firmware、不构建 ESP 固件、不烧录 COM5。
- 板端状态仍为真相；禁止用 `setInterval`、`requestAnimationFrame` 或浏览器音频时钟自行推进真实曲目游标。
- `judge.ok`/黑色按键只标记当前音已经开始；实际拍长继续由 `note_ticks/note_total_ticks` 决定。
- 错音、Waiting、Standby、断线时谱带不得自行前进；暂停中途必须保留当前 `cursor`。
- 内置谱和所有合法 `.abo.score.json` 必须共用同一套轨道定位与复位函数。
- 不改变键盘发声、严格判定、音色、音量、模式切换与曲终实体输入合同。

---

## Root-Cause Evidence

1. `renderScoreCursor()` 只要发现动态创建的 `#jianpu-playhead` 就用简谱几何覆盖 `visibleCursorX`；五线谱显示时简谱带有 `.hidden { display:none }`，其 `offsetLeft/offsetWidth` 为 0，因此五线谱自动滚动条件永远不能成立。
2. 当前只给移动游标设置 `left 110ms` 过渡，真正的长谱移动使用同步赋值 `stage.scrollLeft = desiredScroll`；每约 100ms 收到一次板端快照时会表现为停顿后跳动。
3. 板端 `reset_from_finished()` 已把 `cursor` 清为 0，但网页在 Standby 只重画游标，不把 `score-stage.scrollLeft` 清零；第二轮仍停留在上一轮末尾。
4. 正确按键后变黑来自即时 `judge.ok`，但下一个目标必须等待板端把当前 1 拍或 2 拍播放完。这部分是现有严格跟谱规则，不是网页卡顿：BPM 90 时 1 拍约 667ms、2 拍约 1.33s。
5. 现有网页测试只证明源码中存在游标、`offsetLeft` 和 `scrollLeft`，没有覆盖“隐藏简谱不能影响五线谱”“曲终 cursor 归零必须同步复位视口”和“中央固定指针”行为，因此此前 GREEN 未阻止本次回归。

---

### Task 0: Confirm and Freeze the Revised Presentation Contract

**Files:**
- Modify after user approval: `flow/plan.md`
- Modify after user approval: `docs/Web控制台功能框架.md`
- Modify after user approval: `docs/技术方案.md`
- Append after user approval: `flow/decisions.md`

**Interfaces:**
- Consumes: current P3 state fields and strict-score semantics.
- Produces: one frozen rule: fixed center playhead, moving active score track, board-snapshot interpolation, authoritative reset.

- [x] **Step 1: Replace the old 70% moving-playhead wording**

  Record that the visible playhead is fixed at the horizontal center; only the active notation track translates beneath it. Remove the old “游标到 70% 后设置 `scrollLeft`” behavior from the active-playing contract.

- [x] **Step 2: Freeze the black-key relationship**

  Record that `judge.ok`/`state.keys=holding` makes the physical-key mirror black immediately and marks note onset, while `note_ticks/note_total_ticks` remains the only source for motion through the note duration.

- [x] **Step 3: Freeze reset triggers**

  Record that `standby + cursor=0` after Finished, successful score load, and notation switch must recenter the first note without animation; pausing at `cursor>0` must not jump to the first note.

- [x] **Step 4: Stop for contract review**

  Do not touch `console/index.html` until the user confirms the updated contract text.

---

### Task 1: Add RED Tests for Fixed-Center Motion and Reset

**Files:**
- Modify: `console/index.test.mjs`
- Test: `console/index.test.mjs`

**Interfaces:**
- Consumes: embedded `AboConsoleCore` test surface.
- Produces: tests for `computeScoreVisualPosition()` and `shouldResetScoreViewport()` plus DOM/CSS contracts.

- [x] **Step 1: Write the failing pure geometry tests**

```js
assert.deepEqual(
  core.computeScoreVisualPosition({
    viewportWidth: 1000,
    paddingLeft: 20,
    noteCenters: [90, 142, 212],
    cursor: 1,
    ratio: 0.5,
    endX: 250
  }),
  { cursorX: 177, translateX: 303 }
);

assert.deepEqual(
  core.computeScoreVisualPosition({
    viewportWidth: 1000,
    paddingLeft: 20,
    noteCenters: [90, 142, 212],
    cursor: 1,
    ratio: 0,
    endX: 250
  }),
  { cursorX: 142, translateX: 338 }
);
```

- [x] **Step 2: Write the failing reset tests**

```js
assert.equal(core.shouldResetScoreViewport({
  previousPhase: 'finished', previousCursor: 42,
  nextPhase: 'standby', nextCursor: 0
}), true);

assert.equal(core.shouldResetScoreViewport({
  previousPhase: 'playing', previousCursor: 12,
  nextPhase: 'standby', nextCursor: 12
}), false);
```

- [x] **Step 3: Add structural regression contracts**

  Assert that the page has exactly one `#score-playhead`, both notation views are score tracks, movement uses `translate3d`, active-track selection reads `state.notation`, reset writes `stage.scrollLeft = 0`, and the production script contains no timer/animation loop that advances `state.cursor` or `state.noteTicks`.

- [x] **Step 4: Run RED**

Run: `node console/index.test.mjs`

Expected: FAIL only on the new fixed-center helpers and DOM/CSS contracts; all prior protocol, volume, import, curve-finish and S9 contracts remain green.

---

### Task 2: Implement the Shared Fixed-Center Score Transport

**Files:**
- Modify: `console/index.html:218-313`
- Modify: `console/index.html:964-983`
- Modify: `console/index.html:1430-1572`
- Modify: `console/index.html:1651-1772`
- Test: `console/index.test.mjs`

**Interfaces:**
- Consumes: `state.notation`, `state.cursor`, `state.subphase`, `state.noteTicks`, `state.noteTotalTicks`, rendered `.staff-note` and `.jianpu-note` nodes.
- Produces: `computeScoreVisualPosition(input) -> {cursorX, translateX}`, `shouldResetScoreViewport(input) -> boolean`, `renderScoreMotion({immediate})`, and `resetScoreViewport()`.

- [x] **Step 1: Add pure geometry and reset helpers to `AboConsoleCore`**

```js
function computeScoreVisualPosition({ viewportWidth, paddingLeft, noteCenters, cursor, ratio, endX }) {
  const safeCursor = Math.max(0, Math.min(noteCenters.length, cursor));
  const from = safeCursor < noteCenters.length ? noteCenters[safeCursor] : endX;
  const to = safeCursor + 1 < noteCenters.length ? noteCenters[safeCursor + 1] : endX;
  const safeRatio = Math.max(0, Math.min(1, ratio));
  const cursorX = from + (to - from) * safeRatio;
  return { cursorX, translateX: viewportWidth / 2 - paddingLeft - cursorX };
}

function shouldResetScoreViewport({ previousPhase, previousCursor, nextPhase, nextCursor }) {
  return nextPhase === 'standby' && nextCursor === 0 &&
    (previousPhase === 'finished' || previousCursor > 0);
}
```

  Export both helpers from `AboConsoleCore`.

- [x] **Step 2: Replace the two moving playheads with one stage overlay**

  Add one `<div id="score-playhead" class="score-playhead" aria-hidden="true"></div>` under `#score-stage`. Give `#score-stage` `position:relative; overflow:hidden`; give the fixed playhead `left:50%`, full score height and orange color. Remove `#staff-playhead` and dynamically created `#jianpu-playhead`.

- [x] **Step 3: Make both notation views transformable tracks**

  Add `.score-track { transform:translate3d(0,0,0); transition:transform 110ms linear; will-change:transform; }` to both staff and jianpu. Keep each notation's existing note layout; collect actual note centers from the active view so jianpu gap/min-width and staff spacing cannot contaminate each other.

- [x] **Step 4: Consume subphase and compute the active track only**

  Extend `validateBoardState()` to accept `subphase:none|waiting|holding`; store `state.subphase`. In `renderScoreMotion()`, select exactly `#staff-view` when `state.notation==='staff'`, otherwise `#jianpu-view`. Use `ratio=noteTicks/noteTotalTicks` only while `phase==='playing' && subphase==='holding'`; otherwise use 0 and freeze on the current note center.

- [x] **Step 5: Couple black-key onset to the same render pass**

  Keep the existing immediate `judge.ok -> keyStates[target]='holding' -> renderKeys()` path, then call `renderScoreMotion()` once. Do not increment ticks locally. The following board states, published about every 100ms, update the transform and CSS bridges the snapshots without becoming a second clock.

- [x] **Step 6: Add the authoritative reset path**

  Capture previous phase/cursor before applying each valid state. When `shouldResetScoreViewport()` returns true, call `resetScoreViewport()` with transition disabled for that render, set `stage.scrollLeft=0`, and center note 0. Call the same reset after successful `score_commit` rendering and on notation switch; add a resize handler that recenters without changing cursor.

- [x] **Step 7: Run focused GREEN**

Run: `node console/index.test.mjs`

Expected: PASS, including active-view isolation, central playhead, smooth transform, black-key onset and second-round reset contracts.

---

### Task 3: Full Web Regression and Static Clock Audit

**Files:**
- Verify: `console/index.html`
- Verify: all `console/*.test.mjs`

**Interfaces:**
- Consumes: Task 2 implementation.
- Produces: repeatable automated evidence before COM5 testing.

- [x] **Step 1: Run every console test**

Run every `console/*.test.mjs` with Node and require zero failures. Expected baseline: 16 test files plus the newly added assertions in `index.test.mjs`.

- [x] **Step 2: Parse every embedded script**

  Extract the two inline scripts from `console/index.html` and run `node --check` on each. Expected: 2/2 parse successfully.

- [x] **Step 3: Audit the time-source boundary**

  Search production code for `setInterval`, cursor-changing `setTimeout`, and animation-loop writes to `state.cursor/state.noteTicks`. Expected: none. The existing 80ms volume debounce and 120ms S9 visual pulse are allowed because they do not advance the score.

- [x] **Step 4: Stop before hardware changes**

  This plan requires no firmware build and no burn. If web-only HIL exposes missing board data, return to diagnosis before proposing any firmware change.

---

### Task 4: COM5 Manual HIL Acceptance

**Files:**
- Verify in browser: `http://127.0.0.1:8800/console/index.html`
- Append after acceptance: `flow/进展.md`
- Append if a new defect appears: `flow/踩坑记录.md`

**Interfaces:**
- Consumes: existing flashed P2/P3 firmware and updated web page.
- Produces: acceptance evidence for both notation views and repeated play.

- [x] **Step 1: Five-line score movement**

  Hard-refresh, connect COM5, load《小星星》, select 五线谱, press S9. Expected: orange playhead stays centered; first note is centered; correct key turns black immediately; score moves smoothly beneath the line; wrong key does not move it.

- [x] **Step 2: Jianpu movement and duration**

  Repeat in 简谱 and test both a 1-beat and 2-beat note. Expected: motion begins with the black-key/holding state and remains smooth; 2-beat motion lasts twice as long. The next target still waits for the board-defined note duration.

- [x] **Step 3: Pause and resume**

  Pause at a middle note with S9, then resume. Expected: Standby freezes at the same `cursor`; it does not return to note 0 and does not drift while paused.

- [x] **Step 4: Finish and second round**

  Finish once, press one physical key to return to Standby, then press S9 to start again. Expected: before round two starts, both 五线谱 and 简谱 are centered on the first note; no old end position or scrollbar offset remains.

- [x] **Step 5: Imported-score spot check**

  Load one non-Twinkle `.abo.score.json` and repeat the first/middle/last checks. Expected: the same movement system follows imported note lengths and does not contain Twinkle-specific coordinates.

---

## Effort and Product Boundary

- Recommended web-only implementation: about 60–90 minutes of code/test work plus 10–15 minutes of user COM5 HIL; no model, cloud API or recurring cost.
- This plan removes visual dead time and jumping, but deliberately preserves musical duration. If “无需等待” means pressing once should immediately unlock the next note, that is a different product rule: it requires changing the P2 firmware's `Holding -> finish_current_note()` timing, rebuilding, burning and re-running sound/score HIL. Do not include that behavior in this web-only plan without a separate user decision.
