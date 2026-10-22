'use strict';
// Synthetic protocol fixture, not a real weather/geolocation observation.
var fs=require('fs'),path=require('path'),weather=require('../src/pkjs/kit/weather');
function fixture(now,offset) {
  var base=Math.floor((now+offset)/86400)*86400-offset;
  var hourly={time:[],temperature_2m:[],precipitation_probability:[],weather_code:[],uv_index:[]};
  for(var i=0;i<48;i++) {
    hourly.time.push(base+i*3600);hourly.temperature_2m.push(14);
    hourly.precipitation_probability.push(i===Math.floor((now-base)/3600)+2?60:20);
    hourly.weather_code.push(2);hourly.uv_index.push(3);
  }
  return {utc_offset_seconds:offset,
    current:{time:now,temperature_2m:14,relative_humidity_2m:72,weather_code:2,wind_speed_10m:12,wind_direction_10m:225},hourly:hourly,
    daily:{time:[base,base+86400],sunrise:[base+7*3600,base+31*3600],sunset:[base+18*3600,base+42*3600],
      temperature_2m_max:[17,17],temperature_2m_min:[9,9],uv_index_max:[4,4]}};
}
module.exports=fixture;
if(require.main===module) {
  var output=process.argv[2],now=process.argv[3]===undefined?Math.floor(Date.now()/1000):Number(process.argv[3]);
  if(!output)throw new Error('Usage: node emulator_weather_fixture.js output-directory [epoch]');
  var data=fixture(now,-new Date(now*1000).getTimezoneOffset()*60);
  fs.mkdirSync(output,{recursive:true});
  fs.writeFileSync(path.join(output,'weather.json'),JSON.stringify(data,null,2)+'\n');
  fs.writeFileSync(path.join(output,'weather.bin'),Buffer.from(weather.encode(data,now)));
}
