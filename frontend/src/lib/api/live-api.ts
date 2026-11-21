import type { Device, Rule, Telemetry, TimeRange, User } from '../types';

const core = import.meta.env.VITE_CORE_BASE ?? '/api/core';
const rulesBase = import.meta.env.VITE_RULES_BASE ?? '/api/rules';
const notifications = import.meta.env.VITE_NOTIFICATIONS_BASE ?? '/api/notifications';
const deviceKey = 'iot-monitor-live-device-ids';
const ownerKey = 'iot-monitor-live-owner-id';
const tokenKey = 'iot-monitor-live-token';
const userKey = 'iot-monitor-user';

// Auth state lives in two storages: token in sessionStorage, user in localStorage.
// Keep both in sync so a stale user never produces tokenless API calls.
export const AUTH_CHANGED_EVENT = 'iot-auth-changed';

export function clearSession(): void {
  sessionStorage.removeItem(tokenKey);
  sessionStorage.removeItem(ownerKey);
  sessionStorage.removeItem(deviceKey);
  localStorage.removeItem(userKey);
  window.dispatchEvent(new Event(AUTH_CHANGED_EVENT));
}

export function tokenPayload(): Record<string, unknown> | null {
  const token = sessionStorage.getItem(tokenKey);
  if (!token) return null;
  try {
    const encoded = token.split('.')[1].replace(/-/g, '+').replace(/_/g, '/');
    return JSON.parse(atob(encoded)) as Record<string, unknown>;
  } catch {
    return null;
  }
}

export function hasValidToken(): boolean {
  const payload = tokenPayload();
  if (!payload) return false;
  const exp = Number(payload.exp);
  return Number.isFinite(exp) && exp > Date.now() / 1000;
}

type Json = Record<string, unknown> | unknown[];

async function request<T extends Json>(base: string, path: string, init: RequestInit = {}): Promise<T> {
  const token = sessionStorage.getItem(tokenKey);
  const response = await fetch(`${base}${path}`, {
    ...init,
    headers: {
      'Content-Type': 'application/json',
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
      ...(init.headers ?? {}),
    },
  });
  const text = await response.text();
  const body: unknown = text ? JSON.parse(text) : undefined;
  if (response.status === 401 && !path.startsWith('/auth/login')) {
    clearSession();
    throw new Error('Сессия истекла, войдите снова');
  }
  if (!response.ok) {
    const message = typeof body === 'object' && body !== null && 'message' in body
      ? String(body.message) : `HTTP ${response.status}`;
    throw new Error(message);
  }
  const objectBody = body as Record<string, unknown> | null;
  if (objectBody && !Array.isArray(body) && objectBody.status === 'error') {
    throw new Error(String(objectBody.message ?? 'Ошибка запроса'));
  }
  return body as T;
}

function ids(): number[] { try { return JSON.parse(sessionStorage.getItem(deviceKey) ?? '[]') as number[]; } catch { return []; } }
function remember(id: number): void { sessionStorage.setItem(deviceKey, JSON.stringify([...new Set([...ids(), id])])); }
function forget(id: number): void { sessionStorage.setItem(deviceKey, JSON.stringify(ids().filter(value => value !== id))); }
function deviceFrom(raw: Record<string, unknown>): Device {
  return { id: Number(raw.id), owner_id: Number(raw.owner_id), name: String(raw.name), type: String(raw.type), created_at: '' };
}

