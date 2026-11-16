// The page's half of the browser front end.
//
// The game runs in worker.js and says three kinds of thing: the main term's
// cells (drawn here as a tile map, or whole as the classic screen when the
// game is showing one of its own), what it knows about the character and the
// pack (drawn as the panels), and questions (drawn as dialogs).  Keys and
// clicks go back to it, and so do the dialogs' answers.

'use strict';

(function () {
  // A map tile on the page, in CSS pixels.  Neon is drawn at 16, so this is
  // exactly twice that and the pixels stay square.
  const TILE = 32;
  const PANEL_TILE = 24;
  const DIALOG_TILE = 32;

  // The text screens want this much term: 80 columns is the game's floor, and
  // the rows were sized for the birth screen's list of races, which the game
  // still lays out behind the page's own birth view.
  const MIN_COLS = 80;
  const MIN_ROWS = 32;
  const SIDEBAR_COLS = 13; // COL_MAP with the sidebar on the left

  const COLOUR_SHADE = 28;

  const $ = (id) => document.getElementById(id);
  const els = {
    map: $('map'),
    mapwrap: $('mapwrap'),
    classic: $('classic'),
    classicCanvas: $('classic-canvas'),
    prompt: $('prompt'),
    statusline: $('statusline'),
    log: $('log'),
    char: $('char'),
    inven: $('inven'),
    equip: $('equip'),
    quiver: $('quiver'),
    quiverHead: $('quiver-head'),
    burden: $('burden'),
    scrim: $('scrim'),
    dialog: $('dialog'),
    loading: $('loading'),
    loadingText: $('loading-text'),
    tiles: $('tiles'),
    tilesPick: $('tiles-pick'),
    birth: $('birth'),
  };

  // The tileset is remembered by this browser; the game falls back to Neon if
  // the remembered one has gone.
  const store = {
    get(k) { try { return window.localStorage.getItem(k); } catch (e) { return null; } },
    set(k, v) { try { window.localStorage.setItem(k, v); } catch (e) { /* not kept */ } },
  };

  let palette = new Array(32).fill('#ffffff');
  let sheet = null; // { img, url, w, h }
  let tileW = 16;
  let tileH = 16;
  let term = null; // { wid, hgt, cells: Int32Array of a, c, ta, tc }
  let cursor = { x: 0, y: 0, v: false };
  let layout = { overlay: true, mapRow: 1, mapCol: 13, mapW: 0, mapH: 0, tw: 1, th: 1 };
  let lastGear = null;
  let dialog = null;
  let frameQueued = false;
  let lastPrompt = '';
  let lastStatus = '';

  const worker = new Worker('worker.js');
  const send = (msg) => worker.postMessage(msg);

  // ------------------------------------------------------------- starting --

  function start() {
    const r = els.mapwrap.getBoundingClientRect();
    const tilesW = Math.max(12, Math.floor((r.width - 8) / TILE));
    const tilesH = Math.max(8, Math.floor((r.height - 8) / TILE));
    // Tiles are tw by th cells, chosen so that the term is big enough for the
    // text screens while its map is exactly what fits on the page.
    const tw = Math.max(1, Math.ceil((MIN_COLS - SIDEBAR_COLS - 1) / tilesW));
    const th = Math.max(1, Math.ceil((MIN_ROWS - 2) / tilesH));
    send({
      type: 'start',
      cols: SIDEBAR_COLS + 1 + tw * tilesW,
      rows: 2 + th * tilesH,
      tw: tw,
      th: th,
      graf: Number(store.get('zangband.tiles')) || 7,
    });
  }

  worker.onmessage = function (e) {
    const m = e.data;
    switch (m.type) {
      case 'json': onJson(JSON.parse(m.text)); break;
      case 'rows': onRows(m); break;
      case 'file': if (m.what === 'tiles') loadSheet(m.data); break;
      case 'ask': {
        const q = JSON.parse(m.text);
        if (q.kind === 'birth') openBirth(q); else openDialog(q);
        break;
      }
      case 'loading': els.loadingText.textContent = m.text || 'Starting…'; break;
      case 'quit': onQuit(m); break;
      case 'error': els.loadingText.textContent = 'Something went wrong: ' + m.text; break;
    }
  };

  function onJson(j) {
    switch (j.type) {
      case 'palette': palette = j.colors; redraw(); break;
      case 'tileinfo': tileW = j.w; tileH = j.h; break;
      case 'tilesets': renderTilesets(j); break;
      case 'state': layout = j; redraw(); break;
      case 'status': renderChar(j); break;
      case 'gear': lastGear = j; renderGear(j); break;
      case 'msg': addMessage(j); break;
    }
  }

  function renderTilesets(t) {
    els.tiles.innerHTML = t.list.map((m) => '<option value="' + m.id + '"' + (m.ok ? '' : ' disabled') +
      (m.id === t.current ? ' selected' : '') + '>' + esc(m.name) + (m.ok ? '' : ' (not in the browser)') +
      '</option>').join('');
    els.tilesPick.hidden = false;
  }

  els.tiles.addEventListener('change', function () {
    const id = Number(els.tiles.value);
    store.set('zangband.tiles', String(id));
    send({ type: 'graf', id: id });
    els.tiles.blur(); // the keys belong to the game
  });

  function loadSheet(bytes) {
    const url = URL.createObjectURL(new Blob([bytes], { type: 'image/png' }));
    const img = new Image();
    img.onload = function () {
      if (sheet) URL.revokeObjectURL(sheet.url);
      sheet = { img: img, url: url, w: img.naturalWidth, h: img.naturalHeight };
      if (lastGear) renderGear(lastGear);
      redraw();
    };
    img.src = url;
  }

  function onRows(m) {
    if (!term || term.wid !== m.wid || term.hgt !== m.hgt) {
      term = { wid: m.wid, hgt: m.hgt, cells: new Int32Array(m.wid * m.hgt * 4) };
    }
    const stride = m.wid * 4;
    for (let i = 0; i < m.ys.length; i++) {
      term.cells.set(m.data.subarray(i * stride, (i + 1) * stride), m.ys[i] * stride);
    }
    cursor = { x: m.cx, y: m.cy, v: !!m.cv };
    if (!els.loading.hidden && !els.loading.dataset.quit) els.loading.hidden = true;
    redraw();
  }

  function onQuit(m) {
    els.loading.dataset.quit = '1';
    els.loading.hidden = false;
    els.loadingText.textContent = m.saved
      ? 'Zangband has exited. Your game is saved in this browser.'
      : 'Zangband has exited. The game may not have been saved.';
    const b = document.createElement('button');
    b.textContent = 'Start again';
    b.onclick = () => window.location.reload();
    els.loading.appendChild(b);
  }

  // -------------------------------------------------------------- drawing --

  function redraw() {
    if (frameQueued) return;
    frameQueued = true;
    requestAnimationFrame(draw);
  }

  function draw() {
    frameQueued = false;
    if (!term) return;
    drawTextRow(els.prompt, 0, 0, term.wid, 'prompt');
    // A dialog stands in front of the map; the classic screen is for what the
    // game has drawn itself, and has its own bottom line.
    const classic = layout.overlay;
    els.classic.hidden = !classic;
    els.statusline.style.visibility = classic ? 'hidden' : '';
    // The status bar starts after the sidebar's columns, but some prompts
    // (look's, for one) put their hints in the bottom line from the left.
    let from = layout.mapCol;
    for (let x = 0; x < layout.mapCol; x++) {
      const [a, c] = cellAt(x, term.hgt - 1);
      if (c > 32 && !isTile(a, c) && a !== 255) { from = 0; break; }
    }
    drawTextRow(els.statusline, term.hgt - 1, from, term.wid, 'status');
    if (classic) drawClassic(); else drawMap();
  }

  function cellAt(x, y) {
    const i = (y * term.wid + x) * 4;
    const c = term.cells;
    return [c[i], c[i + 1], c[i + 2], c[i + 3]];
  }

  const isTile = (a, c) => (a & 0x80) && (c & 0x80);

  function blit(ctx, a, c, x, y, w, h) {
    if (!sheet) return;
    ctx.drawImage(sheet.img, (c & 0x7f) * tileW, (a & 0x7f) * tileH, tileW, tileH, x, y, w, h);
  }

  // One cell, which may be a tile spanning tw by th cells, or a character.
  function drawCell(ctx, tx, ty, px, py, cw, ch, font) {
    const [a, c, ta, tc] = cellAt(tx, ty);
    if (a === 255) return; // the far side of a tile drawn already
    if (isTile(a, c)) {
      const w = cw * layout.tw;
      const h = ch * layout.th;
      if (isTile(ta, tc)) blit(ctx, ta, tc, px, py, w, h);
      if (a !== ta || c !== tc) blit(ctx, a, c, px, py, w, h);
      return;
    }
    const fg = palette[a % 32] || '#fff';
    const mode = Math.floor(a / 256);
    if (mode === 1 || mode === 2) {
      ctx.fillStyle = mode === 1 ? fg : palette[COLOUR_SHADE];
      ctx.fillRect(px, py, cw, ch);
    }
    if (c > 32) {
      ctx.fillStyle = fg;
      ctx.font = font;
      ctx.fillText(String.fromCodePoint(c), px + cw / 2, py + ch * 0.8);
    }
  }

  // Size a canvas to W by H logical pixels, shrunk to fit its box if need be,
  // at the screen's own resolution.
  function fit(canvas, W, H, box) {
    const scale = Math.min(1, (box.clientWidth - 4) / W, (box.clientHeight - 4) / H);
    const cssW = Math.floor(W * scale);
    const cssH = Math.floor(H * scale);
    const dpr = window.devicePixelRatio || 1;
    const pw = Math.round(cssW * dpr);
    const ph = Math.round(cssH * dpr);
    if (canvas.width !== pw || canvas.height !== ph) {
      canvas.width = pw;
      canvas.height = ph;
      canvas.style.width = cssW + 'px';
      canvas.style.height = cssH + 'px';
    }
    const ctx = canvas.getContext('2d');
    ctx.setTransform(pw / W, 0, 0, ph / H, 0, 0);
    ctx.imageSmoothingEnabled = false;
    ctx.textAlign = 'center';
    return ctx;
  }

  function drawMap() {
    const L = layout;
    if (!L.mapW || !L.mapH) return;
    const cw = TILE / L.tw;
    const ch = TILE / L.th;
    const W = L.mapW * cw;
    const H = L.mapH * ch;
    const ctx = fit(els.map, W, H, els.mapwrap);
    const font = Math.round(Math.min(ch * 0.8, cw * 1.5)) + 'px "Plex Mono", monospace';

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, W, H);
    for (let y = 0; y < L.mapH; y++) {
      for (let x = 0; x < L.mapW; x++) {
        drawCell(ctx, L.mapCol + x, L.mapRow + y, x * cw, y * ch, cw, ch, font);
      }
    }

    if (cursor.v && cursor.x >= L.mapCol && cursor.y >= L.mapRow
        && cursor.x < L.mapCol + L.mapW && cursor.y < L.mapRow + L.mapH) {
      ctx.strokeStyle = '#f2d24b';
      ctx.lineWidth = 2;
      ctx.strokeRect((cursor.x - L.mapCol) * cw + 1, (cursor.y - L.mapRow) * ch + 1,
        cw * L.tw - 2, ch * L.th - 2);
    }
  }

  // The whole term, as the game drew it: the classic screen.
  function drawClassic() {
    const box = els.mapwrap;
    const cw = Math.max(6, Math.floor(Math.min((box.clientWidth - 8) / term.wid,
      0.6 * (box.clientHeight - 8) / term.hgt)));
    const ch = Math.floor(cw / 0.6);
    const W = term.wid * cw;
    const H = term.hgt * ch;
    const ctx = fit(els.classicCanvas, W, H, box);
    const font = Math.round(ch * 0.78) + 'px "Plex Mono", monospace';

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, W, H);
    for (let y = 0; y < term.hgt; y++) {
      for (let x = 0; x < term.wid; x++) {
        drawCell(ctx, x, y, x * cw, y * ch, cw, ch, font);
      }
    }
    if (cursor.v) {
      ctx.strokeStyle = '#f2d24b';
      ctx.lineWidth = 1.5;
      ctx.strokeRect(cursor.x * cw + 0.5, cursor.y * ch + 0.5, cw - 1, ch - 1);
    }
  }

  // A row of the term as text, in the game's colours: the prompt line and the
  // status line, which read better as type than as a canvas.
  function drawTextRow(el, y, x0, x1, which) {
    let key = '';
    const runs = [];
    let run = null;
    for (let x = x0; x < x1; x++) {
      const [a, c] = cellAt(x, y);
      const ch = (a === 255 || isTile(a, c) || c < 32) ? ' ' : String.fromCodePoint(c);
      const col = a % 32;
      if (!run || run.col !== col) {
        run = { col: col, text: '' };
        runs.push(run);
      }
      run.text += ch;
      key += ch + col;
    }
    if (which === 'prompt') {
      if (key === lastPrompt) return;
      lastPrompt = key;
    } else {
      if (key === lastStatus) return;
      lastStatus = key;
    }
    el.textContent = '';
    for (const r of runs) {
      const span = document.createElement('span');
      span.style.color = palette[r.col];
      span.textContent = r.text;
      el.appendChild(span);
    }
  }

  // --------------------------------------------------------------- panels --

  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

  function bar(label, cur, max, colour) {
    const pct = max > 0 ? Math.max(0, Math.min(100, (100 * cur) / max)) : 0;
    return '<div class="bar"><div class="label"><span>' + label + '</span><b>' + cur + ' / ' + max +
      '</b></div><div class="track"><div class="fill" style="width:' + pct + '%;background:' + colour +
      '"></div></div></div>';
  }

  function renderChar(s) {
    let html = '<div class="name">' + esc(s.name) + '</div>';
    html += '<div class="who">' + esc(s.race) + ' ' + esc(s.cls) + ' · ' + esc(s.title) + '</div>';
    html += bar('Health', s.hp, s.mhp, palette[s.hpColor] || 'var(--hp)');
    if (s.hasMana) html += bar('Mana', s.sp, s.msp, 'var(--sp)');
    const toNext = s.next ? Math.max(0, s.next - s.exp) : 0;
    html += '<div class="stats">';
    for (const st of s.stats) {
      html += '<span class="k">' + esc(st.name) + '</span><span class="v' + (st.reduced ? ' reduced' : '') + '">' +
        esc(st.value.trim()) + (st.maxed ? '!' : '') + '</span>';
    }
    html += '</div><div class="facts">';
    const fact = (k, v) => { html += '<span class="k">' + k + '</span><span class="v">' + v + '</span>'; };
    fact('Level', s.lev + (s.lev < s.maxLev ? ' <span style="color:#e6c84c">(' + s.maxLev + ')</span>' : ''));
    fact('Next level', s.next ? toNext.toLocaleString() + ' xp' : '—');
    fact('Armour', s.ac);
    fact('Speed', s.speed === 0 ? 'Normal' : (s.speed > 0 ? '<span style="color:#7fd88e">Fast (+' + s.speed + ')</span>'
      : '<span style="color:#d39a5c">Slow (' + s.speed + ')</span>'));
    fact('Gold', s.gold.toLocaleString());
    fact('Where', esc(s.depth));
    html += '</div>';
    els.char.innerHTML = html;
  }

  function tileHTML(o, size) {
    if (o.t && sheet) {
      const k = size / tileW;
      return '<div class="tile" style="width:' + size + 'px;height:' + size + 'px;background-image:url(' + sheet.url +
        ');background-size:' + sheet.w * k + 'px ' + sheet.h * k + 'px;background-position:-' + o.t[1] * size + 'px -' +
        o.t[0] * size + 'px"></div>';
    }
    if (o.g) return '<div class="glyph" style="color:' + palette[(o.ga || 1) % 32] + '">' + esc(o.g) + '</div>';
    return '<div></div>';
  }

  const lb = (w) => (w / 10).toFixed(1) + ' lb';

  function itemLI(o) {
    return '<li title="' + esc(o.n) + '">' + tileHTML(o, PANEL_TILE) + '<span class="lbl">' + esc(o.l) +
      ')</span><span class="nm" style="color:' + palette[o.c % 32] + '">' + esc(o.n) + '</span><span class="wt">' +
      lb(o.w) + '</span></li>';
  }

  function renderGear(g) {
    els.inven.innerHTML = g.inven.length ? g.inven.map(itemLI).join('')
      : '<li class="empty"><div></div><span></span><span class="nm">Nothing</span></li>';
    els.equip.innerHTML = g.equip.map(function (e) {
      if (!e.obj) {
        const slot = e.slot.charAt(0).toUpperCase() + e.slot.slice(1);
        return '<li class="empty"><div></div><span></span><span class="nm">' + esc(slot) + '</span><span></span></li>';
      }
      return itemLI(e.obj);
    }).join('');
    els.quiver.innerHTML = g.quiver.map(itemLI).join('');
    els.quiverHead.hidden = g.quiver.length === 0;
    els.burden.textContent = lb(g.burden);
  }

  function addMessage(m) {
    const div = document.createElement('div');
    div.className = 'm';
    div.style.color = palette[m.color % 32];
    div.textContent = m.text;
    els.log.appendChild(div);
    while (els.log.childElementCount > 300) els.log.firstElementChild.remove();
    els.log.scrollTop = els.log.scrollHeight;
  }

  // Messages from before the last command are dimmed, so the new ones stand out.
  function ageMessages() {
    for (const m of els.log.querySelectorAll('.m:not(.old)')) m.classList.add('old');
  }

  // -------------------------------------------------------------- dialogs --

  const SECTIONS = [
    { id: 'inven', name: 'Pack', key: '/' },
    { id: 'equip', name: 'Equipment', key: '/' },
    { id: 'quiver', name: 'Quiver', key: '|' },
    { id: 'floor', name: 'Floor', key: '-' },
  ];

  function answer(value) {
    send({ type: 'answer', value: value });
    dialog = null;
    els.scrim.hidden = true;
    els.dialog.innerHTML = '';
    if (birth && birth.waiting) birthIdle();
    redraw();
  }

  function openDialog(q) {
    dialog = { q: q, sel: 0, section: null, rows: [] };
    els.scrim.hidden = false;
    if (q.kind === 'item') buildItem();
    else if (q.kind === 'menu') buildMenu();
    else if (q.kind === 'check') buildCheck();
    else if (q.kind === 'quantity') buildInput(true);
    else if (q.kind === 'string') buildInput(false);
    else answer(-1);
    redraw();
  }

  function shell(prompt, body, foot) {
    // The text front end's key hints are the dialog's footer here.
    prompt = String(prompt).replace(/^\(Enter to select, ESC\)\s*/, '').replace(/:\s*$/, '');
    els.dialog.innerHTML = '<div class="q">' + esc(prompt) + '</div>' + body +
      (foot ? '<div class="foot">' + foot + '</div>' : '');
  }

  // Rows that can be picked: by letter, by the arrows and Return, or by a click.
  function wireRows(onPick) {
    dialog.rows = Array.from(els.dialog.querySelectorAll('.row'));
    dialog.rows.forEach(function (row, i) {
      row.addEventListener('click', function () { if (!row.classList.contains('off')) onPick(i); });
      row.addEventListener('mousemove', function () { select(i); });
    });
    dialog.pick = onPick;
    select(Math.max(0, dialog.rows.findIndex((r) => !r.classList.contains('off'))));
  }

  function select(i) {
    if (!dialog || !dialog.rows.length) return;
    dialog.sel = (i + dialog.rows.length) % dialog.rows.length;
    dialog.rows.forEach((r, k) => r.classList.toggle('sel', k === dialog.sel));
    dialog.rows[dialog.sel].scrollIntoView({ block: 'nearest' });
  }

  function rowKeys(e) {
    if (e.key === 'ArrowDown') { select(dialog.sel + 1); return true; }
    if (e.key === 'ArrowUp') { select(dialog.sel - 1); return true; }
    if (e.key === 'Enter' || e.key === ' ') {
      const row = dialog.rows[dialog.sel];
      if (row && !row.classList.contains('off')) dialog.pick(dialog.sel);
      return true;
    }
    return false;
  }

  function buildItem() {
    const q = dialog.q;
    const present = SECTIONS.filter((s) => q.items.some((it) => it.where === s.id));
    if (!dialog.section) {
      dialog.section = (q.start && present.some((s) => s.id === q.start)) ? q.start : present[0].id;
    }
    const shown = q.items.map((it, i) => ({ it: it, i: i })).filter((x) => x.it.where === dialog.section);
    let tabs = '';
    if (present.length > 1) {
      tabs = '<div class="tabs">' + present.map((s) => '<button data-s="' + s.id + '"' +
        (s.id === dialog.section ? ' class="on"' : '') + '>' + s.name + '<kbd>' + esc(s.key) + '</kbd></button>').join('') +
        '</div>';
    }
    const list = '<div class="list">' + shown.map((x) => '<div class="row">' + tileHTML(x.it, DIALOG_TILE) +
      '<span class="lbl">' + esc(x.it.l) + ')</span><span style="color:' + palette[x.it.c % 32] + '">' + esc(x.it.n) +
      '</span><span class="wt">' + lb(x.it.w) + '</span></div>').join('') + '</div>';
    shell(q.prompt, tabs + list, '<span><kbd>a</kbd>–<kbd>z</kbd> choose · <kbd>Esc</kbd> cancel</span>');
    for (const b of els.dialog.querySelectorAll('.tabs button')) {
      b.addEventListener('click', function () { dialog.section = b.dataset.s; buildItem(); });
    }
    dialog.shown = shown;
    wireRows((k) => answer(shown[k].i));
    dialog.onKey = function (e) {
      if (e.key === 'Escape') return answer(-1);
      if (rowKeys(e)) return;
      const has = (id) => present.some((s) => s.id === id);
      let to = null;
      if (e.key === '/') to = dialog.section === 'inven' ? 'equip' : 'inven';
      else if (e.key === '|') to = 'quiver';
      else if (e.key === '-') to = 'floor';
      else if (e.key === 'Tab') {
        const at = present.findIndex((s) => s.id === dialog.section);
        to = present[(at + 1) % present.length].id;
      }
      if (to && has(to)) { dialog.section = to; buildItem(); return; }
      const hit = shown.findIndex((x) => x.it.l === e.key);
      if (hit >= 0) answer(shown[hit].i);
    };
  }

  function buildMenu() {
    const q = dialog.q;
    const list = '<div class="list">' + q.items.map((it) => '<div class="row text' + (it.ok ? '' : ' off') + '">' +
      '<span class="lbl">' + (it.l ? esc(it.l) + ')' : '') + '</span><span>' + esc(it.n) + '</span></div>').join('') +
      '</div>';
    shell(q.prompt, list, '<span><kbd>↑</kbd><kbd>↓</kbd> and <kbd>Return</kbd>, or the letter · <kbd>Esc</kbd> cancel</span>');
    wireRows((k) => answer(k));
    dialog.onKey = function (e) {
      if (e.key === 'Escape') return answer(-1);
      if (rowKeys(e)) return;
      const hit = q.items.findIndex((it) => it.l && it.l === e.key && it.ok);
      if (hit >= 0) answer(hit);
    };
  }

  function buildCheck() {
    const text = dialog.q.prompt.replace(/\s*\[y\/n\]\s*$/i, '').trim();
    shell(text, '', '<span><kbd>y</kbd> or <kbd>n</kbd></span><div class="buttons">' +
      '<button class="act" data-v="0">No</button><button class="act primary" data-v="1">Yes</button></div>');
    for (const b of els.dialog.querySelectorAll('button.act')) {
      b.addEventListener('click', () => answer(Number(b.dataset.v)));
    }
    dialog.onKey = function (e) {
      if (e.key === 'y' || e.key === 'Y') answer(1);
      else if (e.key === 'n' || e.key === 'N' || e.key === 'Escape') answer(0);
    };
  }

  function buildInput(isNumber) {
    const q = dialog.q;
    const prompt = (q.prompt || '').replace(/[:\s]+$/, '');
    const value = isNumber ? '1' : (q.value || '');
    shell(prompt, '<input id="dlg-input" autocomplete="off" spellcheck="false"' +
      (isNumber ? ' inputmode="numeric"' : '') + ' maxlength="' + (q.max || 80) + '" value="' + esc(value) + '">',
      (isNumber ? '<span>up to ' + q.max + '</span>' : '<span></span>') + '<div class="buttons">' +
      '<button class="act" data-a="cancel">Cancel</button>' +
      (isNumber ? '<button class="act" data-a="all">All</button>' : '') +
      '<button class="act primary" data-a="ok">OK</button></div>');
    const input = $('dlg-input');
    const done = function (how) {
      if (how === 'cancel') return answer(isNumber ? -1 : null);
      if (isNumber) {
        const v = input.value.trim();
        if (how === 'all' || v === '*') return answer(q.max);
        const n = parseInt(v, 10);
        return answer(isNaN(n) ? -1 : Math.max(0, Math.min(q.max, n)));
      }
      answer(input.value);
    };
    for (const b of els.dialog.querySelectorAll('button.act')) b.addEventListener('click', () => done(b.dataset.a));
    input.focus();
    input.select();
    dialog.onKey = function (e) {
      if (e.key === 'Escape') done('cancel');
      else if (e.key === 'Enter') done('ok');
      else return 'pass'; // let the input have it
    };
  }

  // ---------------------------------------------------- making a character --

  // ui-birth.c runs character creation and asks the page for each stage's
  // answer, as it asks the Tk Birth window.  The page shows the stage and
  // answers with a verb -- "choose 3", "buy 0", "name Brand", "back" --
  // which main-web.c turns into the key the stage would have had.

  const STEPS = ['Race', 'Class', 'Realms', 'Stats', 'Name', 'History', 'Confirm'];
  const STEP_OF = { race: 0, class: 1, realm: 2, method: 3, points: 3, roller: 3, name: 4, history: 5, confirm: 6 };
  const MENU_STAGES = ['race', 'class', 'realm', 'method'];
  const PORTRAIT = 128;

  let birth = null; // the stage on show: { q, sel, waiting }
  const made = { last: '', sel: 0, stat: 0, rolled: false, classChosen: false, name: '', nameAt: -1, timer: 0 };

  // The portraits: a race is a column, a class two rows -- a man's, then a
  // woman's -- and the class backdrops are the last row.  portraits.txt
  // gives the order.
  const portraits = { size: 0, races: [], classes: [] };
  fetch('portraits/portraits.txt').then((r) => (r.ok ? r.text() : '')).then(function (t) {
    for (const line of t.split('\n')) {
      const [k, v] = line.split('\t');
      if (k === 'size') portraits.size = Number(v);
      else if (k === 'race' && v) portraits.races.push(v.trim().toLowerCase());
      else if (k === 'class' && v) portraits.classes.push(v.trim().toLowerCase());
    }
    if (birth) drawBirth();
  }).catch(function () { /* no portraits, then */ });

  // A woman or a man by the parity of what birth rolled, as in the Tk front
  // end, so a reroll can change it.
  const isWoman = (c) => !!c && (c.age + c.ht + c.wt) % 2 === 1;

  // The first of the classes that has a portrait; the races are drawn as
  // Rangers until a class is chosen, as Tk draws them, since a Ranger wears
  // nothing on the head to hide what tells the races apart.
  function portraitHTML(race, classes, woman, size) {
    const s = portraits.size;
    const col = portraits.races.indexOf(String(race || '').toLowerCase());
    let row = -1;
    for (const k of classes) {
      row = portraits.classes.indexOf(String(k || '').toLowerCase());
      if (row >= 0) break;
    }
    const box = 'width:' + size + 'px;height:' + size + 'px';
    if (!s || col < 0 || row < 0) return '<div class="portrait none" style="' + box + '"></div>';
    const n = portraits.classes.length;
    return '<div class="portrait" style="' + box + ';background-size:' +
      Math.max(portraits.races.length, n) * size + 'px ' + (2 * n + 1) * size + 'px;background-position:-' +
      col * size + 'px -' + (2 * row + (woman ? 1 : 0)) * size + 'px,-' + row * size + 'px -' + 2 * n * size +
      'px"></div>';
  }

  // The character as it would be if made now: the race or class under the
  // cursor while one is being chosen.
  function pictured(size) {
    const q = birth.q;
    const c = q.character || {};
    let race = c.race;
    let classes = [c.cls];
    const row = q.choices[birth.sel];
    if (row && q.stage === 'race') race = row.n;
    if (row && q.stage === 'class') classes = [row.n];
    if (q.stage === 'race' && !made.classChosen) classes = ['Ranger', c.cls];
    return portraitHTML(race, classes, isWoman(q.character), size);
  }

  const cap = (s) => String(s).charAt(0).toUpperCase() + String(s).slice(1);
  const signed = (n) => (n > 0 ? '+' + n : String(n));
  const paragraphs = (t) => String(t).split(/\n\s*\n/).filter((p) => p.trim())
    .map((p) => '<p>' + esc(p.replace(/\s+/g, ' ').trim()) + '</p>').join('');

  function realmsOf(c) {
    return c && c.realms && c.realms.length ? c.realms.map(cap).join(' and ') : '';
  }

  function birthQuestion(q) {
    const c = q.character || {};
    switch (q.stage) {
      case 'realm':
        // A class with two realms is asked twice, and the second question
        // has to look like a second one.
        if (q.slot === 0) return 'Choose a realm of magic.  ' + q.question;
        return 'Choose your second realm of magic' +
          (c.realms && c.realms[0] ? ', to study beside ' + cap(c.realms[0]) : '') + '.  ' + q.question;
      case 'points': return 'Spend the points on your stats.  Each one costs more the higher the stat already is.';
      case 'roller': return 'Roll again until you like what you see.  Previous brings back the roll before.';
      case 'name': return 'What is your character called?';
      case 'history': return 'Your character\'s history.  Change it if you like.';
      case 'confirm': return 'This is your character.  Create it to begin, or start over.';
      case 'quickstart':
        return 'Start again from your last character?  Play Again makes the same character afresh; ' +
          'you can also change the name and history, or make a new one.';
      default: return q.question;
    }
  }

  function button(act, label, key, opts) {
    opts = opts || {};
    return '<button class="bb' + (opts.primary ? ' primary' : '') + '" data-act="' + act + '"' +
      (opts.off ? ' disabled' : '') + '>' + label + (key ? '<kbd>' + esc(key) + '</kbd>' : '') + '</button>';
  }

  // Who is being made, beside the stages that are not about choosing who.
  function whoHTML(c) {
    if (!c) return '';
    let h = '<div class="bwho">' + pictured(160);
    // Not PLAYER, the savefile's name, which the game uses until there is one.
    const name = c.name && c.name !== 'PLAYER' ? c.name : made.name;
    if (name && birth.q.stage !== 'name') h += '<div class="nm">' + esc(name) + '</div>';
    h += '<div class="sub">' + esc(c.race + ' ' + c.cls) + '</div>';
    const realms = realmsOf(c);
    if (realms) h += '<div class="sub">' + esc(realms) + '</div>';
    return h + '</div>';
  }

  function statTable(c, points) {
    let h = '<table class="bstats"><thead><tr><th class="k">Stat</th><th>Base</th><th>Race</th><th>Class</th>' +
      '<th>Total</th>' + (points ? '<th>Spent</th><th></th>' : '') + '</tr></thead><tbody>';
    c.stats.forEach(function (s, i) {
      h += '<tr data-i="' + i + '"' + (points && i === made.stat ? ' class="sel"' : '') + '><td class="k">' +
        esc(s.name) + '</td><td>' + esc(s.base) + '</td><td>' + signed(s.race) + '</td><td>' + signed(s.cls) +
        '</td><td class="best">' + esc(s.best) + '</td>';
      if (points) {
        h += '<td>' + s.spent + '</td><td class="pm">' +
          '<button class="pmb" data-act="sell:' + i + '"' + (s.spent > 0 ? '' : ' disabled') +
          ' title="Take a point back">−</button>' +
          '<button class="pmb" data-act="buy:' + i + '"' + (s.cost <= c.left ? '' : ' disabled') +
          ' title="The next point costs ' + s.cost + '">+</button></td>';
      }
      h += '</tr>';
    });
    return h + '</tbody></table>';
  }

  function summaryHTML(c) {
    let h = '<div class="bsum">' + portraitHTML(c.race, [c.cls], isWoman(c), 192) + '<div class="main">';
    h += '<h3>' + esc(c.name) + '</h3><div class="sub">' + esc(c.race + ' ' + c.cls);
    const realms = realmsOf(c);
    if (realms) h += ', of ' + esc(realms);
    h += '</div><div class="facts">';
    const fact = (k, v) => { h += '<span class="k">' + k + '</span><span class="v">' + v + '</span>'; };
    fact('Hit points', c.hp);
    if (c.sp) fact('Mana', c.sp);
    fact('Age', c.age);
    fact('Height', Math.floor(c.ht / 12) + '′' + (c.ht % 12) + '″');
    fact('Weight', c.wt + ' lb');
    h += '</div><div class="stats">';
    for (const s of c.stats) h += '<span class="k">' + esc(s.name) + '</span><span class="v">' + esc(s.best) + '</span>';
    h += '</div>';
    if (c.patron) {
      h += '<p class="patron">Serving <b>' + esc(c.patron) + '</b>' + (c.patronTitle ? ', ' + esc(c.patronTitle) : '') +
        '.' + (c.patronText ? ' ' + esc(c.patronText.replace(/\s+/g, ' ').trim()) : '') + '</p>';
    }
    if (c.history) h += '<p class="hist">' + esc(c.history.replace(/\s+/g, ' ').trim()) + '</p>';
    return h + '</div></div>';
  }

  function openBirth(q) {
    clearTimeout(made.timer);
    if (!els.loading.dataset.quit) els.loading.hidden = true;
    const key = q.stage + ':' + q.slot;
    if (key !== made.last) {
      // A new stage starts where the game's own cursor is.
      made.last = key;
      made.sel = q.cursor;
      made.rolled = false;
      made.nameAt = -1;
    }
    if (q.character && q.character.name && q.character.name !== 'PLAYER') made.name = q.character.name;
    if (q.stage === 'quickstart') made.classChosen = false;
    else if (!['race', 'class'].includes(q.stage)) made.classChosen = true;
    birth = { q: q, sel: Math.max(0, Math.min(made.sel, q.choices.length - 1)), waiting: false };
    document.body.classList.add('birthing');
    els.birth.hidden = false;
    els.birth.classList.remove('waiting');
    drawBirth();
  }

  function drawBirth() {
    const q = birth.q;
    const c = q.character;
    const at = STEP_OF[q.stage];
    // The quick start is not one of the steps; it comes before them.
    let h = '<div class="bcard' + (MENU_STAGES.includes(q.stage) ? '' : ' compact') + '">' +
      (at === undefined ? '<div class="steps">Your last character</div>' : '<ol class="steps">' +
        STEPS.map((s, i) => '<li class="' + (i > at ? '' : 'done') + (i === at ? ' now' : '') + '">' + s + '</li>')
          .join('') + '</ol>') + '<div class="bq">' + esc(birthQuestion(q)) + '</div><div class="bbody">';

    let left = '';
    let right = button('back', 'Back', 'Esc', { off: q.stage === 'quickstart' }) +
      button('next', q.stage === 'confirm' ? 'Create character' : 'Next', 'Return', { primary: true });

    if (MENU_STAGES.includes(q.stage)) {
      h += '<div class="bmenu"><div class="blist">' + q.choices.map((ch, i) =>
        '<div class="brow" data-i="' + i + '">' + esc(cap(ch.n)) + '</div>').join('') +
        '</div><div class="bdetail" id="b-detail"></div></div>';
      left = button('options', 'Options', '=') +
        (q.stage === 'race' || q.stage === 'class' ? button('random', 'Random', '*') : '') +
        button('finish', 'Finish at random', '@');
    } else if (q.stage === 'points' && c) {
      const spent = c.stats.reduce((n, s) => n + s.spent, 0);
      h += '<div class="bpair"><div>' + statTable(c, true) + '<p class="note">Points spent: <b>' + spent + '</b> of ' +
        (spent + c.left) + '</p></div>' + whoHTML(c) + '</div>';
      left = button('reset', 'Reset', 'r');
    } else if (q.stage === 'roller' && c) {
      h += '<div class="bpair"><div>' + statTable(c, false) + '</div>' + whoHTML(c) + '</div>';
      left = button('reroll', 'Roll again', 'r') + button('previous', 'Previous', 'p', { off: !made.rolled });
    } else if (q.stage === 'name') {
      // The browser keeps one savefile, PLAYER, and the game offers that as
      // the name; one of its random names is a better start.
      // Stepping back to it, the game has put PLAYER back; the name the
      // page last saw is the one to offer again.
      let name = c ? c.name : '';
      if (!name || name === 'PLAYER') name = made.name || '';
      if (!name && q.names && q.names.length) {
        if (made.nameAt < 0) made.nameAt = 0;
        name = q.names[made.nameAt];
      }
      h += '<div class="bpair"><div><input id="b-name" maxlength="31" autocomplete="off" spellcheck="false" value="' +
        esc(name) + '"></div>' + whoHTML(c) + '</div>';
      left = q.names && q.names.length ? button('randname', 'Another name', '') : '';
    } else if (q.stage === 'history') {
      h += '<div class="bpair"><div><textarea id="b-hist" maxlength="239" spellcheck="false">' +
        esc(c ? c.history.replace(/\s+/g, ' ').trim() : '') + '</textarea></div>' + whoHTML(c) + '</div>';
    } else if ((q.stage === 'confirm' || q.stage === 'quickstart') && c) {
      h += '<div class="bpair one">' + summaryHTML(c) + '</div>';
      if (q.stage === 'confirm') left = button('startover', 'Start over', 'S');
      else {
        left = button('options', 'Options', '=');
        right = button('redo', 'New character', 'n') + button('change', 'Change name and history', 'c') +
          button('use', 'Play again', 'y', { primary: true });
      }
    }
    h += '</div><div class="bfoot">' + left + '<span class="gap"></span>' + right + '</div></div>';
    els.birth.innerHTML = h;

    for (const b of els.birth.querySelectorAll('button[data-act]')) {
      b.addEventListener('click', () => birthAct(b.dataset.act));
    }
    for (const r of els.birth.querySelectorAll('.brow')) {
      r.addEventListener('click', () => birthSelect(Number(r.dataset.i)));
      r.addEventListener('dblclick', () => birthAct('next'));
    }
    for (const r of els.birth.querySelectorAll('table.bstats tr[data-i]')) {
      if (q.stage === 'points') r.addEventListener('click', function () { made.stat = Number(r.dataset.i); markStat(); });
    }
    if (MENU_STAGES.includes(q.stage)) birthSelect(birth.sel);
    const input = $('b-name') || $('b-hist');
    if (input) {
      input.focus();
      if (input.select) input.select();
    }
  }

  function birthSelect(i) {
    const q = birth.q;
    const n = q.choices.length;
    if (!n) return;
    birth.sel = made.sel = (i + n) % n;
    const rows = els.birth.querySelectorAll('.brow');
    rows.forEach((r, k) => r.classList.toggle('sel', k === birth.sel));
    if (rows[birth.sel]) rows[birth.sel].scrollIntoView({ block: 'nearest' });

    const ch = q.choices[birth.sel];
    const c = q.character || {};
    let h = '<div class="bhead">' + pictured(PORTRAIT) + '<div><h3>' + esc(cap(ch.n)) + '</h3>';
    if (q.stage === 'class' && c.race) h += '<div class="sub">' + esc(c.race + ' ' + ch.n) + '</div>';
    if (q.stage === 'method' || q.stage === 'realm') h += '<div class="sub">' + esc(c.race + ' ' + c.cls) + '</div>';
    h += '</div></div>';
    if (ch.p) h += '<div class="prose">' + paragraphs(ch.p) + '</div>';
    if (ch.d && ch.d.trim()) h += '<pre class="bdesc">' + esc(ch.d.replace(/\s+$/, '')) + '</pre>';
    $('b-detail').innerHTML = h;
    $('b-detail').scrollTop = 0;
  }

  function markStat() {
    els.birth.querySelectorAll('table.bstats tr[data-i]')
      .forEach((r) => r.classList.toggle('sel', Number(r.dataset.i) === made.stat));
  }

  function birthReply(value) {
    if (!birth || birth.waiting) return;
    birth.waiting = true;
    els.birth.classList.add('waiting');
    send({ type: 'answer', value: value });
    birthIdle();
  }

  // The next stage usually asks at once.  If nothing has asked in a while the
  // game has gone somewhere else -- into the dungeon, or to a text screen of
  // its own like the birth options -- and the overlay steps aside.
  function birthIdle() {
    clearTimeout(made.timer);
    made.timer = setTimeout(function () {
      if (!dialog && birth && birth.waiting) closeBirth();
    }, 400);
  }

  function closeBirth() {
    birth = null;
    els.birth.hidden = true;
    els.birth.innerHTML = '';
    document.body.classList.remove('birthing');
    redraw();
  }

  function birthAct(act) {
    if (!birth || birth.waiting) return;
    const q = birth.q;
    const [verb, arg] = act.split(':');
    switch (verb) {
      case 'next': return birthNext();
      case 'randname': {
        const names = q.names || [];
        if (!names.length) return;
        made.nameAt = (made.nameAt + 1) % names.length;
        const input = $('b-name');
        input.value = names[made.nameAt];
        input.focus();
        return;
      }
      case 'buy':
      case 'sell':
        made.stat = Number(arg);
        return birthReply(verb + ' ' + arg);
      case 'reroll':
        made.rolled = true;
        return birthReply('reroll');
      default:
        return birthReply(verb);
    }
  }

  function birthNext() {
    const q = birth.q;
    if (MENU_STAGES.includes(q.stage)) return birthReply('choose ' + birth.sel);
    if (q.stage === 'name') {
      const input = $('b-name');
      const v = input.value.trim();
      if (!v) {
        input.classList.remove('bad');
        void input.offsetWidth; // restart the shake
        input.classList.add('bad');
        return;
      }
      return birthReply('name ' + v);
    }
    if (q.stage === 'history') {
      const v = $('b-hist').value.replace(/\s+/g, ' ').trim();
      const was = ((q.character && q.character.history) || '').replace(/\s+/g, ' ').trim();
      return birthReply(v && v !== was ? 'history ' + v : 'accept');
    }
    if (q.stage === 'quickstart') return birthReply('use');
    return birthReply('accept');
  }

  function birthKey(e) {
    if (!birth || birth.waiting) return;
    const q = birth.q;
    const k = e.key;
    if (q.stage === 'name' || q.stage === 'history') {
      if (k === 'Escape') return birthAct('back');
      if (k === 'Enter') return birthAct('next');
      return 'pass'; // the typing is the input's
    }
    if (k === 'Escape') return q.stage === 'quickstart' ? undefined : birthAct('back');
    if (MENU_STAGES.includes(q.stage)) {
      if (k === 'ArrowDown') birthSelect(birth.sel + 1);
      else if (k === 'ArrowUp') birthSelect(birth.sel - 1);
      else if (k === 'Home') birthSelect(0);
      else if (k === 'End') birthSelect(q.choices.length - 1);
      else if (k === 'Enter' || k === 'ArrowRight') birthAct('next');
      else if (k === 'ArrowLeft') birthAct('back');
      else if (k === '*' && (q.stage === 'race' || q.stage === 'class')) birthAct('random');
      else if (k === '@') birthAct('finish');
      else if (k === '=') birthAct('options');
      return;
    }
    switch (q.stage) {
      case 'points': {
        const n = q.character ? q.character.stats.length : 6;
        if (k === 'ArrowDown') { made.stat = (made.stat + 1) % n; markStat(); }
        else if (k === 'ArrowUp') { made.stat = (made.stat + n - 1) % n; markStat(); }
        else if (k === '+' || k === 'ArrowRight') birthAct('buy:' + made.stat);
        else if (k === '-' || k === 'ArrowLeft') birthAct('sell:' + made.stat);
        else if (k === 'r') birthAct('reset');
        else if (k === 'Enter') birthAct('next');
        return;
      }
      case 'roller':
        if (k === 'r') birthAct('reroll');
        else if (k === 'p' && made.rolled) birthAct('previous');
        else if (k === 'Enter') birthAct('next');
        return;
      case 'confirm':
        if (k === 'S' || k === 's') birthAct('startover');
        else if (k === 'Enter') birthAct('next');
        return;
      case 'quickstart':
        if (k === 'y' || k === 'Enter') birthAct('use');
        else if (k === 'n') birthAct('redo');
        else if (k === 'c') birthAct('change');
        else if (k === '=') birthAct('options');
        return;
    }
  }

  // ------------------------------------------------------------ the keys --

  const SPECIAL = {
    ArrowDown: 0x80, ArrowLeft: 0x81, ArrowRight: 0x82, ArrowUp: 0x83,
    F1: 0x84, F2: 0x85, F3: 0x86, F4: 0x87, F5: 0x88, F6: 0x89, F7: 0x8a, F8: 0x8b,
    F9: 0x8c, F10: 0x8d, F11: 0x8e, F12: 0x8f,
    Home: 0x94, PageUp: 0x95, End: 0x96, PageDown: 0x97, Insert: 0x98, Pause: 0x99,
    Enter: 0x9c, Tab: 0x9d, Delete: 0x9e, Backspace: 0x9f, Escape: 0xe000,
  };
  const MOD = { CONTROL: 0x01, SHIFT: 0x02, ALT: 0x04, META: 0x08, KEYPAD: 0x10 };

  // As ui-event.h has it: control folds into letters, shift into punctuation.
  const encodesCtrl = (v) => (v >= 0x40 && v <= 0x5f) || (v >= 0x61 && v <= 0x7a);
  const includesShift = (v) => !((v >= 0x21 && v <= 0x2f) || (v >= 0x3a && v <= 0x60) || (v >= 0x7b && v <= 0x7e));

  function translate(e) {
    if (e.metaKey) return null; // the browser's own shortcuts
    if (['Shift', 'Control', 'Alt', 'Meta', 'CapsLock', 'Dead'].includes(e.key)) return null;
    const keypad = e.location === 3;
    let mods = keypad ? MOD.KEYPAD : 0;
    if (e.key in SPECIAL) {
      if (e.ctrlKey) mods |= MOD.CONTROL;
      if (e.shiftKey) mods |= MOD.SHIFT;
      if (e.altKey) mods |= MOD.ALT;
      return { code: SPECIAL[e.key], mods: mods };
    }
    if ([...e.key].length !== 1) return null;
    let code = e.key.codePointAt(0);
    if (e.ctrlKey) {
      // The letter itself, not whatever the layout made of it with control.
      const base = e.code.startsWith('Key') ? e.code.charCodeAt(3) + 32 : code;
      if (encodesCtrl(base)) code = base & 0x1f;
      else { code = base; mods |= MOD.CONTROL; }
    }
    if (e.shiftKey && includesShift(code) && code > 0x20) mods |= MOD.SHIFT;
    return { code: code, mods: mods };
  }

  window.addEventListener('keydown', function (e) {
    if (dialog) {
      if (dialog.onKey && dialog.onKey(e) === 'pass') return;
      e.preventDefault();
      return;
    }
    if (birth) {
      if (birthKey(e) === 'pass') return;
      e.preventDefault();
      return;
    }
    if (e.target === els.tiles) return;
    const k = translate(e);
    if (!k) return;
    e.preventDefault();
    ageMessages();
    send({ type: 'key', code: k.code, mods: k.mods });
  });

  // ---------------------------------------------------------- the mouse --

  function mouse(e, canvas, x0, y0, cols, rows) {
    const r = canvas.getBoundingClientRect();
    const x = x0 + Math.floor(((e.clientX - r.left) / r.width) * cols);
    const y = y0 + Math.floor(((e.clientY - r.top) / r.height) * rows);
    let mods = 0;
    if (e.shiftKey) mods |= MOD.SHIFT;
    if (e.ctrlKey) mods |= MOD.CONTROL;
    if (e.altKey) mods |= MOD.ALT;
    const button = (e.button === 2 ? 2 : 1) | (mods << 4);
    send({ type: 'mouse', x: x, y: y, button: button });
  }

  els.map.addEventListener('mousedown', function (e) {
    if (dialog) return;
    e.preventDefault();
    mouse(e, els.map, layout.mapCol, layout.mapRow, layout.mapW, layout.mapH);
  });
  els.classicCanvas.addEventListener('mousedown', function (e) {
    if (dialog || !term) return;
    e.preventDefault();
    mouse(e, els.classicCanvas, 0, 0, term.wid, term.hgt);
  });
  for (const c of [els.map, els.classicCanvas]) c.addEventListener('contextmenu', (e) => e.preventDefault());

  // ------------------------------------------------------------ the rest --

  document.addEventListener('visibilitychange', () => send({ type: 'flush' }));
  window.addEventListener('pagehide', () => send({ type: 'flush' }));
  window.addEventListener('resize', redraw);

  document.fonts.load('16px "Plex Mono"').finally(function () {
    start();
    redraw();
  });
})();
