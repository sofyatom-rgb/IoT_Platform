export type Role = 'admin'|'user';
export type User = {id:number;email:string;name:string;telegram_chat_id:string|null;role:Role};
export type Device = {id:number;owner_id:number;name:string;type:string;created_at:string};
export type Telemetry = {id:number;device_id:number;temperature:number;time:string};
export type TimeRange = {from:number;to:number};
export type Rule = {rule_id:number;device_id:number;operator:'>'|'<'|'='|'!=';value:number};
export type Status = 'online'|'stale'|'offline'|'alert';
