'use strict';
var assert = require('assert');
var makeSettings = require('../src/pkjs/kit/settings');
var makeConfig = require('../src/pkjs/kit/config');
var a = {heading:'Mondrian', storageKey:'mondrian-kit-settings-v1', largeLabels:true};
var b = {heading:'Underground', storageKey:'underground-kit-settings-v1', largeLabels:false};
function keys(schema) {
  return schema.reduce(function(out,item) {
    if(item.messageKey) out.push(item.messageKey);
    return item.items ? out.concat(keys(item.items)) : out;
  },[]);
}
var ma = makeConfig(a), mb = makeConfig(b);
assert.strictEqual(ma[0].defaultValue,'Mondrian');
assert.strictEqual(mb[0].defaultValue,'Underground');
assert(keys(ma).indexOf('LARGE_LABELS') >= 0);
assert.strictEqual(keys(mb).indexOf('LARGE_LABELS'),-1);
assert(keys(makeConfig(a)).indexOf('LARGE_LABELS') >= 0); // independent schemas
assert.throws(function(){ makeSettings({}); });
var memory = {}, storage = {
  getItem:function(k){return memory[k] || null;},
  setItem:function(k,v){memory[k]=v;}
};
memory[a.storageKey] = JSON.stringify({TIME_MODE:2,LARGE_LABELS:1,STEP_GOAL:18000});
memory[b.storageKey] = JSON.stringify({TIME_MODE:1,STEP_GOAL:9000});
var sa=makeSettings(a), sb=makeSettings(b);
assert.strictEqual(sa.load(storage).STEP_GOAL,18000);
assert.strictEqual(sa.load(storage).LARGE_LABELS,1);
assert.strictEqual(sb.load(storage).STEP_GOAL,9000);
sb.save(storage,{STEP_GOAL:12000});
assert.strictEqual(sa.load(storage).STEP_GOAL,18000);
assert.strictEqual(sb.load(storage).STEP_GOAL,12000);
delete memory[b.storageKey];
memory['clay-settings']=JSON.stringify({STEP_GOAL:21000});
assert.strictEqual(sb.load(storage).STEP_GOAL,21000); // preserve legacy fallback
console.log('Configuration tests passed: per-face UI, existing settings and isolated namespaces.');
