import { describe, expect, it } from 'vitest';
import {
  RANGE_PRESETS,
  rangeForMode,
} from '../../app/components/DateRangePicker';

describe('telemetry range modes', () => {
  const now = Date.UTC(2026, 10, 21, 12);

  it('resolves each preset against the current time', () => {
    RANGE_PRESETS.forEach(({ key, ms }) => {
      expect(rangeForMode({ kind: 'preset', preset: key }, now)).toEqual({
        from: now - ms,
        to: now,
      });
    });
  });

  it('keeps a custom fixed range unchanged as time advances', () => {
    const range = { from: now - 12_000, to: now - 1_000 };
    expect(rangeForMode({ kind: 'custom-fixed', range }, now + 60_000)).toEqual(range);
  });

  it('moves a custom live end to the supplied current time', () => {
    const from = now - 24 * 60 * 60 * 1000;
    expect(rangeForMode({ kind: 'custom-live', from }, now + 15_000)).toEqual({
      from,
      to: now + 15_000,
    });
  });

  it('uses the available history start for all-time display', () => {
    const earliest = Date.UTC(2025, 0, 1);
    expect(rangeForMode({ kind: 'all-time' }, now, earliest)).toEqual({
      from: earliest,
      to: now,
    });
  });
});
