import {
  NavLink,
  Navigate,
  Route,
  Routes,
  useNavigate,
  useParams,
  useSearchParams,
} from 'react-router-dom';
import { useCallback, useEffect, useMemo, useState } from 'react';
import type { CSSProperties, FormEvent, ReactNode } from 'react';
import {
  Activity,
  AlertTriangle,
  ArrowLeft,
  ArrowRight,
  Clock3,
  LockKeyhole,
  Mail,
  PencilLine,
  Plus,
  Search,
  Send,
  Thermometer,
  Trash2,
  UserRound,
  Wifi,
  WifiOff,
} from 'lucide-react';
import { toast } from 'sonner';
import { api } from '../lib/api';
import { useAuth } from '../stores/auth-store';
import { deriveDeviceStatus } from '../lib/device-status';
import { msk, relative, temperature } from '../lib/format';
import type { Device, Rule, Status, Telemetry, TimeRange } from '../lib/types';
import { AuthHero } from './components/AuthHero';
import { DeviceModel, deviceTheme } from './components/DeviceModel';
import { presetRange, type RangePreset } from './components/DateRangePicker';
import { HistoryPanel } from './components/HistoryPanel';

const nav = [
  ['/', 'Обзор'],
  ['/devices', 'Устройства'],
  ['/rules', 'Правила'],
  ['/telemetry', 'Телеметрия'],
  ['/notifications', 'Каналы'],
  ['/settings', 'Настройки'],
] as const;
const deviceTypes = ['temperature', 'humidity', 'pressure', 'energy'] as const;
const typeLabels: Record<(typeof deviceTypes)[number], string> = {
  temperature: 'Температура',
  humidity: 'Влажность',
  pressure: 'Давление',
  energy: 'Энергия',
};
const operators = ['>', '<', '=', '!='] as const;

const statusConfig: Record<Status, { label: string; icon: typeof Wifi }> = {
  online: { label: 'Онлайн', icon: Wifi },
  stale: { label: 'Нет свежих данных', icon: Clock3 },
  offline: { label: 'Офлайн', icon: WifiOff },
  alert: { label: 'Алерт', icon: AlertTriangle },
};

function useEvery15Seconds(action: () => void) {
  useEffect(() => {
    action();
    const timer = window.setInterval(action, 15_000);
    return () => window.clearInterval(timer);
  }, [action]);
}

function ChoiceButtons<T extends string | number>({
  value,
  onChange,
  options,
  label,
}: {
  value: T;
  onChange: (value: T) => void;
  options: readonly { value: T; label: string }[];
  label: string;
}) {
  return (
    <div className="choice-wrap">
      <span className="choice-label">{label}</span>
      <div className="choices">
        {options.map((option) => (
          <button
            type="button"
            key={String(option.value)}
            className={`choice ${value === option.value ? 'active' : ''}`}
            onClick={() => onChange(option.value)}
          >
            {option.label}
          </button>
        ))}
      </div>
    </div>
  );
}

function StatusBadge({ status }: { status: Status }) {
  const { label, icon: Icon } = statusConfig[status];
  return (
    <span className={`badge ${status}`}>
      <Icon size={12} />
      {label}
    </span>
  );
}

function Shell({ children }: { children: ReactNode }) {
  const { user, logout } = useAuth();
  const live = import.meta.env.VITE_API_MODE === 'live';
  const [menuOpen, setMenuOpen] = useState(false);
  return (
    <div className={`shell ${menuOpen ? 'menu-open' : ''}`}>
      <aside className="side">
        <div className="side-top">
          <div className="brand">
            <span className="brand-glyph small">◈</span> IoT Monitor
          </div>
          <button className="nav-close" onClick={() => setMenuOpen(false)} aria-label="Закрыть меню">
            ×
          </button>
        </div>
        <nav className="nav">
          {nav.map(([to, label]) => (
            <NavLink key={to} to={to} end={to === '/'} onClick={() => setMenuOpen(false)}>
              {label}
            </NavLink>
          ))}
        </nav>
        <div className="user">
          <span className="badge">{live ? 'Live API' : 'Демо-данные'}</span>
          <p>{user?.name}</p>
          <button className="btn ghost" onClick={logout}>
            Выйти
          </button>
        </div>
      </aside>
      <main className="main">
        <header className="top">
          <button className="menu-toggle" onClick={() => setMenuOpen(true)} aria-label="Открыть меню">
            ☰ <span>Меню</span>
          </button>
          <span>Сеть / мониторинг</span>
          <span className="mono">
            {new Intl.DateTimeFormat('ru-RU', {
              timeStyle: 'medium',
              timeZone: 'Europe/Moscow',
            }).format(new Date())}{' '}
            MSK
          </span>
        </header>
        <div className="content">{children}</div>
      </main>
    </div>
  );
}

