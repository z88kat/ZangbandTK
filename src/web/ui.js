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

  // The text screens want this much term: 80 columns is the game's floor and
  // the birth screen's list of races wants about thirty rows.
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
      case 'ask': openDialog(JSON.parse(m.text)); break;
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
