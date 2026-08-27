// Browser-side support for the wasm build: savefile persistence, an end-of-game
// panel, and reclaiming the Escape key in fullscreen.

// ---------------------------------------------------------------- savefiles --
//
// The wasm filesystem is a JavaScript object that dies with the page, so the
// save and scores directories are mounted on IndexedDB instead, which does not.
// addRunDependency holds main() back until the first sync has read whatever is
// already stored -- without it the game starts, finds an empty save directory,
// and offers to roll a new character over the top of an existing one.

var ZBTK_PERSISTED = ['/lib/save', '/lib/scores', '/lib/user', '/lib/panic',
                      '/lib/archive'];
var zbtkFlushTimer = null;

Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  ZBTK_PERSISTED.forEach(function (d) {
    try { FS.mkdirTree(d); } catch (e) { /* preloaded already */ }
    try { FS.mount(IDBFS, {}, d); } catch (e) { console.warn('mount ' + d + ': ' + e); }
  });
  Module.addRunDependency('idbfs-load');
  FS.syncfs(true, function (err) {
    if (err) console.warn('idbfs load: ' + err);
    Module.removeRunDependency('idbfs-load');
  });
});

// Push the other way often enough that a closed tab does not cost a session.
// The game writes its savefile at its own moments and does not tell us about
// them, so this polls rather than hooks.  The exception is quitting, which
// main-sdl2.c reports explicitly -- see Module.zbtkOnQuit below.
Module.postRun = Module.postRun || [];
Module.postRun.push(function () {
  var flush = function () {
    try {
      FS.syncfs(false, function (e) { if (e) console.warn('idbfs save: ' + e); });
    } catch (e) { /* filesystem gone; nothing to do */ }
  };
  zbtkFlushTimer = setInterval(flush, 5000);
  window.addEventListener('visibilitychange', flush);
  window.addEventListener('pagehide', flush);
});

// ------------------------------------------------------------------- quitting --
//
// Called from quit_hook in main-sdl2.c, which is the last C code to run before
// exit().  SDL_Quit has already destroyed the renderer by this point, so the
// canvas is blank and the only thing left that can speak to the player is the
// DOM.  Flush first and put the panel up when the flush lands, so the button
// is never offered before the game is actually safe on disk.
Module.zbtkOnQuit = function () {
  if (zbtkFlushTimer !== null) {
    clearInterval(zbtkFlushTimer);
    zbtkFlushTimer = null;
  }

  var show = function (saved) {
    if (document.getElementById('zbtk-exit')) return;

    var pane = document.createElement('div');
    pane.id = 'zbtk-exit';
    pane.style.cssText = 'position:fixed;inset:0;z-index:9999;display:flex;' +
      'align-items:center;justify-content:center;background:rgba(8,8,10,0.92);' +
      'font:16px/1.5 ui-sans-serif,system-ui,sans-serif;color:#e8e6e3';

    var card = document.createElement('div');
    card.style.cssText = 'text-align:center;padding:2.5rem 3rem;max-width:32rem;' +
      'border:1px solid #34323a;border-radius:10px;background:#16151a';

    var h = document.createElement('div');
    h.textContent = 'ZangbandTK has exited';
    h.style.cssText = 'font-size:1.35rem;font-weight:600;margin-bottom:0.6rem';

    var p = document.createElement('div');
    p.textContent = saved
      ? 'Your game has been saved to this browser.'
      : 'Your game may not have been saved -- see the console for details.';
    p.style.cssText = 'color:' + (saved ? '#a8a5ad' : '#e0a03a') +
      ';margin-bottom:1.6rem';

    var btn = document.createElement('button');
    btn.textContent = 'Start again';
    btn.style.cssText = 'font:inherit;font-weight:600;padding:0.6rem 1.5rem;' +
      'border:0;border-radius:6px;background:#c9a227;color:#1a1710;cursor:pointer';
    btn.onclick = function () { window.location.reload(); };

    card.appendChild(h); card.appendChild(p); card.appendChild(btn);
    pane.appendChild(card);
    document.body.appendChild(pane);
    btn.focus();
  };

  // Reloading automatically would be tidier, right up to the first time the
  // game quits during startup -- then it is a reload loop hammering the server
  // with no way for the player to stop it.  One button, one click.
  try {
    FS.syncfs(false, function (err) {
      if (err) console.warn('idbfs final save: ' + err);
      show(!err);
    });
  } catch (e) {
    console.warn('idbfs final save: ' + e);
    show(false);
  }
};
