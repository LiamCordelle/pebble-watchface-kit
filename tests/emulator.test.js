'use strict';
var assert=require('assert'),fixture=require('../tools/emulator_weather_fixture'),weather=require('../src/pkjs/kit/weather');
[1792706340,1774746000,1792890000].forEach(function(now) {
  [0,3600,-18000,20700].forEach(function(offset) {
    var source=fixture(now,offset),bytes=weather.encode(source,now);
    assert.strictEqual(bytes.length,204);
    assert.strictEqual(source.hourly.time.length,48);
    assert(source.hourly.time[0]<=now && source.hourly.time[47]>now);
    assert.strictEqual(source.current.time,now);
  });
});
console.log('Synthetic emulator fixtures passed: fresh payloads, local dates, offsets and 48-hour coverage.');
