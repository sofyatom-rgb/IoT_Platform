import { NavLink, Navigate, Route, Routes, useNavigate, useParams } from 'react-router-dom';
import { useCallback, useEffect, useState } from 'react';
import { Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts';
import { toast } from 'sonner';
import { api } from '../lib/api';
import { useAuth } from '../stores/auth-store';
import { deriveDeviceStatus } from '../lib/device-status';
import { msk, parseTimestamp, relative, temperature } from '../lib/format';
import type { Device, Rule, Telemetry } from '../lib/types';

const nav = [['/', 'Обзор'], ['/devices', 'Устройства'], ['/rules', 'Правила'], ['/telemetry', 'Телеметрия'], ['/notifications', 'Каналы'], ['/settings', 'Настройки']] as const;
const deviceTypes = ['temperature', 'humidity', 'pressure', 'energy'] as const;
const typeLabels: Record<(typeof deviceTypes)[number], string> = { temperature: 'Температура', humidity: 'Влажность', pressure: 'Давление', energy: 'Энергия' };
const operators = ['>', '<', '=', '!='] as const;

function useEvery15Seconds(action: () => void) { useEffect(() => { action(); const timer = window.setInterval(action, 15_000); return () => window.clearInterval(timer); }, [action]); }
function ChoiceButtons<T extends string | number>({ value, onChange, options, label }: { value: T; onChange: (value: T) => void; options: readonly { value: T; label: string }[]; label: string }) { return <div className="choice-wrap"><span className="choice-label">{label}</span><div className="choices">{options.map(option => <button type="button" key={String(option.value)} className={`choice ${value === option.value ? 'active' : ''}`} onClick={() => onChange(option.value)}>{option.label}</button>)}</div></div>; }
function SensorScene({ compact = false }: { compact?: boolean }) { return <div className={`sensor-scene ${compact ? 'compact' : ''}`} aria-label="Анимированный датчик"><div className="sensor-shadow"/><div className="sensor"><i/><i/><i/><b>°C</b></div><span className="particle p1"/><span className="particle p2"/><span className="particle p3"/><span className="particle p4"/></div>; }

function Shell({ children }: { children: React.ReactNode }) { const { user, logout } = useAuth(); const live = import.meta.env.VITE_API_MODE === 'live'; const [menuOpen, setMenuOpen] = useState(false); return <div className={`shell ${menuOpen ? 'menu-open' : ''}`}><aside className="side"><div className="side-top"><div className="brand">◈ IoT Monitor</div><button className="nav-close" onClick={() => setMenuOpen(false)} aria-label="Закрыть меню">×</button></div><nav className="nav">{nav.map(([to, label]) => <NavLink key={to} to={to} end={to === '/'} onClick={() => setMenuOpen(false)}>{label}</NavLink>)}</nav><div className="user"><span className="badge">{live ? 'Live API' : 'Демо-данные'}</span><p>{user?.name}</p><button className="btn ghost" onClick={logout}>Выйти</button></div></aside><main className="main"><header className="top"><button className="menu-toggle" onClick={() => setMenuOpen(true)} aria-label="Открыть меню">☰ <span>Меню</span></button><span>Сеть / мониторинг</span><span className="mono">{new Intl.DateTimeFormat('ru-RU', { timeStyle: 'medium', timeZone: 'Europe/Moscow' }).format(new Date())} MSK</span></header><div className="content">{children}</div></main></div>; }

function Login({ register = false }: { register?: boolean }) { const login = useAuth(s => s.login); const navTo = useNavigate(); const [error, setError] = useState(''); const live = import.meta.env.VITE_API_MODE === 'live'; const submit = async (e: React.FormEvent<HTMLFormElement>) => { e.preventDefault(); const f = new FormData(e.currentTarget); try { const user = register ? await api.register(String(f.get('name')), String(f.get('email')), String(f.get('password')), String(f.get('telegram') ?? '')) : await api.login(String(f.get('email')), String(f.get('password'))); login(user); toast.success(register ? 'Аккаунт создан' : `С возвращением, ${user.name || user.email}`); navTo('/'); } catch (x) { setError(x instanceof Error ? x.message : 'Ошибка'); } }; return <div className="auth"><section className="hero"><SensorScene/><h1>IoT Monitor</h1><p>Ночная смена под полным контролем.</p><span className="badge">● Защищённая сеть</span> <span className="badge">● 24/7 мониторинг</span></section><form className="panel" onSubmit={submit}><h1>{register ? 'Создать аккаунт' : 'Добро пожаловать'}</h1><p>{register ? 'Подключите свою команду к наблюдению.' : live ? 'Временный вход по адресу email.' : 'Войдите в центр управления сетью.'}</p>{error && <p className="error">{error}</p>}{register && <label className="field">Имя<input name="name" required minLength={2}/></label>}<label className="field">Email<input name="email" type="email" required/></label>{(!live || register) && <label className="field">Пароль<input name="password" type="password" required minLength={8}/></label>}{register && <label className="field">Telegram chat ID <input name="telegram" placeholder="например: -1001234567890"/></label>}<button className="btn" type="submit">{register ? 'Зарегистрироваться' : 'Войти'}</button><p>{register ? 'Уже есть аккаунт? ' : 'Нет аккаунта? '}<NavLink to={register ? '/login' : '/register'}>{register ? 'Войти' : 'Создать'}</NavLink></p>{!register && !live && <div className="notice"><b>Демо-доступ</b><br/><span className="mono">demo@iot.local / demo1234</span></div>}</form></div>; }
function Require({ children }: { children: React.ReactNode }) { return useAuth(s => s.user) ? <>{children}</> : <Navigate to="/login" replace/>; }
function notifyError(error: unknown, fallback: string, id?: string) { toast.error(error instanceof Error ? error.message : fallback, id ? { id } : undefined); }
function useData() { const [devices, setDevices] = useState<Device[]>([]); const [rules, setRules] = useState<Rule[]>([]); const reload = useCallback(() => { void Promise.all([api.devices(), api.rules()]).then(([d, r]) => { setDevices(d); setRules(r); toast.dismiss('device-data-load'); }).catch(error => notifyError(error, 'Не удалось загрузить устройства и правила', 'device-data-load')); }, []); useEvery15Seconds(reload); return { devices, rules, reload }; }
function useTelemetry(devices: Device[]) { const [telemetry, setTelemetry] = useState<Record<number, Telemetry[]>>({}); const reload = useCallback(() => { void Promise.all(devices.map(async device => [device.id, await api.telemetry(device.id)] as const)).then(rows => { setTelemetry(Object.fromEntries(rows)); toast.dismiss('telemetry-load'); }).catch(error => notifyError(error, 'Не удалось загрузить показания', 'telemetry-load')); }, [devices]); useEvery15Seconds(reload); return telemetry; }

function Dashboard() { const { devices, rules } = useData(); const tele = useTelemetry(devices); const user = useAuth(s => s.user); const alerts = devices.filter(d => deriveDeviceStatus(tele[d.id]?.[0], rules.filter(r => r.device_id === d.id)) === 'alert').length; const values = Object.values(tele).flat(); const avg = values.reduce((sum, item) => sum + item.temperature, 0) / (values.length || 1); return <Shell><div className="header"><div><h1>Здравствуйте, {user?.name || user?.email || 'оператор'}!</h1><p>Сводка промышленной сети на текущую смену.</p></div><NavLink className="btn" to="/devices">Добавить устройство</NavLink></div><div className="grid stats">{[['Устройства', devices.length], ['Средняя температура', temperature(avg)], ['Активные алерты', alerts], ['Правила', rules.length]].map(([a, b]) => <div className="stat" key={String(a)}><span>{a}</span><b>{b}</b></div>)}</div><div className="grid two" style={{ marginTop: 16 }}><section className="card"><h2>Созвездие устройств</h2><p>Датчик телеметрии · узлов в сети: {devices.length}</p><SensorScene compact/><div className="notice">Данные устройств обновляются автоматически каждые 15 секунд.</div></section><section className="card"><h2>Требуют внимания</h2><div className="rows">{devices.filter(d => ['alert', 'offline'].includes(deriveDeviceStatus(tele[d.id]?.[0], rules.filter(r => r.device_id === d.id)))).slice(0, 4).map(d => <div className="row" key={d.id}><NavLink to={`/devices/${d.id}`}>{d.name}</NavLink><span className="badge alert">Проверить</span></div>)}{!devices.length && <div className="empty">Все системы ожидают данных</div>}</div></section></div></Shell>; }

function Devices() {
  const { devices, rules, reload } = useData();
  const [query, setQuery] = useState('');
  const [adding, setAdding] = useState(false);
  const [type, setType] = useState<(typeof deviceTypes)[number]>('temperature');
  const telemetry = useTelemetry(devices);

  const save = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const name = String(new FormData(event.currentTarget).get('name'));
    try {
      await api.saveDevice({ name, type });
      toast.success('Устройство создано');
      setAdding(false);
      reload();
    } catch (error) {
      notifyError(error, 'Не удалось создать устройство');
    }
  };

  const visibleDevices = devices.filter(device =>
    device.name.toLowerCase().includes(query.toLowerCase()));

  return <Shell>
    <div className="header"><div><h1>Устройства <span className="mono">{devices.length}</span></h1><p>Оборудование, подключённое к сети.</p></div><button className="btn" onClick={() => setAdding(!adding)}>Добавить устройство</button></div>
    {adding && <form className="card" onSubmit={event => void save(event)}>
      <h2>Новое устройство</h2>
      <label className="field">Название<input name="name" required minLength={2}/></label>
      <ChoiceButtons label="Тип датчика" value={type} onChange={setType} options={deviceTypes.map(value => ({ value, label: typeLabels[value] }))}/>
      <button className="btn">Создать</button>
    </form>}
    <label className="field">Поиск<input value={query} onChange={event => setQuery(event.target.value)} placeholder="Название устройства"/></label>
    <div className="grid cards">{visibleDevices.map(device => {
      const last = telemetry[device.id]?.[0];
      const status = deriveDeviceStatus(last, rules.filter(rule => rule.device_id === device.id));
      return <article className="card device" key={device.id}>
        <div className="row"><h2>{device.name}</h2><span className={`badge ${status === 'alert' ? 'alert' : status === 'offline' ? 'offline' : ''}`}>{status === 'alert' ? 'Алерт' : status === 'online' ? 'Онлайн' : status === 'stale' ? 'Нет свежих данных' : 'Офлайн'}</span></div>
        <p className="mono">{typeLabels[device.type as keyof typeof typeLabels] ?? device.type} · #{device.id}</p>
        <div className="gauge">{last ? temperature(last.temperature) : '—'}</div>
        <p>{last ? relative(last.time) : 'Нет показаний'}</p>
        <NavLink className="btn ghost" to={`/devices/${device.id}`}>Открыть карточку</NavLink>
      </article>;
    })}</div>
  </Shell>;
}