function Login({ register = false }: { register?: boolean }) {
  const login = useAuth((state) => state.login);
  const navTo = useNavigate();
  const [error, setError] = useState('');
  const live = import.meta.env.VITE_API_MODE === 'live';

  const submit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    try {
      const user = register
        ? await api.register(
            String(form.get('name')),
            String(form.get('email')),
            String(form.get('password')),
            String(form.get('telegram') ?? ''),
          )
        : await api.login(String(form.get('email')), String(form.get('password')));
      login(user);
      toast.success(register ? 'Аккаунт создан' : `С возвращением, ${user.name || user.email}`);
      navTo('/');
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : 'Ошибка');
    }
  };

  return (
    <div className="auth">
      <AuthHero />
      <div className="auth-panel-wrap">
        <form className="panel auth-panel" onSubmit={submit}>
          <div className="panel-head">
            <span className="eyebrow">{register ? 'РЕГИСТРАЦИЯ' : 'ЦЕНТР УПРАВЛЕНИЯ'}</span>
            <h1>{register ? 'Создать аккаунт' : 'Добро пожаловать'}</h1>
            <p>
              {register
                ? 'Подключите свою команду к наблюдению за сетью.'
                : live
                  ? 'Введите email, чтобы открыть панель мониторинга.'
                  : 'Войдите в центр управления сетью.'}
            </p>
          </div>
          {error && (
            <p className="error" role="alert">
              {error}
            </p>
          )}
          {register && (
            <label className="field">
              <span>Имя</span>
              <div className="input-icon">
                <UserRound size={16} />
                <input name="name" required minLength={2} placeholder="Как к вам обращаться" />
              </div>
            </label>
          )}
          <label className="field">
            <span>Email</span>
            <div className="input-icon">
              <Mail size={16} />
              <input name="email" type="email" required placeholder="you@company.ru" />
            </div>
          </label>
          {(!live || register) && (
            <label className="field">
              <span>Пароль</span>
              <div className="input-icon">
                <LockKeyhole size={16} />
                <input
                  name="password"
                  type="password"
                  required
                  minLength={8}
                  placeholder="Минимум 8 символов"
                />
              </div>
            </label>
          )}
          {register && (
            <label className="field">
              <span>
                Telegram chat ID <em>необязательно</em>
              </span>
              <div className="input-icon">
                <Send size={16} />
                <input name="telegram" placeholder="-1001234567890" />
              </div>
            </label>
          )}
          <button className="btn btn-lg" type="submit">
            {register ? 'Зарегистрироваться' : 'Войти'} <ArrowRight size={16} />
          </button>
          <p className="auth-switch">
            {register ? 'Уже есть аккаунт? ' : 'Нет аккаунта? '}
            <NavLink to={register ? '/login' : '/register'}>
              {register ? 'Войти' : 'Создать'}
            </NavLink>
          </p>
          {!register && !live && (
            <div className="notice">
              <b>Демо-доступ</b>
              <br />
              <span className="mono">demo@iot.local / demo1234</span>
            </div>
          )}
        </form>
        <p className="auth-footnote">Данные защищены · мониторинг 24/7 · MSK</p>
      </div>
    </div>
  );
}

function Require({ children }: { children: ReactNode }) {
  return useAuth((state) => state.user) ? <>{children}</> : <Navigate to="/login" replace />;
}

function notifyError(error: unknown, fallback: string, id?: string) {
  toast.error(error instanceof Error ? error.message : fallback, id ? { id } : undefined);
}

