'use strict';
module.exports = function(options,Clay) {
var config=require('./config')(options);
var storage=require('./settings')(options);
var enqueue=require('./queue')(function(payload,ok,fail) { Pebble.sendAppMessage(payload,ok,fail); });
var settings=storage.load(localStorage);
var clay=new Clay(config,null,{autoHandleEvents:false});
function syncClay() {
  var value={};Object.keys(settings).forEach(function(k){value[k]=settings[k];});
  value.WEATHER_ENABLED=!!value.WEATHER_ENABLED;value.DEMO_FALLBACK=!!value.DEMO_FALLBACK;
  value.LARGE_LABELS=!!value.LARGE_LABELS;
  (options.faceSettings || []).forEach(function(item) {
    if (item.type==='toggle') value[item.messageKey]=!!value[item.messageKey];
  });
  clay.setSettings(value);
}
function sendSettings() {
  var payload={READY:1};Object.keys(settings).forEach(function(k){payload[k]=settings[k];});
  enqueue(payload);
}
function position(ok,fail) {
  var done=false;
  var timer=setTimeout(function(){if(!done){done=true;fail('Location timeout');}},16000);
  navigator.geolocation.getCurrentPosition(function(p){if(!done){done=true;clearTimeout(timer);ok(p);}},
    function(e){if(!done){done=true;clearTimeout(timer);fail(e.message||'Location unavailable');}},
    {timeout:15000,maximumAge:1800000});
}
function http(url,ok,fail) {
  var xhr=new XMLHttpRequest(),done=false;
  function error(message){if(!done){done=true;fail(message);}}
  xhr.onload=function(){
    if(done)return;
    if(xhr.status>=200&&xhr.status<300){done=true;ok(xhr.responseText);}
    else error('HTTP '+xhr.status);
  };
  xhr.onerror=function(){error('Network error');};
  xhr.ontimeout=function(){error('HTTP timeout');};
  xhr.onabort=function(){error('HTTP aborted');};
  try {xhr.open('GET',url);xhr.timeout=15000;xhr.send();} catch(e){error(e.message);}
}
var requestWeather=require('./weather').client({
  position:position,http:http,send:enqueue,enabled:function(){return !!settings.WEATHER_ENABLED;},
  now:function(){return Math.floor(Date.now()/1000);},log:function(s){console.log(s);}
});
Pebble.addEventListener('ready',function(){syncClay();sendSettings();});
Pebble.addEventListener('appmessage',function(e){
  var id=e.payload.REQUEST_WEATHER;
  if(typeof id==='number'&&id>0)requestWeather(id);
});
Pebble.addEventListener('showConfiguration',function(){syncClay();Pebble.openURL(clay.generateUrl());});
Pebble.addEventListener('webviewclosed',function(e){
  if(!e||!e.response||e.response==='CANCELLED')return;
  try {
    settings=storage.normalize(clay.getSettings(e.response,false));
    storage.save(localStorage,settings);syncClay();sendSettings();
  } catch(err){console.log('Settings unchanged: '+err.message);}
});

};
