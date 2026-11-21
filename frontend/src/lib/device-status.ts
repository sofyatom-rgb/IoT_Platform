import type { Rule, Status, Telemetry } from './types';

export function violatesRule(rule: Rule, value: number): boolean {
  if (rule.operator === '>') return value > rule.value;
  if (rule.operator === '<') return value < rule.value;
  if (rule.operator === '=') return value === rule.value;
  return value !== rule.value;
}

export function isAlertReading(rules: Rule[] | undefined, value: number): boolean {
  return !!rules?.length && rules.some((rule) => violatesRule(rule, value));
}

export function deriveDeviceStatus(last: Telemetry | undefined, rules: Rule[], now = Date.now()): Status {
  if (!last) return 'offline';
  if (isAlertReading(rules, last.temperature)) return 'alert';
  const age = now - new Date(last.time + 'Z').getTime();
  return age < 900000 ? 'online' : age < 3600000 ? 'stale' : 'offline';
}