function HistoryChart({ values }: { values: Telemetry[] }) {
  const data = values
    .map(item => ({
      timestamp: parseTimestamp(item.time).getTime(),
      temperature: item.temperature,
    }))
    .filter(item => Number.isFinite(item.timestamp) && Number.isFinite(item.temperature))
    .sort((a, b) => a.timestamp - b.timestamp);
  if (!data.length) return <div className="chart-empty">Недостаточно данных для графика</div>;

  const firstTime = data[0].timestamp;
  const lastTime = data[data.length - 1].timestamp;
  const timeRange = lastTime - firstTime;
  const timeDomain: [number, number] = timeRange === 0
    ? [firstTime - 30_000, lastTime + 30_000]
    : [firstTime, lastTime];
  const temperatures = data.map(item => item.temperature);
  const minTemperature = Math.min(...temperatures);
  const maxTemperature = Math.max(...temperatures);
  const temperaturePadding = Math.max((maxTemperature - minTemperature) * 0.1, 0.5);
  const tickOptions: Intl.DateTimeFormatOptions = timeRange >= 365 * 24 * 60 * 60 * 1000
    ? { month: 'short', year: '2-digit' }
    : timeRange >= 24 * 60 * 60 * 1000
      ? { day: '2-digit', month: '2-digit' }
      : timeRange < 5 * 60 * 1000
        ? { hour: '2-digit', minute: '2-digit', second: '2-digit' }
        : { hour: '2-digit', minute: '2-digit' };
  const tickFormatter = new Intl.DateTimeFormat('ru-RU', {
    ...tickOptions,
    timeZone: 'Europe/Moscow',
  });

  return <div className="chart"><ResponsiveContainer width="100%" height="100%"><LineChart data={data} margin={{ top: 16, right: 16, left: 0, bottom: 4 }}><XAxis dataKey="timestamp" type="number" scale="time" domain={timeDomain} tick={{ fill: '#8a9bb4', fontSize: 11 }} tickLine={false} axisLine={false} minTickGap={24} tickFormatter={value => tickFormatter.format(Number(value))}/><YAxis dataKey="temperature" tick={{ fill: '#8a9bb4', fontSize: 11 }} tickLine={false} axisLine={false} width={50} domain={[minTemperature - temperaturePadding, maxTemperature + temperaturePadding]} tickFormatter={value => temperature(Number(value)).replace(' °C', '°')}/><Tooltip contentStyle={{ background: '#08111d', border: '1px solid rgba(94,234,212,.45)', borderRadius: 10 }} labelStyle={{ color: '#8a9bb4' }} labelFormatter={value => msk(new Date(Number(value)).toISOString())} formatter={value => [temperature(Number(value ?? 0)), 'Температура']}/><Line type="linear" dataKey="temperature" stroke="#5eead4" strokeWidth={3} dot={false} activeDot={{ r: 6 }}/></LineChart></ResponsiveContainer></div>;
}

