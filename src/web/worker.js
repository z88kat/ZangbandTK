// The game's thread.  It loads the wasm build and passes the page's messages
// into it; web/pre.js, inside the module, does the rest.

'use strict';

let game = null;

self.onmessage = function (e) {
  const m = e.data;

  switch (m.type) {
    case 'start':
      importScripts('zangband-web.js');
      game = {
        zbCols: m.cols,
        zbRows: m.rows,
        zbTileW: m.tw,
        zbTileH: m.th,
        zbGraf: m.graf,
        arguments: [],
        print: function (t) { console.log(t); },
        printErr: function (t) { console.warn(t); },
        setStatus: function (t) { postMessage({ type: 'loading', text: t }); },
      };
      // pre.js fills in the zb* functions as soon as this is called.
      createZangband(game).catch(function (err) {
        postMessage({ type: 'error', text: String(err) });
      });
      break;
    case 'key':
      if (game) game.zbInput([1, m.code, m.mods, 0]);
      break;
    case 'mouse':
      if (game) game.zbInput([2, m.x, m.y, m.button]);
      break;
    case 'answer':
      if (game) game.zbAnswer(m.value);
      break;
    case 'graf':
      if (game) {
        game.zbWantGraf = m.id;
        game.zbPoke();
      }
      break;
    case 'flush':
      if (game && game.zbFlush) game.zbFlush();
      break;
  }
};
