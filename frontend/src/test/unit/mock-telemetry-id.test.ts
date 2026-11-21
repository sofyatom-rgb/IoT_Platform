import { describe, expect, it } from 'vitest';
import { db, telemetryId } from '../../lib/api/mock-db';

describe('mock telemetry IDs', () => {
  it('starts generated IDs after all seeded telemetry rows', () => {
    const largestSeededId = db.telemetry.reduce(
      (max, item) => Math.max(max, item.id),
      0,
    );
    expect(telemetryId()).toBeGreaterThan(largestSeededId);
  });
});