function Detail() {
  const { deviceId } = useParams();
  const id = Number(deviceId);
  const navTo = useNavigate();
  const [device, setDevice] = useState<Device>();
  const [items, setItems] = useState<Telemetry[]>([]);
  const [rules, setRules] = useState<Rule[]>([]);
  const errorToastId = `device-detail-${id}`;
  const reload = useCallback(() => {
    void Promise.all([api.device(id), api.telemetry(id), api.rules(id)])
      .then(([nextDevice, nextItems, nextRules]) => {
        setDevice(nextDevice);
        setItems(nextItems);
        setRules(nextRules);
        toast.dismiss(errorToastId);
      })
      .catch(error => notifyError(error, 'Не удалось загрузить устройство', errorToastId));
  }, [errorToastId, id]);
  useEvery15Seconds(reload);

  if (!device) return <Shell><div className="empty">Загрузка устройства…</div></Shell>;

  const last = items[0];
  const send = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const value = Number(new FormData(event.currentTarget).get('temperature'));
    if (!Number.isFinite(value)) return;
    try {
      await api.send(id, value);
      toast.success('Показание отправлено');
      reload();
    } catch (error) {
      notifyError(error, 'Не удалось отправить показание');
    }
  };
  const remove = async () => {
    if (!confirm('Удалить устройство и его телеметрию?')) return;
    try {
      await api.deleteDevice(id);
      navTo('/devices');
    } catch (error) {
      notifyError(error, 'Не удалось удалить устройство');
    }
  };

  return <Shell><button className="btn ghost" onClick={() => navTo('/devices')}>← Устройства</button><div className="device-head"><div><p className="eyebrow">ДАТЧИК #{device.id}</p><h1>{device.name}</h1><p>{typeLabels[device.type as keyof typeof typeLabels] ?? device.type} · создано {device.created_at ? msk(device.created_at) : '—'}</p></div><div className="device-meta"><span>Последнее обновление</span><b>{last ? msk(last.time) : 'Нет данных'}</b><strong>{last ? temperature(last.temperature) : '—'}</strong></div></div><div className="grid two"><form className="card" onSubmit={event => void send(event)}><h2>Новое показание</h2><label className="field">Температура, °C<input name="temperature" type="number" step="0.1" required/></label><button className="btn">Отправить</button></form><section className="card"><h2>Правила</h2>{rules.map(rule => <p key={rule.rule_id} className="notice"><span className="mono">#{rule.rule_id}</span> Температура {rule.operator} {temperature(rule.value)}</p>)}{!rules.length && <p>Для устройства не настроены пороги.</p>}</section></div><section className="card history-card"><div><h2>История показаний</h2><p>Обновляется автоматически каждые 15 секунд.</p></div><div className="history-layout"><div className="history-list">{items.map(item => <div className="history-item" key={item.id}><span>{msk(item.time)}</span><b>{temperature(item.temperature)}</b></div>)}{!items.length && <p>Показаний пока нет.</p>}</div><HistoryChart values={items}/></div></section><button className="btn ghost danger" onClick={() => void remove()}>Удалить устройство</button></Shell>;
}

