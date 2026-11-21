import type { Device, Rule, Telemetry, User } from '../types';
const now=Date.now(); const iso=(mins:number)=>new Date(now-mins*60000).toISOString().slice(0,19);
export const db:{users:User[];devices:Device[];telemetry:Telemetry[];rules:Rule[]}={users:[{id:1,email:'demo@iot.local',name:'Алексей Воронов',telegram_chat_id:'-100123456',role:'user'},{id:2,email:'admin@iot.local',name:'Мария Орлова',telegram_chat_id:null,role:'admin'}],devices:[['Датчик кухни','temperature'],['Серверная','temperature'],['Теплица','humidity'],['Холодильный склад','temperature'],['Котельная','pressure'],['Склад № 4','energy']].map(([name,type],i)=>({id:i+1,owner_id:1,name,type,created_at:iso(7000-i*90)})),telemetry:[],rules:[{rule_id:1,device_id:2,operator:'>',value:28},{rule_id:2,device_id:4,operator:'>',value:6},{rule_id:3,device_id:1,operator:'<',value:15}]};
// История за 30 дней с шагом 10 минут: быстрые диапазоны и календарь
// получают осмысленные данные, а карточки по-прежнему показывают последнее значение.
db.devices.forEach((device,index)=>{
  for(let ageMinutes=0;ageMinutes<=30*24*60;ageMinutes+=10){
    const timestamp=now-ageMinutes*60000;
    const daily=Math.sin((timestamp/86400000)*Math.PI*2+index*1.7)*3.2;
    const drift=Math.sin((timestamp/3600000)*0.7+index)*0.8;
    const noise=Math.sin(ageMinutes*0.37+index*2.1)*0.5;
    const spike=index===1&&ageMinutes<15?9.5:0;
    db.telemetry.push({id:db.telemetry.length+1,device_id:device.id,temperature:Number((18+index*1.7+daily+drift+noise+spike).toFixed(1)),time:new Date(timestamp).toISOString().slice(0,19)});
  }
});
let next=99; export const id=()=>++next;
