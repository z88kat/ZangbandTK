// Inside the Emscripten module, in the worker: the other half of main-web.c.
//
// Everything the C side calls through EM_JS lands here, and everything the
// page sends (keys, clicks, answers) arrives through worker.js into
// Module.zbInput and Module.zbAnswer.

Module.zbQueue = [];
Module.zbWake = null;
var zbFlushSoon = null;
Module.zbAnswerWaiting = null;

Module.zbPost = function (msg, transfer) {
  postMessage(msg, transfer || []);
};

// JSON from C goes up as text and is parsed on the page.
Module.zbPostJson = function (text) {
  postMessage({ type: 'json', text: text });
};

// A key or a click from the page: [kind, a, b, c].
Module.zbInput = function (ev) {
  Module.zbQueue.push(ev);
  if (Module.zbWake) {
    const wake = Module.zbWake;
    Module.zbWake = null;
    wake(Module.zbQueue.length);
  }
};

// Wake a waiting game with nothing to read, so that it looks again at what
// the page has asked for (a tileset, for one).
Module.zbPoke = function () {
  if (Module.zbWake) {
    const wake = Module.zbWake;
    Module.zbWake = null;
    wake(Module.zbQueue.length);
  }
};

// Called by the game when it wants input.  Blocking, it waits for some;
// otherwise it only lets the worker's event loop run once, so that a key
// pressed while the game rests or runs can arrive.
Module.zbWait = function (block) {
  // Each time the game settles to wait for the player, write whatever it saved
  // to IndexedDB soon after: a page closed straight after "Saving game...
  // done." would otherwise lose the save to the five-second timer.
  if (block && !zbFlushSoon) {
    zbFlushSoon = setTimeout(function () {
      zbFlushSoon = null;
      Module.zbFlush();
    }, 300);
  }
  if (Module.zbQueue.length) return Promise.resolve(Module.zbQueue.length);
  if (!block) {
    return new Promise(function (resolve) {
      const ch = new MessageChannel();
      ch.port1.onmessage = function () { resolve(Module.zbQueue.length); };
      ch.port2.postMessage(0);
    });
  }
  return new Promise(function (resolve) { Module.zbWake = resolve; });
};

Module.zbAsk = function (json) {
  postMessage({ type: 'ask', text: json });
  return new Promise(function (resolve) { Module.zbAnswerWaiting = resolve; });
};

Module.zbAnswer = function (value) {
  const waiting = Module.zbAnswerWaiting;
  Module.zbAnswerWaiting = null;
  if (waiting) waiting(value);
};

Module.zbSendFile = function (path, what) {
  try {
    const data = FS.readFile(path).slice();
    postMessage({ type: 'file', what: what, path: path, data: data }, [data.buffer]);
  } catch (e) {
    console.warn('cannot send ' + path + ': ' + e);
  }
};

// ---------------------------------------------------------------- savefiles --
// As src/wasm/pre.js does for the SDL2 build: the save, score and user
// folders live in IndexedDB, read before main() and written every few seconds.

var ZB_PERSISTED = ['/lib/save', '/lib/scores', '/lib/user', '/lib/panic',
                    '/lib/archive'];
var zbFlushTimer = null;

Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  ZB_PERSISTED.forEach(function (d) {
    try { FS.mkdirTree(d); } catch (e) { /* preloaded already */ }
    try { FS.mount(IDBFS, {}, d); } catch (e) { console.warn('mount ' + d + ': ' + e); }
  });
  Module.addRunDependency('idbfs-load');
  FS.syncfs(true, function (err) {
    if (err) console.warn('idbfs load: ' + err);
    Module.removeRunDependency('idbfs-load');
  });
});

Module.zbFlush = function (done) {
  try {
    FS.syncfs(false, function (e) {
      if (e) console.warn('idbfs save: ' + e);
      if (done) done(!e);
    });
  } catch (e) {
    if (done) done(false);
  }
};

Module.postRun = Module.postRun || [];
Module.postRun.push(function () {
  zbFlushTimer = setInterval(function () { Module.zbFlush(); }, 5000);
});

Module.zbQuit = function (why) {
  if (zbFlushTimer !== null) {
    clearInterval(zbFlushTimer);
    zbFlushTimer = null;
  }
  Module.zbFlush(function (saved) {
    postMessage({ type: 'quit', saved: saved, why: why });
  });
};
