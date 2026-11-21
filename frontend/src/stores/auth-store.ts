import { create } from 'zustand';
import { AUTH_CHANGED_EVENT, hasValidToken } from '../lib/api/live-api';
import type { User } from '../lib/types';

const userKey = 'iot-monitor-user';

function storedUser(): User | null {
  // Без действующего JWT сохранённый пользователь — устаревшее состояние.
  if (!hasValidToken()) {
    localStorage.removeItem(userKey);
    return null;
  }
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

export const useAuth = create<Auth>((set) => {
  if (typeof window !== 'undefined') {
    window.addEventListener(AUTH_CHANGED_EVENT, () => set({ user: null }));
  }
  return {
    user: storedUser(),
    login: (user) => {
      localStorage.setItem(userKey, JSON.stringify(user));
      set({ user });
    },
    logout: () => {
      sessionStorage.removeItem('iot-monitor-live-token');
      sessionStorage.removeItem('iot-monitor-live-owner-id');
      sessionStorage.removeItem('iot-monitor-live-device-ids');
      localStorage.removeItem(userKey);
      set({ user: null });
    },
  };
});
