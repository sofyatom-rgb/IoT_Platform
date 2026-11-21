import { CalendarDays } from 'lucide-react';
import { msk, parseTimestamp, temperature } from '../../lib/format';
import type { Rule, Telemetry, TimeRange } from '../../lib/types';
import { RangeControls, rangeLabel, type RangePreset } from './DateRangePicker';
import { HistoryChart } from './HistoryChart';

type HistoryPanelProps = {
  title: string;
  eyebrow?: string;
  subtitle?: string;
  values: Telemetry[];
  range: TimeRange;
  preset: RangePreset;
  onPreset: (preset: Exclude<RangePreset, 'custom'>) => void;
  onCustom: (range: TimeRange) => void;
  accent?: string;
  rules?: Rule[];
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
  preset,
  onPreset,
  onCustom,
  accent,
  rules,
}: HistoryPanelProps) {
  const visible = values
    .map((item) => ({ item, timestamp: parseTimestamp(item.time).getTime() }))
    .filter(
      ({ timestamp }) =>
        Number.isFinite(timestamp) && timestamp >= range.from && timestamp <= range.to,
    )
    .sort((left, right) => right.timestamp - left.timestamp);
  const list = visible.slice(0, 300);

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
          <span className="mono">{rangeLabel(range)}</span>
        </div>
      </div>

      <RangeControls preset={preset} range={range} onPreset={onPreset} onCustom={onCustom} />

      <div className="history-layout">
        <div className="history-list">
          <div className="history-list-head">
            <span>Время</span>
            <span>Температура</span>
          </div>
          {list.map(({ item }) => {
            const bad = rules?.some((rule) => violates(rule, item.temperature)) ?? false;
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
          {visible.length > list.length && (
            <p className="history-more">
              Показаны последние {list.length} из {visible.length}
            </p>
          )}
        </div>
        <HistoryChart values={visible.map(({ item }) => item)} range={range} accent={accent} />
      </div>
    </section>
  );
}
