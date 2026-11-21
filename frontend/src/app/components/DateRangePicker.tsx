import { useEffect, useMemo, useRef, useState } from 'react';
import { CalendarDays, Check, ChevronLeft, ChevronRight, RotateCcw, X } from 'lucide-react';
import type { TimeRange } from '../../lib/types';

export type RangePreset = '1h' | '2h' | '6h' | '24h' | '7d' | '30d' | 'custom';

export const RANGE_PRESETS: {
  key: Exclude<RangePreset, 'custom'>;
  label: string;
  ms: number;
}[] = [
  { key: '1h', label: 'Последний час', ms: 60 * 60 * 1000 },
  { key: '2h', label: '2 часа', ms: 2 * 60 * 60 * 1000 },
  { key: '6h', label: '6 часов', ms: 6 * 60 * 60 * 1000 },
  { key: '24h', label: 'День', ms: 24 * 60 * 60 * 1000 },
  { key: '7d', label: 'Неделя', ms: 7 * 24 * 60 * 60 * 1000 },
  { key: '30d', label: 'Месяц', ms: 30 * 24 * 60 * 60 * 1000 },
];

const MSK_OFFSET = 3 * 60 * 60 * 1000;
const WEEKDAYS = ['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'];

const dateFormatter = new Intl.DateTimeFormat('ru-RU', {
  day: '2-digit',
  month: 'short',
  timeZone: 'Europe/Moscow',
});
const timeFormatter = new Intl.DateTimeFormat('ru-RU', {
  hour: '2-digit',
  minute: '2-digit',
  timeZone: 'Europe/Moscow',
});
const fullDateFormatter = new Intl.DateTimeFormat('ru-RU', {
  day: '2-digit',
  month: 'short',
  year: 'numeric',
  timeZone: 'Europe/Moscow',
});
const monthFormatter = new Intl.DateTimeFormat('ru-RU', {
  month: 'long',
  year: 'numeric',
  timeZone: 'UTC',
});

type MskParts = {
  year: number;
  month: number;
  day: number;
  hour: number;
  minute: number;
};

function pad(value: number): string {
  return String(value).padStart(2, '0');
}

function mskParts(timestamp: number): MskParts {
  const date = new Date(timestamp + MSK_OFFSET);
  return {
    year: date.getUTCFullYear(),
    month: date.getUTCMonth(),
    day: date.getUTCDate(),
    hour: date.getUTCHours(),
    minute: date.getUTCMinutes(),
  };
}

function dayKey(timestamp: number): string {
  const parts = mskParts(timestamp);
  return `${parts.year}-${pad(parts.month + 1)}-${pad(parts.day)}`;
}

function startOfMskDay(key: string): number {
  const [year, month, day] = key.split('-').map(Number);
  return Date.UTC(year, month - 1, day) - MSK_OFFSET;
}

function timeValue(timestamp: number): string {
  const parts = mskParts(timestamp);
  return `${pad(parts.hour)}:${pad(parts.minute)}`;
}

function timeInMs(value: string): number {
  const [hours, minutes] = value.split(':').map(Number);
  return ((hours || 0) * 60 + (minutes || 0)) * 60 * 1000;
}

function monthLabel(year: number, month: number): string {
  return monthFormatter.format(new Date(Date.UTC(year, month, 15, 12)));
}

type CalendarCell = {
  key: string;
  day: number;
  outside: boolean;
};

function buildMonth(year: number, month: number): CalendarCell[] {
  const firstWeekday = (new Date(Date.UTC(year, month, 1)).getUTCDay() + 6) % 7;
  return Array.from({ length: 42 }, (_, index) => {
    const date = new Date(Date.UTC(year, month, index - firstWeekday + 1));
    return {
      key: `${date.getUTCFullYear()}-${pad(date.getUTCMonth() + 1)}-${pad(date.getUTCDate())}`,
      day: date.getUTCDate(),
      outside: date.getUTCMonth() !== month,
    };
  });
}

