'use strict';
var VALID_CODES = [0,1,2,3,45,48,51,53,55,56,57,61,63,65,66,67,71,73,75,77,80,81,82,85,86,95,96,99];
function numeric(v,min,max) {
  if (typeof v !== 'number' || !isFinite(v) || v < min || v > max) throw new Error('Invalid weather number');
  return Math.round(v);
}
function code(v) {
  if (VALID_CODES.indexOf(v) < 0) throw new Error('Invalid weather code');
  return v;
}
function uv10(v) {
  if (typeof v !== 'number' || !isFinite(v) || v < 0 || v > 30) throw new Error('Invalid UV');
  return Math.round(v*10);
}
function checksum(a,size) {
  var h = 2166136261;
  for (var i=0;i<size;i++) {
    h = (h ^ a[i]) >>> 0;
    h = (h + (h<<1) + (h<<4) + (h<<7) + (h<<8) + (h<<24)) >>> 0;
  }
  return h;
}
function dateKey(epoch,offset) {
  var d = new Date((epoch+offset)*1000);
  return d.getUTCFullYear()*10000+(d.getUTCMonth()+1)*100+d.getUTCDate();
}
function encode(data,now) {
  if (!data || !data.current || !data.hourly || !data.daily) throw new Error('Incomplete weather');
  var c=data.current,h=data.hourly,d=data.daily, bytes=[],i;
  var count=Math.min(48,(h.time||[]).length);
  if (count<24) throw new Error('Short forecast');
  var observed=numeric(c.time,1577836800,2147483647);
  if (observed>now+300 || now-observed>10800) throw new Error('Old/future observation');
  var base=numeric(h.time[0],1577836800,2147483647);
  var current=Math.floor((observed-base)/3600);
  if (current<0 || current>=count) throw new Error('Forecast does not cover now');
  for(i=0;i<204;i++) bytes.push(0);
  function u16(p,v) { bytes[p]=v&255; bytes[p+1]=(v>>>8)&255; }
  function u32(p,v) { u16(p,v);u16(p+2,v>>>16); }
  bytes[0]=77;bytes[1]=75;bytes[2]=1;bytes[3]=count;
  u32(4,observed);u32(8,base);
  var offset=numeric(data.utc_offset_seconds,-43200,50400);
  for(i=0;i<2;i++) {
    var day=numeric((d.time||[])[i],1577836800,2147483647);
    u32(12+i*4,dateKey(day,offset));
    var rise=(d.sunrise||[])[i],set=(d.sunset||[])[i];
    if (rise===null && set===null) { rise=0;set=0; }
    else {
      rise=numeric(rise,1577836800,2147483647);
      set=numeric(set,1577836800,2147483647);
      if (rise>=set) throw new Error('Invalid sun events');
    }
    u32(20+i*8,rise);u32(24+i*8,set);
    var hi=numeric((d.temperature_2m_max||[])[i],-90,65);
    var lo=numeric((d.temperature_2m_min||[])[i],-90,65);
    if (lo>hi) throw new Error('Inverted temperature range');
    bytes[45+i*2]=hi&255;bytes[46+i*2]=lo&255;
    u16(49+i*2,uv10((d.uv_index_max||[])[i]));
  }
  bytes[36]=numeric(c.temperature_2m,-90,65)&255;
  bytes[37]=code(c.weather_code);
  bytes[38]=numeric(c.relative_humidity_2m,0,100);
  u16(39,uv10((h.uv_index||[])[current]));
  u16(41,numeric(c.wind_speed_10m,0,500));
  u16(43,numeric(c.wind_direction_10m,0,360));
  for(i=0;i<count;i++) {
    if (h.time[i]!==base+i*3600) throw new Error('Non-contiguous hourly forecast');
    bytes[53+i]=numeric((h.temperature_2m||[])[i],-90,65)&255;
    bytes[101+i]=numeric((h.precipitation_probability||[])[i],0,100);
    bytes[149+i]=code((h.weather_code||[])[i]);
  }
  u32(200,checksum(bytes,200));
  return bytes;
}
function url(lat,lon) {
  numeric(lat,-90,90);numeric(lon,-180,180);
  return 'https://api.open-meteo.com/v1/forecast?latitude='+encodeURIComponent(lat)+
    '&longitude='+encodeURIComponent(lon)+
    '&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,wind_direction_10m'+
    '&hourly=temperature_2m,precipitation_probability,weather_code,uv_index'+
    '&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max'+
    '&forecast_days=2&timezone=auto&timeformat=unixtime';
}
function client(deps) {
  var busy=false,pending=0;
  function request(id) {
    if (!id || !deps.enabled()) return;
    if (busy) { pending=id;return; }
    busy=true;
    var finished=false;
    function finish(data,error) {
      if (finished) return;
      finished=true;
      var payload={WEATHER_ID:id};
      try {
        if (error) throw new Error(error);
        payload.WEATHER=encode(data,deps.now());
      } catch(e) { payload.WEATHER_ERROR=1;deps.log('Weather: '+e.message); }
      deps.send(payload);
      busy=false;
      if (pending) { var next=pending;pending=0;request(next); }
    }
    try { deps.position(function(pos) {
      try {
        deps.http(url(pos.coords.latitude,pos.coords.longitude),function(text) {
          try { finish(JSON.parse(text)); } catch(e) { finish(null,e.message); }
        },function(err) {finish(null,err);});
      } catch(e) {finish(null,e.message);}
    },function(err) {finish(null,err);}); } catch(e) {finish(null,e.message);}
  }
  return request;
}
module.exports={encode:encode,checksum:checksum,url:url,client:client};