export const liveApi = {
  async login(email: string, password: string): Promise<User> {
    const auth = await request<Record<string, unknown>>(core, '/auth/login', {
      method: 'POST',
      body: JSON.stringify({ email, password }),
    });
    const id = Number(auth.user_id);
    sessionStorage.setItem(tokenKey, String(auth.token));
    sessionStorage.setItem(ownerKey, String(id));
    const raw = await request<Record<string, unknown>>(core, `/users?id=${id}`);
    return {
      id,
      email: String(raw.email ?? email),
      name: String(raw.name ?? ''),
      telegram_chat_id: raw.telegram_chat_id === null ? null : String(raw.telegram_chat_id ?? ''),
      role: 'user',
    };
  },
  async register(name: string, email: string, password: string, telegramChatId = ''): Promise<User> {
    await request(core, '/users', {
      method: 'POST',
      body: JSON.stringify({ name, email, password, telegram_chat_id: telegramChatId }),
    });
    return this.login(email, password);
  },
  async devices(): Promise<Device[]> {
    // Список устройств определяется токеном, owner_id в запросе не нужен.
    const result = await request<Record<string, unknown>[]>(core, '/devices');
    return result.map(deviceFrom);
  },
  async device(id: number): Promise<Device> { const result = await request<Record<string, unknown>>(core, `/devices?id=${id}`); remember(id); return deviceFrom(result); },
  async saveDevice(data: Omit<Device, 'id'|'owner_id'|'created_at'>, existing?: Device): Promise<Device> {
    if (existing) { await request(core, `/devices?id=${existing.id}`, { method: 'PUT', body: JSON.stringify({ ...data, owner_id: existing.owner_id }) }); return { ...existing, ...data }; }
    const result = await request<Record<string, unknown>>(core, '/devices', { method: 'POST', body: JSON.stringify(data) });
    const ownerId = Number(tokenPayload()?.sub ?? 0);
    const device = { ...data, id: Number(result.device_id), owner_id: ownerId, created_at: '' }; remember(device.id); return device;
  },
  async deleteDevice(id: number): Promise<void> { await request(core, '/devices', { method: 'DELETE', body: JSON.stringify({ device_id: id }) }); forget(id); },
  async telemetry(
    deviceId: number,
    range?: TimeRange,
    afterId?: number,
    allTime = false,
  ): Promise<Telemetry[]> {
    return request<Telemetry[]>(core, '/telemetry/query', {
      method: 'POST',
      body: JSON.stringify({
        device_id: deviceId,
        ...(range
          ? {
              from: new Date(range.from).toISOString(),
              to: new Date(range.to).toISOString(),
            }
          : {}),
        ...(afterId === undefined ? {} : { after_id: afterId }),
        ...(allTime ? { all_time: true } : {}),
      }),
    });
  },
  async send(deviceId: number, value: number): Promise<void> { await request(core, '/telemetry', { method: 'POST', body: JSON.stringify({ device_id: deviceId, temperature: value }) }); },
  async clearTelemetry(deviceId: number): Promise<void> { await request(core, '/telemetry', { method: 'DELETE', body: JSON.stringify({ device_id: deviceId }) }); },
  async deleteUser(userId: number): Promise<void> { await request(core, '/users', { method: 'DELETE', body: JSON.stringify({ user_id: userId }) }); clearSession(); },
  async sendEmailAlert(email: string): Promise<void> { await request(notifications, '/notifications/email', { method: 'POST', body: JSON.stringify({ email, event_type: 'Тестовый алерт', sensor_name: 'Тестовый датчик', sensor_id: '1', reading_value: '31.5 °C', threshold: '30.0 °C', location: 'Тестовый стенд', severity: 'WARNING', description: 'Тестовое уведомление, отправленное из интерфейса.' }) }); },
  async rules(deviceId?: number): Promise<Rule[]> {
    if (deviceId) return request<Rule[]>(rulesBase, `/rules?device_id=${deviceId}`);
    // Rule Engine exposes rules per device. Fetch every device visible to the
    // current owner so the rules screen and dashboard stay complete.
    const devices = await this.devices();
    const groups = await Promise.all(devices.map(device => request<Rule[]>(rulesBase, `/rules?device_id=${device.id}`)));
    return groups.flat();
  },
  async saveRule(data: Omit<Rule, 'rule_id'>, existing?: Rule): Promise<Rule> { if (existing) { await request(rulesBase, `/rules/${existing.rule_id}`, { method: 'PUT', body: JSON.stringify(data) }); return { ...existing, ...data }; } const result = await request<Record<string, unknown>>(rulesBase, '/rules', { method: 'POST', body: JSON.stringify(data) }); return { ...data, rule_id: Number(result.rule_id) }; },
  async deleteRule(id: number): Promise<void> { await request(rulesBase, `/rules/${id}`, { method: 'DELETE' }); },
  async resetNotificationCooldown(deviceId: number): Promise<void> { await request(rulesBase, '/notification-cooldown/reset', { method: 'POST', body: JSON.stringify({ device_id: deviceId }) }); },
  async updateUser(user: User, patch: Partial<User>): Promise<User> {
    const next = { ...user, ...patch };
    await request(core, `/users?id=${user.id}`, { method: 'PUT', body: JSON.stringify({ email: next.email, name: next.name, telegram_chat_id: next.telegram_chat_id }) });
    return next;
  },
  async testEmail(email: string): Promise<void> { await request(notifications, '/notifications/email/test', { method: 'POST', body: JSON.stringify({ email }) }); },
  async testTelegram(device: Device, user: User, threshold: number, reading: number): Promise<void> { await request(notifications, '/notifications/temperature', { method: 'POST', body: JSON.stringify({ device_id: null, chat_id: user.telegram_chat_id, email: user.email, device_name: device.name, temperature: reading, threshold }) }); },
};