export function presetRange(key: Exclude<RangePreset, 'custom'>, now = Date.now()): TimeRange {
  const preset = RANGE_PRESETS.find((item) => item.key === key) ?? RANGE_PRESETS[3];
  return { from: now - preset.ms, to: now };
}

export function rangeLabel(range: TimeRange): string {
  const sameDay = dayKey(range.from) === dayKey(range.to);
  const from = `${dateFormatter.format(range.from)}, ${timeFormatter.format(range.from)}`;
  const to = sameDay
    ? timeFormatter.format(range.to)
    : `${dateFormatter.format(range.to)}, ${timeFormatter.format(range.to)}`;
  return `${from} — ${to} MSK`;
}

type DateRangePickerProps = {
  value: TimeRange;
  onApply: (range: TimeRange) => void;
  onClose: () => void;
};

export function DateRangePicker({ value, onApply, onClose }: DateRangePickerProps) {
  const initialFrom = startOfMskDay(dayKey(value.from));
  const initialTo = startOfMskDay(dayKey(value.to));
  const [view, setView] = useState(() => {
    const parts = mskParts(value.from);
    return { year: parts.year, month: parts.month };
  });
  const [from, setFrom] = useState<number | null>(initialFrom);
  const [to, setTo] = useState<number | null>(initialTo);
  const [fromTime, setFromTime] = useState(() => timeValue(value.from));
  const [toTime, setToTime] = useState(() => timeValue(value.to));
  const [error, setError] = useState('');

  const cells = useMemo(() => buildMonth(view.year, view.month), [view]);
  const fromKey = from === null ? null : dayKey(from);
  const toKey = to === null ? null : dayKey(to);
  const todayKey = dayKey(Date.now());

  const shiftMonth = (delta: number) => {
    setView((current) => {
      const date = new Date(Date.UTC(current.year, current.month + delta, 1));
      return { year: date.getUTCFullYear(), month: date.getUTCMonth() };
    });
  };

  const selectDay = (cell: CalendarCell) => {
    const start = startOfMskDay(cell.key);
    if (cell.outside) {
      const parts = mskParts(start);
      setView({ year: parts.year, month: parts.month });
    }
    setError('');
    if (from === null || to !== null) {
      setFrom(start);
      setTo(null);
      setFromTime('00:00');
      setToTime('23:59');
      return;
    }
    if (start < from) {
      setTo(from);
      setFrom(start);
      setFromTime('00:00');
      setToTime('23:59');
      return;
    }
    setTo(start);
    setFromTime('00:00');
    setToTime('23:59');
  };

  const reset = () => {
    setFrom(null);
    setTo(null);
    setFromTime('00:00');
    setToTime('23:59');
    setError('');
  };

  const apply = () => {
    if (from === null || to === null) {
      setError('Выберите даты начала и конца');
      return;
    }
    const start = from + timeInMs(fromTime);
    const end = to + timeInMs(toTime);
    if (end <= start) {
      setError('Конец должен быть позже начала');
      return;
    }
    onApply({ from: start, to: end });
    onClose();
  };

  return (
    <div className="range-picker" role="dialog" aria-label="Выбор промежутка">
      <div className="calendar-head">
        <div>
          <span className="eyebrow">ПРОМЕЖУТОК</span>
          <div className="calendar-title">{monthLabel(view.year, view.month)}</div>
        </div>
        <div className="calendar-head-actions">
          <button type="button" className="calendar-nav" onClick={() => shiftMonth(-1)} aria-label="Предыдущий месяц">
            <ChevronLeft size={15} />
          </button>
          <button type="button" className="calendar-nav" onClick={() => shiftMonth(1)} aria-label="Следующий месяц">
            <ChevronRight size={15} />
          </button>
          <button type="button" className="calendar-nav" onClick={onClose} aria-label="Закрыть календарь">
            <X size={15} />
          </button>
        </div>
      </div>

      <div className="calendar-weekdays">
        {WEEKDAYS.map((weekday) => (
          <span key={weekday}>{weekday}</span>
        ))}
      </div>

      <div className="calendar-grid">
        {cells.map((cell) => {
          const isStart = cell.key === fromKey;
          const isEnd = cell.key === toKey;
          const inRange =
            fromKey !== null && toKey !== null && cell.key > fromKey && cell.key < toKey;
          const classes = ['calendar-day'];
          if (cell.outside) classes.push('outside');
          if (cell.key === todayKey) classes.push('today');
          if (inRange) classes.push('in-range');
          if (isStart) classes.push('range-start');
          if (isEnd) classes.push('range-end');
          if (isStart && isEnd) classes.push('range-single');
          return (
            <button
              type="button"
              key={cell.key}
              className={classes.join(' ')}
              onClick={() => selectDay(cell)}
              aria-label={fullDateFormatter.format(startOfMskDay(cell.key))}
              aria-pressed={isStart || isEnd}
            >
              {cell.day}
            </button>
          );
        })}
      </div>

      <div className="calendar-footer">
        <div className="range-draft">
          <label>
            <span>ОТ</span>
            <b>{from === null ? 'Не выбрано' : fullDateFormatter.format(from)}</b>
            <input
              type="time"
              value={fromTime}
              onChange={(event) => {
                setFromTime(event.target.value);
                setError('');
              }}
              aria-label="Время начала"
            />
          </label>
          <label>
            <span>ДО</span>
            <b>{to === null ? 'Не выбрано' : fullDateFormatter.format(to)}</b>
            <input
              type="time"
              value={toTime}
              onChange={(event) => {
                setToTime(event.target.value);
                setError('');
              }}
              aria-label="Время конца"
            />
          </label>
        </div>
        {error && <p className="calendar-error">{error}</p>}
        <div className="calendar-actions">
          <button type="button" className="btn ghost small" onClick={reset}>
            <RotateCcw size={14} /> Сбросить
          </button>
          <button type="button" className="btn small" onClick={apply}>
            <Check size={14} /> Применить
          </button>
        </div>
      </div>
    </div>
  );
}