function Rules() {
  const { devices, rules, reload } = useData();
  const [device, setDevice] = useState<number | null>(null);
  const [operator, setOperator] = useState<(typeof operators)[number]>('>');

  const create = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    if (!device) return;
    const value = Number(new FormData(event.currentTarget).get('value'));
    try {
      await api.saveRule({ device_id: device, operator, value });
      toast.success('Правило сохранено');
      reload();
    } catch (error) {
      notifyError(error, 'Не удалось сохранить правило');
    }
  };
  const remove = async (rule: Rule) => {
    if (!confirm(`Удалить правило #${rule.rule_id}?`)) return;
    try {
      await api.deleteRule(rule.rule_id);
      reload();
    } catch (error) {
      notifyError(error, 'Не удалось удалить правило');
    }
  };
  const filtered = rules.filter(rule => !device || rule.device_id === device);

  return <Shell>
    <div className="header"><div><h1>Правила</h1><p>Автоматические пороги для устройств и уведомлений.</p></div><span className="badge">Всего: {rules.length}</span></div>
    <ChoiceButtons label="Показать правила устройства" value={device ?? 'all'} onChange={value => setDevice(value === 'all' ? null : value)} options={[{ value: 'all', label: 'Все устройства' }, ...devices.map(item => ({ value: item.id, label: item.name }))]}/>
    {device && <form className="rule-creator card" onSubmit={event => void create(event)}>
      <div><h2>Новое правило</h2><p>Для: {devices.find(item => item.id === device)?.name}</p></div>
      <ChoiceButtons label="Условие" value={operator} onChange={setOperator} options={operators.map(value => ({ value, label: value }))}/>
      <label className="field">Порог<input name="value" type="number" step="0.1" required/></label>
      <button className="btn">Добавить правило</button>
    </form>}
    <div className="rule-grid">{filtered.map(rule => <article className="rule-card" key={rule.rule_id}>
      <div className="rule-id">RULE #{rule.rule_id}</div>
      <h2>{devices.find(item => item.id === rule.device_id)?.name ?? `Устройство #${rule.device_id}`}</h2>
      <p>Температура</p>
      <div className="rule-expression"><span>{rule.operator}</span><b>{temperature(rule.value)}</b></div>
      <button className="btn ghost danger" onClick={() => void remove(rule)}>Удалить</button>
    </article>)}{!filtered.length && <div className="empty">Правил для выбранного устройства пока нет.</div>}</div>
  </Shell>;
}

