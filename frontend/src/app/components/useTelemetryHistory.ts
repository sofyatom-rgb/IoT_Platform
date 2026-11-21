import { useCallback, useEffect, useRef, useState } from 'react';
import { api } from '../../lib/api';
import { parseTimestamp } from '../../lib/format';
import type { Telemetry } from '../../lib/types';
import { rangeForMode, type RangeMode } from './DateRangePicker';

function mergeTelemetry(current: Telemetry[], incoming: Telemetry[]): Telemetry[] {
  const byId = new Map(current.map((item) => [item.id, item]));
  incoming.forEach((item) => byId.set(item.id, item));
  return [...byId.values()].sort(
    (left, right) =>
      parseTimestamp(right.time).getTime() - parseTimestamp(left.time).getTime(),
  );
}

function newest(items: Telemetry[]): Telemetry | undefined {
  return items.reduce<Telemetry | undefined>(
    (result, item) => (!result || item.id > result.id ? item : result),
    undefined,
  );
}

type UseTelemetryHistoryOptions = {
  deviceId: number | null;
  mode: RangeMode;
  onError: (error: unknown) => void;
};

export function useTelemetryHistory({
  deviceId,
  mode,
  onError,
}: UseTelemetryHistoryOptions) {
  const [items, setItems] = useState<Telemetry[]>([]);
  const [latest, setLatest] = useState<Telemetry>();
  const [now, setNow] = useState(Date.now());
  const [loaded, setLoaded] = useState(false);
  const [reloadId, setReloadId] = useState(0);
  const cursor = useRef(0);
  const lastPollAt = useRef(0);
  const loadedMode = useRef<RangeMode | null>(null);
  const loadedDeviceId = useRef<number | null>(deviceId);

  useEffect(() => {
    const timer = window.setInterval(() => setNow(Date.now()), 15_000);
    return () => window.clearInterval(timer);
  }, []);

  useEffect(() => {
    if (loadedDeviceId.current !== deviceId) {
      setLatest(undefined);
      loadedDeviceId.current = deviceId;
    }
    if (!deviceId) {
      setItems([]);
      setLatest(undefined);
      setLoaded(false);
      loadedMode.current = null;
      return;
    }

    let active = true;
    setLoaded(false);
    setItems([]);
    cursor.current = 0;
    lastPollAt.current = 0;
    loadedMode.current = null;
    const historyRequest =
      mode.kind === 'all-time'
        ? api.telemetry(deviceId, undefined, undefined, true)
        : api.telemetry(deviceId, rangeForMode(mode, Date.now()));

    void Promise.all([
      api.telemetry(deviceId),
      historyRequest,
    ])
      .then(([latestItems, rangeItems]) => {
        if (!active) return;
        const all = mergeTelemetry(latestItems, rangeItems);
        cursor.current = all.reduce((max, item) => Math.max(max, item.id), 0);
        lastPollAt.current = Date.now();
        loadedMode.current = mode;
        setLatest(newest(latestItems));
        setItems(rangeItems);
        setLoaded(true);
        setNow(Date.now());
      })
      .catch((error: unknown) => {
        if (!active) return;
        onError(error);
      });

    return () => {
      active = false;
    };
  }, [deviceId, mode, onError, reloadId]);

  useEffect(() => {
    if (
      !deviceId ||
      !loaded ||
      loadedMode.current !== mode ||
      now <= lastPollAt.current
    ) return;
    let active = true;
    lastPollAt.current = now;
    void api
      .telemetry(deviceId, undefined, cursor.current)
      .then((incoming) => {
        if (!active) return;
        if (!incoming.length) return;
        cursor.current = incoming.reduce(
          (max, item) => Math.max(max, item.id),
          cursor.current,
        );
        setItems((current) => mergeTelemetry(current, incoming));
        setLatest((current) => {
          const next = newest(incoming);
          return next && (!current || next.id > current.id) ? next : current;
        });
      })
      .catch((error: unknown) => {
        if (active) onError(error);
      });

    return () => {
      active = false;
    };
  }, [deviceId, loaded, mode, now, onError]);

  const reload = useCallback(() => setReloadId((value) => value + 1), []);

  return { items, latest, now, reload };
}
