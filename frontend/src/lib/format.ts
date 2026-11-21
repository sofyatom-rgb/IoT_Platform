export const temperature=(v:number)=>`${new Intl.NumberFormat('ru-RU',{minimumFractionDigits:1,maximumFractionDigits:1}).format(v)} °C`;
export const parseTimestamp=(value:string)=>new Date(/(?:Z|[+-]\d{2}:?\d{2})$/i.test(value)?value:`${value}Z`);
export const msk=(utc:string)=>new Intl.DateTimeFormat('ru-RU',{dateStyle:'short',timeStyle:'short',timeZone:'Europe/Moscow'}).format(parseTimestamp(utc))+' MSK';
export const relative=(utc:string)=>{const m=Math.round((Date.now()-parseTimestamp(utc).getTime())/60000);return m<1?'только что':m<60?`${m} мин назад`:m<1440?`${Math.round(m/60)} ч назад`:`${Math.round(m/1440)} дн назад`;};