function useData() {
  const [devices, setDevices] = useState<Device[]>([]);
  const [rules, setRules] = useState<Rule[]>([]);
  const reload = useCallback(() => {
    void Promise.all([api.devices(), api.rules()])
      .then(([nextDevices, nextRules]) => {
        setDevices(nextDevices);
        setRules(nextRules);
        toast.dismiss('device-data-load');
      })
      .catch((error) =>
        notifyError(error, 'Не удалось загрузить устройства и правила', 'device-data-load'),
      );
  }, []);
  useEvery15Seconds(reload);
  return { devices, rules, reload };
}

function useTelemetry(devices: Device[]) {
  const [telemetry, setTelemetry] = useState<Record<number, Telemetry[]>>({});
  const reload = useCallback(() => {
    void Promise.all(
      devices.map(async (device) => [device.id, await api.telemetry(device.id)] as const),
    )
      .then((rows) => {
        setTelemetry(Object.fromEntries(rows));
        toast.dismiss('telemetry-load');
      })
      .catch((error) =>
        notifyError(error, 'Не удалось загрузить показания', 'telemetry-load'),
      );
  }, [devices]);
  useEvery15Seconds(reload);
  return telemetry;
}

function Dashboard() {
  const { devices, rules } = useData();
  const telemetry = useTelemetry(devices);
  const user = useAuth((state) => state.user);
  const alerts = devices.filter(
    (device) =>
      deriveDeviceStatus(
        telemetry[device.id]?.[0],
        rules.filter((rule) => rule.device_id === device.id),
      ) === 'alert',
  ).length;
  const values = Object.values(telemetry).flat();
  const average = values.reduce((sum, item) => sum + item.temperature, 0) / (values.length || 1);
  const attention = devices.filter((device) =>
    ['alert', 'offline'].includes(
      deriveDeviceStatus(
        telemetry[device.id]?.[0],
        rules.filter((rule) => rule.device_id === device.id),
      ),
    ),
  );

  return (
    <Shell>
      <div className="header">
        <div>
          <h1>Здравствуйте, {user?.name || user?.email || 'оператор'}!</h1>
          <p>Сводка промышленной сети на текущую смену.</p>
        </div>
        <NavLink className="btn" to="/devices">
          Добавить устройство
        </NavLink>
      </div>
      <div className="grid stats">
        {[
          ['Устройства', devices.length],
          ['Средняя температура', values.length ? temperature(average) : '—'],
          ['Активные алерты', alerts],
          ['Правила', rules.length],
        ].map(([label, value]) => (
          <div className="stat" key={String(label)}>
            <span>{label}</span>
            <b>{value}</b>
          </div>
        ))}
      </div>
      <div className="grid two" style={{ marginTop: 16 }}>
        <section className="card dashboard-model-card">
          <div className="card-head">
            <div>
              <h2>Созвездие устройств</h2>
              <p>Датчик телеметрии · узлов в сети: {devices.length}</p>
            </div>
            <span className="badge online">
              <Wifi size={12} /> Сеть активна
            </span>
          </div>
          <div className="dashboard-stage">
            <DeviceModel
              variant="card"
              theme={deviceTheme(1)}
              value={values.length ? average.toFixed(1) : undefined}
              label="°C"
            />
          </div>
          <div className="notice">Данные устройств обновляются автоматически каждые 15 секунд.</div>
        </section>
        <section className="card">
          <h2>Требуют внимания</h2>
          <div className="rows">
            {attention.slice(0, 4).map((device) => (
              <div className="row" key={device.id}>
                <NavLink to={`/devices/${device.id}`}>{device.name}</NavLink>
                <StatusBadge
                  status={deriveDeviceStatus(
                    telemetry[device.id]?.[0],
                    rules.filter((rule) => rule.device_id === device.id),
                  )}
                />
              </div>
            ))}
            {!attention.length && <div className="empty">Все системы в норме</div>}
          </div>
        </section>
      </div>
    </Shell>
  );
}

