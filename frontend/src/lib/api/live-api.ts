import type { Device, Rule, Telemetry, TimeRange, User } from '../types';

const core = import.meta.env.VITE_CORE_BASE ?? '/api/core';
const rulesBase = import.meta.env.VITE_RULES_BASE ?? '/api/rules';
const notifications = import.meta.env.VITE_NOTIFICATIONS_BASE ?? '/api/notifications';
const deviceKey = 'iot-monitor-live-device-ids';
const ownerKey = 'iot-monitor-live-owner-id';

type Json = Record<string, unknown> | unknown[];

async function request<T extends Json>(base: string, path: string, init: RequestInit = {}): Promise<T> {
  const response = await fetch(`${base}${path}`, {
    ...init,
    headers: { 'Content-Type': 'application/json', ...(init.headers ?? {}) },
  });
  const text = await response.text();
  const body: unknown = text ? JSON.parse(text) : undefined;
  if (!response.ok) {
    const message = typeof body === 'object' && body !== null && 'message' in body
      ? String(body.message) : `HTTP ${response.status}`;
    throw new Error(message);
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
  async login(email: string): Promise<User> {
    const user = await request<Record<string, unknown>>(core, '/auth/login', { method: 'POST', body: JSON.stringify({ email }) });
    const result = { id: Number(user.id), email: String(user.email), name: String(user.name ?? ''), telegram_chat_id: user.telegram_chat_id === null ? null : String(user.telegram_chat_id ?? ''), role: 'user' as const };
    sessionStorage.setItem(ownerKey, String(result.id));
    return result;
  },
  async register(name: string, email: string, password: string, telegramChatId = ''): Promise<User> {
    const result = await request<Record<string, unknown>>(core, '/users', { method: 'POST', body: JSON.stringify({ name, email, password, telegram_chat_id: telegramChatId }) });
    const id = Number(result.user_id); sessionStorage.setItem(ownerKey, String(id));
    return { id, name, email, telegram_chat_id: telegramChatId || null, role: 'user' };
  },
  async devices(): Promise<Device[]> {
    const ownerId = Number(sessionStorage.getItem(ownerKey) ?? import.meta.env.VITE_LIVE_OWNER_ID);
    if (!Number.isInteger(ownerId) || ownerId <= 0) return [];
    const result = await request<Record<string, unknown>[]>(core, `/devices?owner_id=${ownerId}`);
    return result.map(deviceFrom);
  },
  async device(id: number): Promise<Device> { const result = await request<Record<string, unknown>>(core, `/devices?id=${id}`); remember(id); return deviceFrom(result); },
  async saveDevice(data: Omit<Device, 'id'|'owner_id'|'created_at'>, existing?: Device): Promise<Device> {
    if (existing) { await request(core, `/devices?id=${existing.id}`, { method: 'PUT', body: JSON.stringify({ ...data, owner_id: existing.owner_id }) }); return { ...existing, ...data }; }
    const ownerId = Number(sessionStorage.getItem(ownerKey) ?? import.meta.env.VITE_LIVE_OWNER_ID);
    if (!Number.isInteger(ownerId) || ownerId <= 0) throw new Error('Для live-режима задайте VITE_LIVE_OWNER_ID в frontend/.env');
    const result = await request<Record<string, unknown>>(core, '/devices', { method: 'POST', body: JSON.stringify({ ...data, owner_id: ownerId }) });
    const device = { ...data, id: Number(result.device_id), owner_id: ownerId, created_at: '' }; remember(device.id); return device;
  },
  async deleteDevice(id: number): Promise<void> { await request(core, '/devices', { method: 'DELETE', body: JSON.stringify({ device_id: id }) }); forget(id); },
  async telemetry(deviceId: number, range?: TimeRange): Promise<Telemetry[]> { return request<Telemetry[]>(core, '/telemetry/query', { method: 'POST', body: JSON.stringify({ device_id: deviceId, ...(range ? { from: new Date(range.from).toISOString(), to: new Date(range.to).toISOString() } : {}) }) }); },
  async send(deviceId: number, value: number): Promise<void> { await request(core, '/telemetry', { method: 'POST', body: JSON.stringify({ device_id: deviceId, temperature: value }) }); },
  async clearTelemetry(deviceId: number): Promise<void> { await request(core, '/telemetry', { method: 'DELETE', body: JSON.stringify({ device_id: deviceId }) }); },
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
  async updateUser(user: User, patch: Partial<User>): Promise<User> {
    const next = { ...user, ...patch };
    await request(core, `/users?id=${user.id}`, { method: 'PUT', body: JSON.stringify({ email: next.email, name: next.name, telegram_chat_id: next.telegram_chat_id }) });
    return next;
  },
  async testEmail(email: string): Promise<void> { await request(notifications, '/notifications/email/test', { method: 'POST', body: JSON.stringify({ email }) }); },
  async testTelegram(device: Device, user: User, threshold: number, reading: number): Promise<void> { await request(notifications, '/notifications/temperature', { method: 'POST', body: JSON.stringify({ device_id: device.id, chat_id: user.telegram_chat_id, email: user.email, device_name: device.name, temperature: reading, threshold }) }); },
};
