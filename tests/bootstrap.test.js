'use strict';
var assert=require('assert');
var start=require('../src/pkjs/kit/bootstrap');
['mondrian','underground','custom'].forEach(function(name) {
  var key=name+'-kit-settings-v1', handlers={}, sent=[], memory={}, rendered;
  memory[key]=JSON.stringify({TIME_MODE:2,STEP_GOAL:18000,TEMP_UNIT:1});
  global.localStorage={getItem:function(k){return memory[k]||null;},
    setItem:function(k,v){memory[k]=v;}};
  global.Pebble={
    addEventListener:function(event,handler){handlers[event]=handler;},
    sendAppMessage:function(value,ok){sent.push(value);ok();},
    openURL:function(value){assert.strictEqual(value,'configuration-url');}
  };
  function Clay(schema,unused,options) {
    assert.strictEqual(options.autoHandleEvents,false);
    assert.strictEqual(schema[0].defaultValue,name);
    this.setSettings=function(value){rendered=value;};
    this.generateUrl=function(){return 'configuration-url';};
    this.getSettings=function(){return {TIME_MODE:1,TEMP_UNIT:0,STEP_GOAL:9000,LARGE_LABELS:true,CUSTOM_LIT:true,CUSTOM_MODE:2};};
  }
  start({heading:name,storageKey:key,largeLabels:name==='mondrian',faceSettings:name==='custom' ? [
    {type:'toggle',messageKey:'CUSTOM_LIT',defaultValue:false},
    {type:'select',messageKey:'CUSTOM_MODE',defaultValue:'0',min:0,max:2}] : []},Clay);
  handlers.ready();
  assert.strictEqual(sent[0].READY,1);
  assert.strictEqual(sent[0].STEP_GOAL,18000);
  assert.strictEqual(rendered.TEMP_UNIT,1);
  assert.strictEqual(typeof rendered.WEATHER_ENABLED,'boolean');
  handlers.showConfiguration();
  handlers.webviewclosed({response:'CANCELLED'});
  assert.strictEqual(sent.length,1);
  handlers.webviewclosed({response:'settings-response'});
  assert.strictEqual(sent[1].TIME_MODE,1);
  assert.strictEqual(sent[1].LARGE_LABELS,1);
  if(name==='custom') {
    assert.strictEqual(sent[1].CUSTOM_LIT,1);
    assert.strictEqual(sent[1].CUSTOM_MODE,2);
    assert.strictEqual(rendered.CUSTOM_LIT,true);
    assert.strictEqual(JSON.parse(memory[key]).CUSTOM_MODE,2);
  }
  assert.strictEqual(JSON.parse(memory[key]).STEP_GOAL,9000);
});
delete global.localStorage;
delete global.Pebble;
console.log('Phone bootstrap passed: existing settings, ready/send, Clay opening/save and cancellation.');

var textHandlers={},textSent=[],textMemory={},textRendered;
global.localStorage={getItem:function(k){return textMemory[k]||null;},setItem:function(k,v){textMemory[k]=v;}};
global.Pebble={addEventListener:function(k,v){textHandlers[k]=v;},sendAppMessage:function(v,ok){textSent.push(v);ok();}};
function TextClay(){this.setSettings=function(v){textRendered=v;};this.getSettings=function(){return {CAPTION:'custom'};};}
start({heading:'Text',storageKey:'text',faceSettings:[{type:'input',messageKey:'CAPTION',defaultValue:'DEFAULT',attributes:{maxlength:8}}],
 normalizeFaceSetting:function(key,value){return value.toUpperCase();}},TextClay);
textHandlers.ready();assert.strictEqual(textSent[0].CAPTION,'DEFAULT');
textHandlers.webviewclosed({response:'save'});assert.strictEqual(textSent[1].CAPTION,'CUSTOM');
assert.strictEqual(textRendered.CAPTION,'CUSTOM');assert.strictEqual(JSON.parse(textMemory.text).CAPTION,'CUSTOM');
delete global.localStorage;delete global.Pebble;
console.log('Text settings pass Clay ready/save and serialized transport.');
