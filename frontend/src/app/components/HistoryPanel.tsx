import type { CSSProperties } from 'react';
import { CalendarDays } from 'lucide-react';
import { msk, parseTimestamp, relative, temperature } from '../../lib/format';
import type { Rule, Telemetry, TimeRange } from '../../lib/types';
import { RangeControls, rangeLabel, type RangeMode } from './DateRangePicker';
import { HistoryChart } from './HistoryChart';
import { DeviceModel, type DeviceTheme } from './DeviceModel';

type HistoryPanelProps = {
  title: string;
  eyebrow?: string;
  subtitle?: string;
  values: Telemetry[];
  range: TimeRange;
  mode: RangeMode;
  onMode: (mode: RangeMode) => void;
  accent?: string;
  rules?: Rule[];
  current?: Telemetry | null;
  theme?: DeviceTheme;
};

function violates(rule: Rule, value: number): boolean {
  if (rule.operator === '>') return value > rule.value;
  if (rule.operator === '<') return value < rule.value;
  if (rule.operator === '=') return value === rule.value;
  return value !== rule.value;
}

export function HistoryPanel({
  title,
  eyebrow = 'ТЕЛЕМЕТРИЯ',
  subtitle,
  values,
  range,
  mode,
  onMode,
  accent,
  rules,
  current,
  theme,
}: HistoryPanelProps) {
  const visible = values
    .map((item) => ({ item, timestamp: parseTimestamp(item.time).getTime() }))
    .filter(
      ({ timestamp }) =>
        Number.isFinite(timestamp) && timestamp >= range.from && timestamp <= range.to,
    )
    .sort((left, right) => right.timestamp - left.timestamp);
  return (
    <section className="card history-card">
      <div className="history-head">
        <div>
          <span className="eyebrow">{eyebrow}</span>
          <h2>{title}</h2>
          {subtitle && <p>{subtitle}</p>}
        </div>
        <div className="history-summary">
          <span className="badge">{visible.length} записей</span>
          <span className="mono">
            {mode.kind === 'all-time'
              ? 'Вся доступная история'
              : mode.kind === 'custom-live'
                ? `${rangeLabel(range)} · до сейчас`
                : rangeLabel(range)}
          </span>
        </div>
      </div>

      {current !== undefined && theme && (
        <div
          className="telemetry-current"
          style={{
            '--dm-soft': theme.soft,
            '--dm-glow': theme.glow,
          } as CSSProperties}
        >
          <DeviceModel
            variant="card"
            theme={theme}
            value={current ? current.temperature.toFixed(1) : '—'}
            label="°C"
          />
          <div className="device-reading">
            <span>Текущее значение</span>
            <div className="gauge">{current ? temperature(current.temperature) : '—'}</div>
            <p>{current ? relative(current.time) : 'Нет показаний'}</p>
          </div>
        </div>
      )}

      <RangeControls mode={mode} range={range} onMode={onMode} />

      <div className="history-layout">
        <div className="history-list">
          <div className="history-list-head">
            <span>Время</span>
            <span>Температура</span>
          </div>
          {visible.map(({ item }) => {
            const bad = rules?.every((rule) => violates(rule, item.temperature)) ?? false;
            return (
              <div className={`history-item ${bad ? 'alert' : ''}`} key={item.id}>
                <span className="history-time">
                  <i className={`dot ${bad ? 'bad' : ''}`} />
                  {msk(item.time)}
                </span>
                <b>{temperature(item.temperature)}</b>
              </div>
            );
          })}
          {!visible.length && (
            <div className="history-empty">
              <CalendarDays size={22} />
              <span>В выбранном промежутке нет показаний</span>
              <small>Попробуйте расширить диапазон</small>
            </div>
          )}
        </div>
        <HistoryChart values={visible.map(({ item }) => item)} range={range} accent={accent} />
      </div>
    </section>
  );
}
