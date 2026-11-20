import { create } from 'zustand'; import type { User } from '../lib/types';
type Auth={user:User|null; login:(user:User)=>void; logout:()=>void};
export const useAuth=create<Auth>(set=>({user:null,login:user=>set({user}),logout:()=>set({user:null})}));
