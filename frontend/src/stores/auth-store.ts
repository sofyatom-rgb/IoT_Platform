import { create } from 'zustand';
import type { User } from '../lib/types';

const userKey = 'iot-monitor-user';

function storedUser(): User | null {
  try {
    const raw = localStorage.getItem(userKey);
    return raw ? (JSON.parse(raw) as User) : null;
  } catch {
    return null;
  }
}

type Auth = {
  user: User | null;
  login: (user: User) => void;
  logout: () => void;
};

export const useAuth = create<Auth>((set) => ({
  user: storedUser(),
  login: (user) => {
    localStorage.setItem(userKey, JSON.stringify(user));
    set({ user });
  },
  logout: () => {
    localStorage.removeItem(userKey);
    sessionStorage.removeItem('iot-monitor-live-token');
    set({ user: null });
  },
}));