function TelemetryPage() {
  const { devices } = useData();
  const [chosen, setChosen] = useState<number | null>(null);
  const [items, setItems] = useState<Telemetry[]>([]);
  const reload = useCallback(() => {
    if (!chosen) {
      setItems([]);
      return;
    }
    void api.telemetry(chosen)
      .then(nextItems => {
        setItems(nextItems);
        toast.dismiss('telemetry-page-load');
      })
      .catch(error => notifyError(error, 'Не удалось загрузить телеметрию', 'telemetry-page-load'));
  }, [chosen]);
  useEvery15Seconds(reload);
  return <Shell><h1>Телеметрия</h1><p>Последние показания устройств.</p><ChoiceButtons label="Устройство" value={chosen ?? 'none'} onChange={value => setChosen(value === 'none' ? null : value)} options={[{ value: 'none', label: 'Выберите устройство' }, ...devices.map(device => ({ value: device.id, label: device.name }))]}/>{chosen ? <section className="card"><HistoryChart values={items}/></section> : <div className="empty">Выберите устройство, чтобы исследовать данные.</div>}</Shell>;
}
function Notifications() {
  const { user } = useAuth();
  const [events, setEvents] = useState<string[]>([]);
  const [devices, setDevices] = useState<Device[]>([]);
  const add = (label: string) => {
    setEvents(items => [`${new Date().toLocaleTimeString('ru-RU')} — ${label}`, ...items]);
    toast.success(label);
  };

  useEffect(() => {
    void api.devices()
      .then(nextDevices => {
        setDevices(nextDevices);
        toast.dismiss('notification-devices-load');
      })
      .catch(error => notifyError(error, 'Не удалось загрузить список устройств', 'notification-devices-load'));
  }, []);

  const sendEmail = async () => {
    if (!user) return;
    try {
      await api.testEmail(user.email);
      add(`Тестовое письмо отправлено на ${user.email}`);
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Не удалось отправить письмо';
      setEvents(items => [`${new Date().toLocaleTimeString('ru-RU')} — ошибка: ${message}`, ...items]);
      toast.error(message);
    }
  };
  const sendTelegram = async () => {
    if (!user?.telegram_chat_id) return;
    try {
      const device = devices[0] ?? { id: 1, name: 'Тестовый датчик', owner_id: user.id, type: 'temperature', created_at: '' };
      await api.testTelegram(device, user, 30, 31.5);
      add('Запрос на Telegram-алерт отправлен сервису');
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Не удалось отправить Telegram-алерт';
      setEvents(items => [`${new Date().toLocaleTimeString('ru-RU')} — ошибка: ${message}`, ...items]);
      toast.error(message);
    }
  };

  return <Shell>
    <h1>Проверка каналов</h1>
    <p>Email берётся из настроек профиля: {user?.email ?? '—'}.</p>
    <div className="grid two">
      <section className="card"><h2>Telegram</h2><p>{user?.telegram_chat_id ? `Chat ID: ${user.telegram_chat_id}` : 'Добавьте Telegram chat ID в настройках профиля'}</p><button className="btn" disabled={!user?.telegram_chat_id} onClick={() => void sendTelegram()}>Отправить тестовый алерт</button></section>
      <section className="card"><h2>Email</h2><p>{user?.email}</p><button className="btn" onClick={() => void sendEmail()}>Отправить тестовое письмо</button></section>
    </div>
    <section className="card" style={{ marginTop: 16 }}><h2>Журнал сессии</h2>{events.length ? events.map((event, index) => <p className="mono" key={index}>{event}</p>) : <p>Пока нет тестовых попыток.</p>}<div className="notice">SMTP и токен бота настраиваются переменными окружения сервера.</div></section>
  </Shell>;
}
function Settings() {
  const { user, login, logout } = useAuth();
  const [name, setName] = useState(user?.name ?? '');
  const [email, setEmail] = useState(user?.email ?? '');
  const [telegram, setTelegram] = useState(user?.telegram_chat_id ?? '');
  if (!user) return null;

  const save = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    try {
      login(await api.updateUser(user, { name, email, telegram_chat_id: telegram || null }));
      toast.success('Профиль сохранён');
    } catch (error) {
      notifyError(error, 'Не удалось сохранить профиль');
    }
  };

  return <Shell>
    <h1>Настройки</h1>
    <form className="card" onSubmit={event => void save(event)}>
      <h2>Профиль</h2>
      <label className="field">Имя<input value={name} onChange={event => setName(event.target.value)} required/></label>
      <label className="field">Email<input value={email} onChange={event => setEmail(event.target.value)} type="email" required/></label>
      <label className="field">Telegram chat ID<input value={telegram} onChange={event => setTelegram(event.target.value)} placeholder="например: -1001234567890"/></label>
      <button className="btn">Сохранить</button>
    </form>
    <section className="card" style={{ marginTop: 16 }}><h2>Сессия</h2><p>{user.email} · {user.role}</p><button className="btn ghost" onClick={logout}>Выйти из аккаунта</button></section>
  </Shell>;
}
function NotFound() { return <Shell><div className="empty"><h1>404</h1><p>Страница не найдена.</p><NavLink className="btn" to="/">На обзор</NavLink></div></Shell>; }
export function Router() { return <Routes><Route path="/login" element={<Login/>}/><Route path="/register" element={<Login register/>}/><Route path="/" element={<Require><Dashboard/></Require>}/><Route path="/devices" element={<Require><Devices/></Require>}/><Route path="/devices/:deviceId" element={<Require><Detail/></Require>}/><Route path="/rules" element={<Require><Rules/></Require>}/><Route path="/telemetry" element={<Require><TelemetryPage/></Require>}/><Route path="/notifications" element={<Require><Notifications/></Require>}/><Route path="/settings" element={<Require><Settings/></Require>}/><Route path="*" element={<Require><NotFound/></Require>}/></Routes>; }
