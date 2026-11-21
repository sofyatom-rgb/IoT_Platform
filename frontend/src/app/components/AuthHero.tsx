import { Activity, Droplets, Radio, ShieldCheck, Thermometer, Wifi } from 'lucide-react';
import { DeviceModel, deviceTheme } from './DeviceModel';

export function AuthHero() {
  return (
    <section className="hero auth-hero">
      <div className="hero-brand">
        <span className="brand-glyph">◈</span>
        <span>IoT Monitor</span>
        <span className="hero-version">v2.0</span>
      </div>

      <div className="hero-stage">
        <span className="hero-beam" />
        <DeviceModel variant="hero" theme={deviceTheme(1)} value="23.6" label="°C" />

        <div className="hero-chip hero-chip-a">
          <span className="chip-icon">
            <Thermometer size={15} />
          </span>
          <span>
            <small>Температура</small>
            <b>23.6 °C</b>
          </span>
        </div>
        <div className="hero-chip hero-chip-b">
          <span className="chip-icon">
            <Droplets size={15} />
          </span>
          <span>
            <small>Влажность</small>
            <b>48 %</b>
          </span>
        </div>
        <div className="hero-chip hero-chip-c">
          <span className="chip-icon">
            <Wifi size={15} />
          </span>
          <span>
            <small>Сеть</small>
            <b>стабильна</b>
          </span>
        </div>
        <div className="hero-chip hero-chip-d">
          <span className="chip-icon">
            <Activity size={15} />
          </span>
          <span>
            <small>Аптайм</small>
            <b>99.98 %</b>
          </span>
        </div>

        <span className="hero-orbit-label">SENSOR NODE · 01</span>
      </div>

      <div className="hero-copy">
        <span className="eyebrow">ПРОМЫШЛЕННАЯ СЕТЬ</span>
        <h1>IoT Monitor</h1>
        <p>
          Ночная смена под полным контролем. Телеметрия, правила и уведомления — в одном
          центре управления.
        </p>
        <div className="hero-badges">
          <span className="badge">
            <ShieldCheck size={13} /> Защищённая сеть
          </span>
          <span className="badge">
            <Activity size={13} /> 24/7 мониторинг
          </span>
          <span className="badge">
            <Radio size={13} /> Live telemetry
          </span>
        </div>
      </div>

      <div className="hero-footer">
        <span>
          <b>128</b> узлов онлайн
        </span>
        <span>
          <b>12 мс</b> отклик
        </span>
        <span>
          <b>99.98%</b> аптайм
        </span>
      </div>
    </section>
  );
}
