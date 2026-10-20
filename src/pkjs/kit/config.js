'use strict';
module.exports = function(options) {
var schema = [
  {type:'heading',defaultValue:options.heading,size:1},
  {type:'section',items:[
    {type:'heading',defaultValue:'Display'},
    {type:'select',messageKey:'TIME_MODE',label:'Time format',defaultValue:'0',options:[
      {label:'Watch setting',value:'0'},{label:'12-hour',value:'1'},{label:'24-hour',value:'2'}]},
    {type:'select',messageKey:'TEMP_UNIT',label:'Temperature',defaultValue:'0',options:[
      {label:'Celsius',value:'0'},{label:'Fahrenheit',value:'1'}]},
    {type:'toggle',messageKey:'LARGE_LABELS',label:'Larger labels',defaultValue:false},
    {type:'text',defaultValue:'Off preserves the original composition and compact pixel captions.'},
    {type:'select',messageKey:'WORLD',label:'Second time zone',defaultValue:'0',options:[
      {label:'New Zealand',value:'0'},{label:'UTC',value:'1'},{label:'London',value:'2'},
      {label:'New York',value:'3'},{label:'Los Angeles',value:'4'},{label:'Tokyo',value:'5'},
      {label:'Sydney',value:'6'}]}
  ]},
  {type:'section',items:[
    {type:'heading',defaultValue:'Data'},
    {type:'slider',messageKey:'STEP_GOAL',label:'Step goal',defaultValue:10000,min:1000,max:50000,step:500},
    {type:'toggle',messageKey:'WEATHER_ENABLED',label:'Weather',defaultValue:true},
    {type:'toggle',messageKey:'DEMO_FALLBACK',label:'Demo values when health is unavailable',defaultValue:false}
  ]},
  {type:'submit',defaultValue:'Save settings'}
];

if (!options.largeLabels) schema[1].items = schema[1].items.filter(function(item) {
  return item.messageKey !== 'LARGE_LABELS' && item.type !== 'text';
});
return schema;
};