type RangeControlsProps = {
  preset: RangePreset;
  range: TimeRange;
  onPreset: (preset: Exclude<RangePreset, 'custom'>) => void;
  onCustom: (range: TimeRange) => void;
};

export function RangeControls({ preset, range, onPreset, onCustom }: RangeControlsProps) {
  const [open, setOpen] = useState(false);
  const rootRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!open) return;
    const onPointerDown = (event: MouseEvent) => {
      if (rootRef.current && !rootRef.current.contains(event.target as Node)) {
        setOpen(false);
      }
    };
    document.addEventListener('mousedown', onPointerDown);
    return () => document.removeEventListener('mousedown', onPointerDown);
  }, [open]);

  return (
    <div className="range-toolbar">
      <div className="range-presets">
        {RANGE_PRESETS.map((item) => (
          <button
            key={item.key}
            type="button"
            className={`range-preset ${preset === item.key ? 'active' : ''}`}
            onClick={() => {
              onPreset(item.key);
              setOpen(false);
            }}
          >
            {item.label}
          </button>
        ))}
        <div className="range-custom" ref={rootRef}>
          <button
            type="button"
            className={`range-preset ${preset === 'custom' ? 'active' : ''}`}
            onClick={() => setOpen((value) => !value)}
            aria-expanded={open}
          >
            <CalendarDays size={14} /> Свой диапазон
          </button>
          {open && (
            <DateRangePicker
              value={range}
              onApply={(next) => {
                onCustom(next);
                setOpen(false);
              }}
              onClose={() => setOpen(false)}
            />
          )}
        </div>
      </div>
      <span className="range-hint">MSK · автообновление 15 сек</span>
    </div>
  );
}
