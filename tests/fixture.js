'use strict';
module.exports=function() {
  var base=Date.UTC(2026,9,16,23,0,0)/1000, h={time:[],temperature_2m:[],precipitation_probability:[],weather_code:[],uv_index:[]};
  for(var i=0;i<48;i++) {
    h.time.push(base+i*3600);h.temperature_2m.push(i===0?-12:14);
    h.precipitation_probability.push(i===28?80:20);h.weather_code.push(2);h.uv_index.push(3.4);
  }
  return {utc_offset_seconds:3600,current:{time:base+10*3600+9*60,temperature_2m:-12,
    relative_humidity_2m:72,weather_code:2,wind_speed_10m:12,wind_direction_10m:225},hourly:h,
    daily:{time:[base,base+86400],sunrise:[base+7*3600+29*60,base+86400+7*3600+31*60],
      sunset:[base+18*3600+3*60,base+86400+18*3600+1*60],temperature_2m_max:[17,16],
      temperature_2m_min:[9,8],uv_index_max:[4.2,3.6]}};
};
