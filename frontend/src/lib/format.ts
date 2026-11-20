export const temperature=(v:number)=>`${new Intl.NumberFormat('ru-RU',{minimumFractionDigits:1,maximumFractionDigits:1}).format(v)} °C`;
export const msk=(utc:string)=>new Intl.DateTimeFormat('ru-RU',{dateStyle:'short',timeStyle:'short',timeZone:'Europe/Moscow'}).format(new Date(utc.endsWith('Z')?utc:utc+'Z'))+' MSK';
export const relative=(utc:string)=>{const m=Math.round((Date.now()-new Date(utc+'Z').getTime())/60000);return m<1?'только что':m<60?`${m} мин назад`:m<1440?`${Math.round(m/60)} ч назад`:`${Math.round(m/1440)} дн назад`;};
