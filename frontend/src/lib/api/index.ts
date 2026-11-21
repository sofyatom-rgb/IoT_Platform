import { db, id, telemetryId } from './mock-db';
import type { Device, Rule, Telemetry, TimeRange, User } from '../types';
import { liveApi } from './live-api';

const pause = () => new Promise((resolve) => setTimeout(resolve, 180));
const clean = (user: User) => ({ ...user });

const mockApi = {
  async login(email: string, password: string) {
    await pause();
    const ok =
      (email === 'demo@iot.local' && password === 'demo1234') ||
      (email === 'admin@iot.local' && password === 'admin1234');
    if (!ok) throw Error('Неверный email или пароль');
    return clean(db.users.find((user) => user.email === email)!);
  },
  async register(name: string, email: string, _password: string, telegramChatId = '') {
    await pause();
    if (db.users.some((user) => user.email === email)) {
      throw Error('Этот email уже зарегистрирован');
    }
    const user: User = {
      id: id(),
      name,
      email,
      telegram_chat_id: telegramChatId || null,
      role: 'user',
    };
    db.users.push(user);
    return clean(user);
  },
  async devices() {
    await pause();
    return [...db.devices];
  },
  async device(deviceId: number) {
    await pause();
    const device = db.devices.find((item) => item.id === deviceId);
    if (!device) throw Error('Устройство не найдено');
    return device;
  },
  async saveDevice(
    data: Omit<Device, 'id' | 'owner_id' | 'created_at'>,
    existing?: Device,
  ) {
    await pause();
    if (existing) {
      Object.assign(existing, data);
      return existing;
    }
    const device: Device = {
      ...data,
      id: id(),
      owner_id: 1,
      created_at: new Date().toISOString().slice(0, 19),
    };
    db.devices.push(device);
    return device;
  },
  async deleteDevice(deviceId: number) {
    await pause();
    db.devices = db.devices.filter((item) => item.id !== deviceId);
    db.telemetry = db.telemetry.filter((item) => item.device_id !== deviceId);
  },
  async telemetry(deviceId: number, range?: TimeRange, afterId?: number, allTime = false) {
    await pause();
    const from = range ? new Date(range.from).toISOString().slice(0, 19) : '';
    const to = range ? new Date(range.to).toISOString().slice(0, 19) : '';
    const readings = db.telemetry
      .filter(
        (item) =>
          item.device_id === deviceId &&
          (afterId === undefined || item.id > afterId) &&
          (allTime || !range || (item.time >= from && item.time <= to)),
      )
      .sort((left, right) => right.time.localeCompare(left.time));
    if (afterId !== undefined) return readings.sort((left, right) => left.id - right.id);
    return readings;
  },
  async send(deviceId: number, temperatureValue: number) {
    await pause();
    const reading: Telemetry = {
      id: telemetryId(),
      device_id: deviceId,
      temperature: temperatureValue,
      time: new Date().toISOString().slice(0, 19),
    };
    db.telemetry.unshift(reading);
    return reading;
  },
  async clearTelemetry(deviceId: number) {
    await pause();
    db.telemetry = db.telemetry.filter((item) => item.device_id !== deviceId);
  },
  async rules(deviceId?: number) {
    await pause();
    return db.rules.filter((rule) => !deviceId || rule.device_id === deviceId);
  },
  async saveRule(data: Omit<Rule, 'rule_id'>, existing?: Rule) {
    await pause();
    if (existing) {
      Object.assign(existing, data);
      return existing;
    }
    const rule: Rule = { ...data, rule_id: id() };
    db.rules.push(rule);
    return rule;
  },
  async deleteRule(ruleId: number) {
    await pause();
    db.rules = db.rules.filter((rule) => rule.rule_id !== ruleId);
  },
  async updateUser(user: User, patch: Partial<User>) {
    await pause();
    Object.assign(user, patch);
    return clean(user);
  },
  async testEmail(_email: string) {
    await pause();
  },
  async testTelegram(
    _device: Device,
    _user: User,
    _threshold: number,
    _reading: number,
  ) {
    await pause();
  },
};

export const api = import.meta.env.VITE_API_MODE === 'live' ? liveApi : mockApi;
