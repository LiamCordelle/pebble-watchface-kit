'use strict';
// All settings, weather and errors use this one acknowledged FIFO.
module.exports = function(send,schedule,cancel,log) {
  var items = [], busy = false, timer = null;
  schedule = schedule || setTimeout; cancel = cancel || clearTimeout; log = log || function(){};
  function pump() {
    if (busy || !items.length) return;
    busy = true;
    var item = items[0], settled = false;
    function finish(ok) {
      if (settled) return;
      settled = true;
      if (timer !== null) cancel(timer);
      timer = null;
      if (ok || ++item.tries >= 3) {
        if (!ok) log('AppMessage abandoned after three attempts');
        items.shift(); busy = false; pump();
      } else {
        timer = schedule(function() {timer = null; busy = false; pump();},1000 * item.tries);
      }
    }
    // Only a native ACK/NACK ends a transport operation. A local timeout must
    // never open a second native send while the first is still outstanding.
    try { send(item.payload,function(){finish(true);},function(){finish(false);}); }
    catch(e) { finish(false); }
  }
  return function(payload) { items.push({payload:payload,tries:0}); pump(); };
};
