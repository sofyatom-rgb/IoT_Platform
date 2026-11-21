import { useRef, useEffect, useState } from 'react';

type Device3DProps = {
  deviceId: number;
  width: number;
  height: number;
};

export const Device3D = ({ deviceId, width, height }: Device3DProps) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [rotate, setRotate] = useState(0);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    
    canvas.width = width;
    canvas.height = height;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const centerX = width / 2;
    const centerY = height / 2;
    const radius = Math.min(width, height) * 0.4;

    let animationId: number;

    function draw() {
      ctx!.clearRect(0, 0, width, height);
      ctx!.fillStyle = 'rgba(255,255,255,0.1)';
      ctx!.fillRect(0, 0, width, height);

      const hue = ((deviceId * 37 + Date.now() / 1000) % 360) / 360;
      const gradient = ctx!.createRadialGradient(centerX, centerY, 0, centerX, centerY, radius);
      gradient.addColorStop(0, `hsla(${hue * 360}, 80%, 70%, 0.8)`);
      gradient.addColorStop(0.5, `hsla(${hue * 360}, 60%, 50%, 0.5)`);
      gradient.addColorStop(1, 'transparent');

      ctx!.save();
      ctx!.translate(centerX, centerY);
      ctx!.rotate(rotate);
      ctx!.translate(-centerX, -centerY);

      ctx!.fillStyle = gradient;
      ctx!.beginPath();
      ctx!.arc(centerX, centerY, radius, 0, Math.PI * 2);
      ctx!.fill();

      ctx!.beginPath();
      ctx!.arc(centerX - 20, centerY - 20, radius * 0.3, 0, Math.PI * 2);
      ctx!.fillStyle = 'rgba(255,255,255,0.1)';
      ctx!.fill();

      ctx!.restore();
      animationId = requestAnimationFrame(draw);
    }

    draw();
    setRotate((prev) => prev + 0.005);

    return () => cancelAnimationFrame(animationId);
  }, [deviceId, width, height]);

  return <canvas ref={canvasRef} style={{ width: '100%', height: '100%', borderRadius: 8 }} />;
};
