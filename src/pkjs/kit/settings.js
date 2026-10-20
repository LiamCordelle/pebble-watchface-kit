'use strict';
var DEFAULTS = {TIME_MODE:0,TEMP_UNIT:0,STEP_GOAL:10000,WORLD:0,
  WEATHER_ENABLED:1,DEMO_FALLBACK:0,LARGE_LABELS:0};
module.exports = function(options) {
if (!options || !options.storageKey) throw new Error('A settings storageKey is required');
var STORAGE_KEY = options.storageKey;
function number(value,min,max,fallback) {
  if (value === null || value === '' || typeof value === 'boolean') return fallback;
  var n = Number(value);
  return isFinite(n) ? Math.max(min,Math.min(max,Math.round(n))) : fallback;
}
function flag(value,fallback) {
  if (value === true || value === 1 || value === '1' || value === 'true') return 1;
  if (value === false || value === 0 || value === '0' || value === 'false') return 0;
  return fallback;
}
function normalize(raw) {
  raw = raw || {};
  var flat = {};
  Object.keys(raw).forEach(function(k) {
    flat[k] = raw[k] && typeof raw[k] === 'object' ? raw[k].value : raw[k];
  });
  return {TIME_MODE:number(flat.TIME_MODE,0,2,0),TEMP_UNIT:number(flat.TEMP_UNIT,0,1,0),
    STEP_GOAL:number(flat.STEP_GOAL,1000,50000,10000),WORLD:number(flat.WORLD,0,6,0),
    WEATHER_ENABLED:flag(flat.WEATHER_ENABLED,1),DEMO_FALLBACK:flag(flat.DEMO_FALLBACK,0),
    LARGE_LABELS:flag(flat.LARGE_LABELS,0)};
}
function load(storage) {
  try { return normalize(JSON.parse(storage.getItem(STORAGE_KEY) || storage.getItem('clay-settings') || '{}')); }
  catch(e) { return normalize(DEFAULTS); }
}
function save(storage,value) { storage.setItem(STORAGE_KEY,JSON.stringify(normalize(value))); }
return {defaults:DEFAULTS,normalize:normalize,load:load,save:save};
};
