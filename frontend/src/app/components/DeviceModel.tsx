import type { CSSProperties } from 'react';

export type DeviceTheme = {
  key: string;
  hue: number;
  accent: string;
  soft: string;
  glow: string;
};

const themes: DeviceTheme[] = [
  { key: 'teal', hue: 172, accent: '#2dd4bf', soft: '#5eead4', glow: 'rgba(45, 212, 191, .5)' },
  { key: 'cyan', hue: 192, accent: '#22d3ee', soft: '#67e8f9', glow: 'rgba(34, 211, 238, .5)' },
  { key: 'violet', hue: 262, accent: '#a78bfa', soft: '#c4b5fd', glow: 'rgba(167, 139, 250, .45)' },
  { key: 'amber', hue: 38, accent: '#fbbf24', soft: '#fde68a', glow: 'rgba(251, 191, 36, .42)' },
  { key: 'rose', hue: 345, accent: '#fb7185', soft: '#fda4af', glow: 'rgba(251, 113, 133, .42)' },
  { key: 'emerald', hue: 150, accent: '#34d399', soft: '#6ee7b7', glow: 'rgba(52, 211, 153, .45)' },
  { key: 'blue', hue: 218, accent: '#60a5fa', soft: '#93c5fd', glow: 'rgba(96, 165, 250, .45)' },
  { key: 'orange', hue: 22, accent: '#fb923c', soft: '#fdba74', glow: 'rgba(251, 146, 60, .42)' },
];

/**
 * Цвет модели стабильно зависит от id устройства: соседние карточки
 * получают разные, но всегда сочетающиеся с тёмной темой оттенки.
 */
export function deviceTheme(id: number): DeviceTheme {
  return themes[Math.abs(id - 1) % themes.length];
}

function generateBeautifulValue(variant: 'hero' | 'card' | 'mini'): string {
  const values = {
    hero: ['23.6', '18.4', '27.1', '31.8', '15.9', '22.5', '29.3'], // sequenced nice numbers
    card: ['--', '17.5', '23.9', '', '21.2', ''], // alternating with dashes
    mini: ['--', '25.1', ''], // simple values
  };
  const v = values[variant];
  return v[Math.floor(Math.random() * v.length)];
}

type DeviceModelProps = {
  theme?: DeviceTheme;
  variant?: 'hero' | 'card' | 'mini';
  value?: string;
  label?: string;
};

export function DeviceModel({
  theme = themes[0],
  variant = 'card',
  value,
  label,
}: DeviceModelProps) {
  const effectiveValue = value ?? generateBeautifulValue(variant);
  const style = {
    '--dm-hue': theme.hue,
    '--dm-accent': theme.accent,
    '--dm-soft': theme.soft,
    '--dm-glow': theme.glow,
  } as CSSProperties;

  return (
    <div className={`device-model device-model-${variant}`} style={style} aria-hidden="true">
      <div className="dm-aura" />
      <div className="dm-floor" />
      <div className="dm-orbit dm-orbit-a">
        <i />
      </div>
      <div className="dm-orbit dm-orbit-b">
        <i />
      </div>
      <div className="dm-pulse-ring" />
      <div className="dm-sensor">
        <span className="dm-antenna">
          <i />
        </span>
        <div className="dm-body">
          <div className="dm-screen">
            <span className="dm-screen-value">{effectiveValue}</span>
            <span className="dm-screen-label">{label ?? 'LIVE'}</span>
            <span className="dm-scan" />
          </div>
          <span className="dm-led" />
          <span className="dm-vents">
            <i />
            <i />
            <i />
          </span>
        </div>
        <span className="dm-highlight" />
      </div>
      <div className="dm-shadow" />
      <div className="dm-sparkles">
        <i />
        <i />
        <i />
        <i />
        <i />
        <i />
      </div>
    </div>
  );
}
