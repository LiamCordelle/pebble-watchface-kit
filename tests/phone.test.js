'use strict';
var assert=require('assert'),fs=require('fs'),path=require('path');
var weather=require('../src/pkjs/kit/weather'),fixture=require('./fixture');
var settings=require('../src/pkjs/kit/settings')({storageKey:'test-kit-settings-v1'}),makeQueue=require('../src/pkjs/kit/queue');
var source=fixture(),now=source.current.time,bytes=weather.encode(source,now);
assert.strictEqual(bytes.length,204);assert.strictEqual(bytes[36],244);
assert.strictEqual(bytes[39]+bytes[40]*256,34);assert.strictEqual(bytes[49],42);
assert.strictEqual(bytes[101+28],80);
assert(weather.url(51.5,-0.1).indexOf('forecast_days=2')>=0);
assert(weather.url(51.5,-0.1).indexOf('timeformat=unixtime')>=0);
function rejects(change) {var v=fixture();change(v);assert.throws(function(){weather.encode(v,now);});}
rejects(function(v){v.current.temperature_2m=null;});
rejects(function(v){v.hourly.uv_index[10]=null;});
rejects(function(v){v.daily.uv_index_max[0]=null;});
rejects(function(v){v.hourly.weather_code[3]=4;});
rejects(function(v){v.hourly.precipitation_probability[0]=101;});
rejects(function(v){v.hourly.temperature_2m[0]=NaN;});
rejects(function(v){v.hourly.time[2]++;});
rejects(function(v){v.hourly.time.length=23;});
rejects(function(v){v.daily.temperature_2m_min[0]=30;});
rejects(function(v){v.daily.sunset[0]=null;});
rejects(function(v){v.current.time=now+301;});
rejects(function(v){v.current.time=now-10801;});
var polar=fixture();polar.daily.sunrise=[null,null];polar.daily.sunset=[null,null];
assert.strictEqual(weather.encode(polar,now)[20],0);
assert.strictEqual(settings.normalize({}).STEP_GOAL,10000);
assert.strictEqual(settings.normalize({}).LARGE_LABELS,0);
assert.strictEqual(settings.normalize({WEATHER_ENABLED:'false'}).WEATHER_ENABLED,0);
assert.strictEqual(settings.normalize({LARGE_LABELS:{value:true}}).LARGE_LABELS,1);
assert.strictEqual(settings.normalize({STEP_GOAL:'18000',WORLD:'6'}).STEP_GOAL,18000);
var memory={};var store={getItem:function(k){return memory[k]||null;},setItem:function(k,v){memory[k]=v;}};
settings.save(store,{TIME_MODE:1,STEP_GOAL:18000,LARGE_LABELS:1});
assert.strictEqual(settings.load(store).LARGE_LABELS,1);
assert.strictEqual(settings.load(store).TIME_MODE,1);

// An artificial clock lets ACKs, send failures and missing callbacks be ordered precisely.
var clock=0,timers=[],sent=[],active=0,maxActive=0;
function schedule(fn,delay){var t={at:clock+delay,fn:fn,cancel:false};timers.push(t);return t;}
function cancel(t){t.cancel=true;}
function advance(ms){clock+=ms;var again=true;while(again){again=false;
  for(var i=0;i<timers.length;i++)if(!timers[i].cancel&&timers[i].at<=clock){
    var t=timers[i];t.cancel=true;t.fn();again=true;break;
  }
}}
var enqueue=makeQueue(function(p,ok,fail){active++;maxActive=Math.max(maxActive,active);
  sent.push({p:p,ok:function(){active--;ok();},fail:function(){active--;fail();}});
},schedule,cancel);
enqueue({a:1});enqueue({a:2});assert.strictEqual(sent.length,1);
sent[0].fail();advance(999);assert.strictEqual(sent.length,1);
advance(1);assert.strictEqual(sent.length,2);assert.strictEqual(sent[1].p.a,1);
sent[1].ok();assert.strictEqual(sent[2].p.a,2);sent[2].ok();
assert.strictEqual(maxActive,1);
// A stale duplicate callback must not remove the next queued item.
sent[1].ok();enqueue({a:3});assert.strictEqual(sent[3].p.a,3);sent[3].ok();
enqueue({a:4});enqueue({a:5});var count=sent.length;
advance(60000);assert.strictEqual(sent.length,count);
sent[count-1].ok();assert.strictEqual(sent[count].p.a,5);sent[count].ok();

var positions=[],responses=[],messages=[],requests=0,enabled=true;
var client=weather.client({enabled:function(){return enabled;},now:function(){return now;},log:function(){},
  position:function(ok,fail){positions.push({ok:ok,fail:fail});},
  http:function(url,ok,fail){requests++;responses.push({ok:ok,fail:fail});},send:function(v){messages.push(v);}});
client(101);client(102);assert.strictEqual(positions.length,1);
positions[0].ok({coords:{latitude:51.5,longitude:-0.1}});assert.strictEqual(requests,1);
responses[0].ok(JSON.stringify(fixture()));assert.strictEqual(messages[0].WEATHER_ID,101);
assert.strictEqual(positions.length,2);positions[1].fail('no GPS');
assert.strictEqual(messages[1].WEATHER_ID,102);assert.strictEqual(messages[1].WEATHER_ERROR,1);
enabled=false;client(103);assert.strictEqual(positions.length,2);
var broken=weather.client({enabled:function(){return true;},now:function(){return now;},log:function(){},
  position:function(){throw new Error('Unavailable');},send:function(v){messages.push(v);}});
broken(104);assert.strictEqual(messages[2].WEATHER_ERROR,1);

fs.mkdirSync(path.join(__dirname,'../build/tests'),{recursive:true});
fs.writeFileSync(path.join(__dirname,'../build/tests/weather.bin'),Buffer.from(bytes));
console.log('Phone tests passed: protocol, invalid data, settings, serialized ACK/retry queue and weather fetches.');