function Devices() {
  const { devices, rules, reload } = useData();
  const [query, setQuery] = useState('');
  const [adding, setAdding] = useState(false);
  const [type, setType] = useState<(typeof deviceTypes)[number]>('temperature');
  const telemetry = useTelemetry(devices);

  const save = async (event: FormEvent<HTMLFormElement>) => {
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

  const visibleDevices = devices.filter((device) =>
    device.name.toLowerCase().includes(query.toLowerCase()),
  );

  return (
    <Shell>
      <div className="header">
        <div>
          <h1>
            Устройства <span className="mono">{devices.length}</span>
          </h1>
          <p>Оборудование, подключённое к сети.</p>
        </div>
        <button className="btn" onClick={() => setAdding(!adding)}>
          <Plus size={16} /> Добавить устройство
        </button>
      </div>
      {adding && (
        <form className="card add-device-card" onSubmit={(event) => void save(event)}>
          <div className="card-head">
            <div>
              <h2>Новое устройство</h2>
              <p>Задайте имя и тип датчика — карточка появится сразу.</p>
            </div>
            <span className="badge">3D-модель генерируется автоматически</span>
          </div>
          <label className="field">
            <span>Название</span>
            <input name="name" required minLength={2} placeholder="Например, Температура в 336 комнате" />
          </label>
          <ChoiceButtons
            label="Тип датчика"
            value={type}
            onChange={setType}
            options={deviceTypes.map((value) => ({ value, label: typeLabels[value] }))}
          />
          <button className="btn">
            <Plus size={16} /> Создать
          </button>
        </form>
      )}
      <div className="devices-toolbar">
        <label className="search-field">
          <Search size={16} />
          <input
            value={query}
            onChange={(event) => setQuery(event.target.value)}
            placeholder="Поиск устройства"
          />
        </label>
        <span className="badge">
          {visibleDevices.length} из {devices.length}
        </span>
      </div>
      <div className="grid cards">
        {visibleDevices.map((device) => {
          const last = telemetry[device.id]?.[0];
          const deviceRules = rules.filter((rule) => rule.device_id === device.id);
          const status = deriveDeviceStatus(last, deviceRules);
          const theme = deviceTheme(device.id);
          const style = {
            '--dm-accent': theme.accent,
            '--dm-soft': theme.soft,
            '--dm-glow': theme.glow,
          } as CSSProperties;
          return (
            <article className="card device-card" key={device.id} style={style}>
              <div className="device-card-head">
                <div>
                  <h2>{device.name}</h2>
                  <p className="mono">
                    {typeLabels[device.type as keyof typeof typeLabels] ?? device.type} · #{device.id}
                  </p>
                </div>
                <StatusBadge status={status} />
              </div>
              <div className="device-card-body">
                <DeviceModel
                  theme={theme}
                  value={last ? last.temperature.toFixed(1) : undefined}
                  label="°C"
                />
                <div className="device-reading">
                  <span>Текущее значение</span>
                  <div className="gauge">{last ? temperature(last.temperature) : '—'}</div>
                  <p>{last ? relative(last.time) : 'Нет показаний'}</p>
                </div>
              </div>
              <div className="device-card-foot">
                <span className="meta">
                  {deviceRules.length ? `Правил: ${deviceRules.length}` : 'Правил нет'}
                </span>
                <NavLink className="btn ghost" to={`/devices/${device.id}`}>
                  Открыть карточку <ArrowRight size={14} />
                </NavLink>
              </div>
            </article>
          );
        })}
      </div>
      {!visibleDevices.length && (
        <div className="empty">
          <Search size={24} />
          <p>Устройства не найдены. Попробуйте изменить запрос.</p>
        </div>
      )}
    </Shell>
  );
}

function Detail() {
  const { deviceId } = useParams();
  const id = Number(deviceId);
  const navTo = useNavigate();
  const [device, setDevice] = useState<Device>();
  const [latest, setLatest] = useState<Telemetry>();
  const [items, setItems] = useState<Telemetry[]>([]);
  const [rules, setRules] = useState<Rule[]>([]);
  const [preset, setPreset] = useState<RangePreset>('24h');
  const [customRange, setCustomRange] = useState<TimeRange>(() => presetRange('24h'));
  const [tick, setTick] = useState(0);
  const [loadError, setLoadError] = useState('');
  const [showManual, setShowManual] = useState(false);
  const errorToastId = `device-detail-${id}`;
  const theme = deviceTheme(id);

  useEffect(() => {
    const timer = window.setInterval(() => setTick((value) => value + 1), 15_000);
    return () => window.clearInterval(timer);
  }, []);

  const now = useMemo(() => Date.now(), [tick]);
  const range = useMemo(
    () => (preset === 'custom' ? customRange : presetRange(preset, now)),
    [preset, customRange, now],
  );

  const reload = useCallback(() => {
    void Promise.all([
      api.device(id),
      api.telemetry(id),
      api.telemetry(id, range),
      api.rules(id),
    ])
      .then(([nextDevice, latestItems, rangeItems, nextRules]) => {
        setDevice(nextDevice);
        setLatest(latestItems[0]);
        setItems(rangeItems);
        setRules(nextRules);
        setLoadError('');
        toast.dismiss(errorToastId);
      })
      .catch((error) => {
        setLoadError(
          error instanceof Error ? error.message : 'Не удалось загрузить устройство',
        );
        notifyError(error, 'Не удалось загрузить устройство', errorToastId);
      });
  }, [errorToastId, id, range, tick]);

  useEffect(() => {
    reload();
  }, [reload]);

  if (!device) {
    return (
      <Shell>
        <div className="empty">{loadError || 'Загрузка устройства…'}</div>
      </Shell>
    );
  }

  const last = latest ?? items[0];

  const handleManualSend = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const value = Number(new FormData(event.currentTarget).get('temperature'));
    if (!Number.isFinite(value)) return;
    try {
      await api.send(id, value);
      toast.success('Показание отправлено');
      setShowManual(false);
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

  return (
    <Shell>
      <button className="btn ghost back-btn" onClick={() => navTo('/devices')}>
        <ArrowLeft size={16} /> Устройства
      </button>
      <div className="device-head">
        <div>
          <p className="eyebrow">ДАТЧИК #{device.id}</p>
          <h1>{device.name}</h1>
          <p>
            {typeLabels[device.type as keyof typeof typeLabels] ?? device.type} · создано{' '}
            {device.created_at ? msk(device.created_at) : '—'}
          </p>
        </div>
        <div className="device-meta">
          <span>Последнее обновление</span>
          <b>{last ? msk(last.time) : 'Нет данных'}</b>
          <strong>{last ? temperature(last.temperature) : '—'}</strong>
        </div>
      </div>

      <div className="grid two">
        {showManual && (
          <form className="card send-card" onSubmit={(event) => void handleManualSend(event)}>
            <div className="card-head">
              <div>
                <h2>Новое показание</h2>
                <p>Ручная отправка значения в телеметрию.</p>
              </div>
              <span className="badge online">
                <Activity size={12} /> Live
              </span>
            </div>
            <label className="field">
              <span>Температура, °C</span>
              <div className="input-icon">
                <Thermometer size={16} />
                <input name="temperature" type="number" step="0.1" required placeholder="23.5" />
              </div>
            </label>
            <button className="btn">
              <Plus size={16} /> Отправить
            </button>
          </form>
        )}

        <button
          className="btn ghost small"
          onClick={() => setShowManual((v) => !v)}
        >
          <Plus size={12} /> Добавить данные вручную
        </button>

        <section className="card rules-card">
          <div className="card-head">
            <div>
              <h2>Правила</h2>
              <p>
                {rules.length
                  ? `Активных порогов: ${rules.length}`
                  : 'Пороги ещё не настроены.'}
              </p>
            </div>
            <NavLink className="btn ghost small" to={`/rules?device=${device.id}`}>
              <PencilLine size={14} /> Изменить правила
            </NavLink>
          </div>
          <div className="rules-list">
            {rules.map((rule) => (
              <div className="rule-pill" key={rule.rule_id}>
                <div>
                  <small>RULE #{rule.rule_id}</small>
                  <div className="rule-expression">
                    <span>{rule.operator}</span>
                    <b>{temperature(rule.value)}</b>
                  </div>
                </div>
                <span className="badge">Температура</span>
              </div>
            ))}
            {!rules.length && <p className="muted">Для устройства не настроены пороги.</p>}
          </div>
        </section>
      </div>

      <HistoryPanel
        title="История показаний"
        subtitle="Выберите промежуток — записи и график обновятся автоматически."
        values={items}
        range={range}
        preset={preset}
        onPreset={setPreset}
        onCustom={(next) => {
          setCustomRange(next);
          setPreset('custom');
        }}
        accent={theme.accent}
        rules={rules}
      />

      <section className="card rules-card">
        <div className="card-head">
          <div>
            <h2>Правила</h2>
            <p>
              {rules.length
                ? `Активных порогов: ${rules.length}`
                : 'Пороги ещё не настроены.'}
            </p>
          </div>
          <NavLink className="btn ghost small" to={`/rules?device=${device.id}`}>
            <PencilLine size={14} /> Изменить правила
          </NavLink>
        </div>
        <div className="rules-list">
          {rules.map((rule) => (
            <div className="rule-pill" key={rule.rule_id}>
              <div>
                <small>RULE #{rule.rule_id}</small>
                <div className="rule-expression">
                  <span>{rule.operator}</span>
                  <b>{temperature(rule.value)}</b>
                </div>
              </div>
              <span className="badge">Температура</span>
            </div>
          ))}
          {!rules.length && <p className="muted">Для устройства не настроены пороги.</p>}
        </div>
      </section>

      <button className="btn ghost danger" onClick={() => void remove()}>
        <Trash2 size={16} /> Удалить устройство
      </button>
    </Shell>
  );
}

function Rules() {
  const { devices, rules, reload } = useData();
  const [searchParams, setSearchParams] = useSearchParams();
  const queryDevice = Number(searchParams.get('device'));
  const [device, setDevice] = useState<number | null>(
    Number.isInteger(queryDevice) && queryDevice > 0 ? queryDevice : null,
  );
  const [operator, setOperator] = useState<(typeof operators)[number]>('>');

  useEffect(() => {
    const next = Number(searchParams.get('device'));
    setDevice(Number.isInteger(next) && next > 0 ? next : null);
  }, [searchParams]);

  const selectDevice = (value: number | null) => {
    setDevice(value);
    setSearchParams(value ? { device: String(value) } : {}, { replace: true });
  };

  const create = async (event: FormEvent<HTMLFormElement>) => {
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

  const filtered = rules.filter((rule) => !device || rule.device_id === device);
  const selectedDevice = devices.find((item) => item.id === device);

  return (
    <Shell>
      <div className="header">
        <div>
          <h1>Правила</h1>
          <p>Автоматические пороги для устройств и уведомлений.</p>
        </div>
        <span className="badge">Всего: {rules.length}</span>
      </div>

      <div className="device-tabs" role="tablist" aria-label="Устройства">
        <button
          type="button"
          role="tab"
          aria-selected={!device}
          className={`device-tab ${!device ? 'active' : ''}`}
          onClick={() => selectDevice(null)}
        >
          Все устройства <span>{rules.length}</span>
        </button>
        {devices.map((item) => {
          const theme = deviceTheme(item.id);
          return (
            <button
              type="button"
              role="tab"
              aria-selected={device === item.id}
              key={item.id}
              className={`device-tab ${device === item.id ? 'active' : ''}`}
              onClick={() => selectDevice(item.id)}
            >
              <i
                className="tab-dot"
                style={{ background: theme.accent, color: theme.accent }}
              />
              {item.name}
              <span>{rules.filter((rule) => rule.device_id === item.id).length}</span>
            </button>
          );
        })}
      </div>

      {device && (
        <form className="rule-creator card" onSubmit={(event) => void create(event)}>
          <div>
            <span className="eyebrow">НОВОЕ ПРАВИЛО</span>
            <h2>{selectedDevice?.name ?? `Устройство #${device}`}</h2>
            <p>Сработает, когда температура пересечёт порог.</p>
          </div>
          <ChoiceButtons
            label="Условие"
            value={operator}
            onChange={setOperator}
            options={operators.map((value) => ({ value, label: value }))}
          />
          <label className="field">
            <span>Порог, °C</span>
            <input name="value" type="number" step="0.1" required />
          </label>
          <button className="btn">
            <Plus size={16} /> Добавить правило
          </button>
        </form>
      )}

      <div className="rule-grid">
        {filtered.map((rule) => {
          const theme = deviceTheme(rule.device_id);
          const style = {
            '--dm-accent': theme.accent,
            '--dm-soft': theme.soft,
          } as CSSProperties;
          return (
            <article className="rule-card" key={rule.rule_id} style={style}>
              <div className="rule-id">RULE #{rule.rule_id}</div>
              <h2>
                {devices.find((item) => item.id === rule.device_id)?.name ??
                  `Устройство #${rule.device_id}`}
              </h2>
              <p>Температура</p>
              <div className="rule-expression">
                <span>{rule.operator}</span>
                <b>{temperature(rule.value)}</b>
              </div>
              <button className="btn ghost danger" onClick={() => void remove(rule)}>
                <Trash2 size={14} /> Удалить
              </button>
            </article>
          );
        })}
        {!filtered.length && <div className="empty">Правил для выбранного устройства пока нет.</div>}
      </div>
    </Shell>
  );
}

function TelemetryPage() {
  const { devices } = useData();
  const [chosen, setChosen] = useState<number | null>(null);
  const [preset, setPreset] = useState<RangePreset>('24h');
  const [customRange, setCustomRange] = useState<TimeRange>(() => presetRange('24h'));
  const [tick, setTick] = useState(0);
  const [items, setItems] = useState<Telemetry[]>([]);

  useEffect(() => {
    const timer = window.setInterval(() => setTick((value) => value + 1), 15_000);
    return () => window.clearInterval(timer);
  }, []);

  const now = useMemo(() => Date.now(), [tick]);
  const range = useMemo(
    () => (preset === 'custom' ? customRange : presetRange(preset, now)),
    [preset, customRange, now],
  );

  const reload = useCallback(() => {
    if (!chosen) {
      setItems([]);
      return;
    }
    void api
      .telemetry(chosen, range)
      .then((nextItems) => {
        setItems(nextItems);
        toast.dismiss('telemetry-page-load');
      })
      .catch((error) =>
        notifyError(error, 'Не удалось загрузить телеметрию', 'telemetry-page-load'),
      );
  }, [chosen, range, tick]);

  useEffect(() => {
    reload();
  }, [reload]);

  const theme = deviceTheme(chosen ?? 1);
  const chosenDevice = devices.find((device) => device.id === chosen);

  return (
    <Shell>
      <div className="header">
        <div>
          <h1>Телеметрия</h1>
          <p>Исследуйте показания устройств за любой промежуток.</p>
        </div>
      </div>
      <ChoiceButtons
        label="Устройство"
        value={chosen ?? 'none'}
        onChange={(value) => setChosen(value === 'none' ? null : value)}
        options={[
          { value: 'none', label: 'Выберите устройство' },
          ...devices.map((device) => ({ value: device.id, label: device.name })),
        ]}
      />
      {chosen ? (
        <HistoryPanel
          title={chosenDevice?.name ?? `Устройство #${chosen}`}
          subtitle="Данные обновляются автоматически, диапазон можно менять."
          values={items}
          range={range}
          preset={preset}
          onPreset={setPreset}
          onCustom={(next) => {
            setCustomRange(next);
            setPreset('custom');
          }}
          accent={theme.accent}
        />
      ) : (
        <div className="empty">Выберите устройство, чтобы исследовать данные.</div>
      )}
    </Shell>
  );
}

function Notifications() {
  const { user } = useAuth();
  const [events, setEvents] = useState<string[]>([]);
  const [devices, setDevices] = useState<Device[]>([]);
  const add = (label: string) => {
    setEvents((items) => [`${new Date().toLocaleTimeString('ru-RU')} — ${label}`, ...items]);
    toast.success(label);
  };

  useEffect(() => {
    void api
      .devices()
      .then((nextDevices) => {
        setDevices(nextDevices);
        toast.dismiss('notification-devices-load');
      })
      .catch((error) =>
        notifyError(error, 'Не удалось загрузить список устройств', 'notification-devices-load'),
      );
  }, []);

  const sendEmail = async () => {
    if (!user) return;
    try {
      await api.testEmail(user.email);
      add(`Тестовое письмо отправлено на ${user.email}`);
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Не удалось отправить письмо';
      setEvents((items) => [`${new Date().toLocaleTimeString('ru-RU')} — ошибка: ${message}`, ...items]);
      toast.error(message);
    }
  };
  const sendTelegram = async () => {
    if (!user?.telegram_chat_id) return;
    try {
      const device =
        devices[0] ??
        {
          id: 1,
          name: 'Тестовый датчик',
          owner_id: user.id,
          type: 'temperature',
          created_at: '',
        };
      await api.testTelegram(device, user, 30, 31.5);
      add('Запрос на Telegram-алерт отправлен сервису');
    } catch (error) {
      const message = error instanceof Error ? error.message : 'Не удалось отправить Telegram-алерт';
      setEvents((items) => [`${new Date().toLocaleTimeString('ru-RU')} — ошибка: ${message}`, ...items]);
      toast.error(message);
    }
  };

  return (
    <Shell>
      <h1>Проверка каналов</h1>
      <p>Email берётся из настроек профиля: {user?.email ?? '—'}.</p>
      <div className="grid two">
        <section className="card">
          <h2>Telegram</h2>
          <p>
            {user?.telegram_chat_id
              ? `Chat ID: ${user.telegram_chat_id}`
              : 'Добавьте Telegram chat ID в настройках профиля'}
          </p>
          <button className="btn" disabled={!user?.telegram_chat_id} onClick={() => void sendTelegram()}>
            Отправить тестовый алерт
          </button>
        </section>
        <section className="card">
          <h2>Email</h2>
          <p>{user?.email}</p>
          <button className="btn" onClick={() => void sendEmail()}>
            Отправить тестовое письмо
          </button>
        </section>
      </div>
      <section className="card" style={{ marginTop: 16 }}>
        <h2>Журнал сессии</h2>
        {events.length ? (
          events.map((event, index) => (
            <p className="mono" key={index}>
              {event}
            </p>
          ))
        ) : (
          <p>Пока нет тестовых попыток.</p>
        )}
        <div className="notice">SMTP и токен бота настраиваются переменными окружения сервера.</div>
      </section>
    </Shell>
  );
}

function Settings() {
  const { user, login, logout } = useAuth();
  const [name, setName] = useState(user?.name ?? '');
  const [email, setEmail] = useState(user?.email ?? '');
  const [telegram, setTelegram] = useState(user?.telegram_chat_id ?? '');
  if (!user) return null;

  const save = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    try {
      login(await api.updateUser(user, { name, email, telegram_chat_id: telegram || null }));
      toast.success('Профиль сохранён');
    } catch (error) {
      notifyError(error, 'Не удалось сохранить профиль');
    }
  };

  return (
    <Shell>
      <h1>Настройки</h1>
      <form className="card" onSubmit={(event) => void save(event)}>
        <h2>Профиль</h2>
        <label className="field">
          <span>Имя</span>
          <input value={name} onChange={(event) => setName(event.target.value)} required />
        </label>
        <label className="field">
          <span>Email</span>
          <input
            value={email}
            onChange={(event) => setEmail(event.target.value)}
            type="email"
            required
          />
        </label>
        <label className="field">
          <span>Telegram chat ID</span>
          <input
            value={telegram}
            onChange={(event) => setTelegram(event.target.value)}
            placeholder="например: -1001234567890"
          />
        </label>
        <button className="btn">Сохранить</button>
      </form>
      <section className="card" style={{ marginTop: 16 }}>
        <h2>Сессия</h2>
        <p>
          {user.email} · {user.role}
        </p>
        <button className="btn ghost" onClick={logout}>
          Выйти из аккаунта
        </button>
      </section>
    </Shell>
  );
}

function NotFound() {
  return (
    <Shell>
      <div className="empty">
        <h1>404</h1>
        <p>Страница не найдена.</p>
        <NavLink className="btn" to="/">
          На обзор
        </NavLink>
      </div>
    </Shell>
  );
}

export function Router() {
  return (
    <Routes>
      <Route path="/login" element={<Login />} />
      <Route path="/register" element={<Login register />} />
      <Route
        path="/"
        element={
          <Require>
            <Dashboard />
          </Require>
        }
      />
      <Route
        path="/devices"
        element={
          <Require>
            <Devices />
          </Require>
        }
      />
      <Route
        path="/devices/:deviceId"
        element={
          <Require>
            <Detail />
          </Require>
        }
      />
      <Route
        path="/rules"
        element={
          <Require>
            <Rules />
          </Require>
        }
      />
      <Route
        path="/telemetry"
        element={
          <Require>
            <TelemetryPage />
          </Require>
        }
      />
      <Route
        path="/notifications"
        element={
          <Require>
            <Notifications />
          </Require>
        }
      />
      <Route
        path="/settings"
        element={
          <Require>
            <Settings />
          </Require>
        }
      />
      <Route
        path="*"
        element={
          <Require>
            <NotFound />
          </Require>
        }
      />
    </Routes>
  );
}
