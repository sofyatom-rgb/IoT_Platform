import { describe, expect, it } from 'vitest';
import { parseTimestamp } from '../../lib/format';

describe('parseTimestamp', () => {
  it('treats timestamps without a timezone as UTC', () => {
    expect(parseTimestamp('2026-11-21T00:00:00').toISOString())
      .toBe('2026-11-21T00:00:00.000Z');
  });

  it('preserves an explicit timezone offset', () => {
    expect(parseTimestamp('2026-11-21T03:00:00+03:00').toISOString())
      .toBe('2026-11-21T00:00:00.000Z');
  });
});
