import { useId } from 'react';
import {
  Area,
  AreaChart,
  CartesianGrid,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts';
import type { TooltipContentProps } from 'recharts';
import { Activity } from 'lucide-react';
import { msk, parseTimestamp, temperature } from '../../lib/format';
import type { Telemetry, TimeRange } from '../../lib/types';

type ChartPoint = {
  timestamp: number;
  temperature: number;
};

type HistoryChartProps = {
  values: Telemetry[];
  range: TimeRange;
  accent?: string;
};

function downsample(points: ChartPoint[], limit = 280): ChartPoint[] {
  if (points.length <= limit) return points;
  const bucketSize = points.length / limit;
  const result: ChartPoint[] = [];
  for (let index = 0; index < limit; index += 1) {
    const start = Math.floor(index * bucketSize);
    const end = Math.max(start + 1, Math.floor((index + 1) * bucketSize));
    const bucket = points.slice(start, end);
    if (!bucket.length) continue;
    const average = bucket.reduce((sum, point) => sum + point.temperature, 0) / bucket.length;
    result.push({
      timestamp: bucket[Math.floor(bucket.length / 2)].timestamp,
      temperature: Number(average.toFixed(2)),
    });
  }
  const last = points[points.length - 1];
  if (result[result.length - 1]?.timestamp !== last.timestamp) result.push(last);
  return result;
}

function tickFormatterFor(range: TimeRange): (value: number) => string {
  const span = range.to - range.from;
  const options: Intl.DateTimeFormatOptions =
    span <= 5 * 60 * 1000
      ? { hour: '2-digit', minute: '2-digit', second: '2-digit' }
      : span <= 24 * 60 * 60 * 1000
        ? { hour: '2-digit', minute: '2-digit' }
        : span <= 7 * 24 * 60 * 60 * 1000
          ? { weekday: 'short', day: '2-digit' }
          : span <= 90 * 24 * 60 * 60 * 1000
            ? { day: '2-digit', month: '2-digit' }
            : { month: 'short', year: '2-digit' };
  const formatter = new Intl.DateTimeFormat('ru-RU', {
    ...options,
    timeZone: 'Europe/Moscow',
  });
  return (value) => formatter.format(new Date(value));
}

function HistoryTooltip({ active, payload, label }: TooltipContentProps) {
  if (!active || !payload?.length) return null;
  const timestamp = typeof label === 'number' ? label : Number(label);
  if (!Number.isFinite(timestamp)) return null;
  const value = Number(payload[0]?.value ?? 0);
  return (
    <div className="chart-tooltip">
      <span>{msk(new Date(timestamp).toISOString())}</span>
      <b>{temperature(value)}</b>
      <small>точка телеметрии</small>
    </div>
  );
}

export function HistoryChart({ values, range, accent = '#2dd4bf' }: HistoryChartProps) {
  const gradientId = `history-gradient-${useId().replace(/[^a-zA-Z0-9_-]/g, '')}`;
  const points = values
    .map((item) => ({
      timestamp: parseTimestamp(item.time).getTime(),
      temperature: item.temperature,
    }))
    .filter(
      (point) =>
        Number.isFinite(point.timestamp) &&
        Number.isFinite(point.temperature) &&
        point.timestamp >= range.from &&
        point.timestamp <= range.to,
    )
    .sort((left, right) => left.timestamp - right.timestamp);

  if (!points.length) {
    return (
      <div className="chart-empty">
        <Activity size={24} />
        <span>В выбранном промежутке нет данных для графика</span>
      </div>
    );
  }

  const data = downsample(points);
  const temperatures = points.map((point) => point.temperature);
  const minimum = Math.min(...temperatures);
  const maximum = Math.max(...temperatures);
  const average = temperatures.reduce((sum, value) => sum + value, 0) / temperatures.length;
  const delta = points[points.length - 1].temperature - points[0].temperature;
  const spread = maximum - minimum;
  const padding = spread === 0 ? 1 : Math.max(spread * 0.18, 0.3);
  const domain: [number, number] = [
    Math.floor((minimum - padding) * 2) / 2,
    Math.ceil((maximum + padding) * 2) / 2,
  ];
  const formatTick = tickFormatterFor(range);
  const formatValue = new Intl.NumberFormat('ru-RU', { maximumFractionDigits: 1 });

  return (
    <div className="chart-panel">
      <div className="chart-stats">
        <div className="chart-stat">
          <span>Минимум</span>
          <b>{formatValue.format(minimum)}°</b>
        </div>
        <div className="chart-stat">
          <span>Средняя</span>
          <b className="accent">{formatValue.format(average)}°</b>
        </div>
        <div className="chart-stat">
          <span>Максимум</span>
          <b>{formatValue.format(maximum)}°</b>
        </div>
        <div className="chart-stat">
          <span>Изменение</span>
          <b className={delta < 0 ? 'down' : delta > 0 ? 'up' : ''}>
            {delta > 0 ? '+' : ''}
            {formatValue.format(delta)}°
          </b>
        </div>
      </div>
      <div className="chart">
        <ResponsiveContainer width="100%" height="100%">
          <AreaChart data={data} margin={{ top: 18, right: 14, left: 0, bottom: 4 }}>
            <defs>
              <linearGradient id={gradientId} x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor={accent} stopOpacity={0.34} />
                <stop offset="70%" stopColor={accent} stopOpacity={0.06} />
                <stop offset="100%" stopColor={accent} stopOpacity={0} />
              </linearGradient>
            </defs>
            <CartesianGrid stroke="rgba(148, 163, 184, .08)" vertical={false} />
            <XAxis
              dataKey="timestamp"
              type="number"
              scale="time"
              domain={[range.from, range.to]}
              tick={{ fill: '#8a9bb4', fontSize: 11 }}
              tickLine={false}
              axisLine={false}
              minTickGap={28}
              tickFormatter={(value) => formatTick(Number(value))}
            />
            <YAxis
              dataKey="temperature"
              domain={domain}
              tick={{ fill: '#8a9bb4', fontSize: 11 }}
              tickLine={false}
              axisLine={false}
              width={46}
              tickFormatter={(value) => `${formatValue.format(Number(value))}°`}
            />
            <Tooltip
              content={(props) => <HistoryTooltip {...props} />}
              cursor={{ stroke: 'rgba(94, 234, 212, .25)', strokeWidth: 1 }}
            />
            <ReferenceLine
              y={average}
              stroke="rgba(230, 237, 247, .16)"
              strokeDasharray="4 6"
              label={{
                value: 'среднее',
                position: 'insideTopRight',
                fill: '#64748b',
                fontSize: 10,
              }}
            />
            <Area
              type="monotone"
              dataKey="temperature"
              stroke={accent}
              strokeWidth={3}
              fill={`url(#${gradientId})`}
              dot={
                data.length <= 24
                  ? { r: 3, strokeWidth: 0, fill: accent }
                  : false
              }
              activeDot={{ r: 6, stroke: '#04100f', strokeWidth: 2, fill: accent }}
              animationDuration={650}
            />
          </AreaChart>
        </ResponsiveContainer>
      </div>
    </div>
  );
}
