import { useRef, useEffect, useCallback } from 'react';

const COLORS = {
  bg: '#020a18',
  grid: 'rgba(6, 182, 212, 0.12)',
  gridBright: 'rgba(6, 182, 212, 0.25)',
  sweep: 'rgba(6, 182, 212, 0.8)',
  sweepTrail: 'rgba(6, 182, 212, 0.04)',
  text: 'rgba(6, 182, 212, 0.6)',
  textBright: 'rgba(6, 182, 212, 0.9)',
  safe: 'rgba(34, 211, 238, 0.7)',
  warning: 'rgba(250, 204, 21, 0.85)',
  danger: 'rgba(239, 68, 68, 0.9)',
  dangerGlow: 'rgba(239, 68, 68, 0.3)',
  center: 'rgba(6, 182, 212, 0.9)',
};

// Max range for visualization (cm)
const MAX_RANGE = 100;
const DETECTION_THRESHOLD = 50; // cm - yellow zone
const ALERT_DISTANCE = 30;      // cm - red zone

export default function RadarCanvas({ radarData, className }) {
  const canvasRef = useRef(null);
  const animFrameRef = useRef(null);
  const trailRef = useRef(new Float32Array(91).fill(0)); // fade trail (0-90)
  const localAngleRef = useRef(45); // Smooth angle interpolation

  const draw = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const dpr = window.devicePixelRatio || 1;

    // Set actual canvas size
    const rect = canvas.getBoundingClientRect();
    const w = rect.width;
    const h = rect.height;
    canvas.width = w * dpr;
    canvas.height = h * dpr;
    ctx.scale(dpr, dpr);

    // Radar center at bottom-left corner with offset for labels
    const cx = 40;
    const cy = h - 40;
    // Radius spans the available height/width
    const radius = Math.min(w - 60, h - 60);

    // Clear
    ctx.fillStyle = COLORS.bg;
    ctx.fillRect(0, 0, w, h);

    // Draw range rings (quarter arcs)
    const rings = 5;
    for (let i = 1; i <= rings; i++) {
      const r = (radius / rings) * i;
      ctx.beginPath();
      // Draw arc clockwise from 270 deg (Math.PI * 1.5) to 360/0 deg (0)
      ctx.arc(cx, cy, r, Math.PI * 1.5, 0, false);
      ctx.strokeStyle = i === rings ? COLORS.gridBright : COLORS.grid;
      ctx.lineWidth = i === rings ? 1.5 : 0.8;
      ctx.stroke();

      // Range labels (written horizontally along the bottom-axis)
      const rangeCm = Math.round((MAX_RANGE / rings) * i);
      ctx.fillStyle = COLORS.text;
      ctx.font = '10px "JetBrains Mono", monospace';
      ctx.textAlign = 'center';
      ctx.fillText(`${rangeCm}cm`, cx + r, cy + 16);
    }

    // Draw angle lines (every 15 degrees from 0 to 90)
    for (let deg = 0; deg <= 90; deg += 15) {
      const rad = (deg * Math.PI) / 180;
      const x = cx + Math.cos(rad) * radius;
      const y = cy - Math.sin(rad) * radius;
      
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(x, y);
      ctx.strokeStyle = COLORS.grid;
      ctx.lineWidth = 0.6;
      ctx.stroke();

      // Angle labels around the outer edge
      ctx.fillStyle = COLORS.text;
      ctx.font = '10px "JetBrains Mono", monospace';
      ctx.textAlign = deg === 90 ? 'right' : (deg === 0 ? 'left' : 'center');
      
      const lx = cx + Math.cos(rad) * (radius + 16);
      const ly = cy - Math.sin(rad) * (radius + 16);
      ctx.fillText(`${deg}°`, lx, ly + 3);
    }

    // Draw detection zones (filled arcs)
    // Alert zone (inner - red tint)
    const alertRadius = (ALERT_DISTANCE / MAX_RANGE) * radius;
    ctx.beginPath();
    ctx.arc(cx, cy, alertRadius, Math.PI * 1.5, 0, false);
    ctx.lineTo(cx, cy);
    ctx.closePath();
    ctx.fillStyle = 'rgba(239, 68, 68, 0.04)';
    ctx.fill();
    ctx.strokeStyle = 'rgba(239, 68, 68, 0.15)';
    ctx.lineWidth = 0.5;
    ctx.stroke();

    // Detection zone (mid - yellow tint)
    const detectionRadius = (DETECTION_THRESHOLD / MAX_RANGE) * radius;
    ctx.beginPath();
    ctx.arc(cx, cy, detectionRadius, Math.PI * 1.5, 0, false);
    ctx.lineTo(cx + alertRadius, cy);
    ctx.arc(cx, cy, alertRadius, 0, Math.PI * 1.5, true);
    ctx.closePath();
    ctx.fillStyle = 'rgba(250, 204, 21, 0.03)';
    ctx.fill();

    // Extract scan data
    const scanMap = radarData?.scanMap || [];
    const currentAngle = radarData?.currentAngle ?? 45;
    const mode = radarData?.mode || 'patrol';
    const tracking = radarData?.tracking;
    const alertActive = radarData?.alertActive || false;

    // Smooth local angle LERP interpolation towards the target angle
    localAngleRef.current += (currentAngle - localAngleRef.current) * 0.12;
    const localAngle = localAngleRef.current;

    // Update trail (fade effect)
    const trail = trailRef.current;
    for (let i = 0; i <= 90; i++) {
      trail[i] *= 0.98; // slow fade
    }
    // Set current angle trail to full brightness
    const angleIdx = Math.round(Math.max(0, Math.min(90, localAngle)));
    trail[angleIdx] = 1.0;
    // Light up neighbors slightly
    if (angleIdx > 0) trail[angleIdx - 1] = Math.max(trail[angleIdx - 1], 0.6);
    if (angleIdx < 90) trail[angleIdx + 1] = Math.max(trail[angleIdx + 1], 0.6);

    // Draw scan trail (fading cyan sweep)
    for (let deg = 0; deg <= 90; deg++) {
      const alpha = trail[deg];
      if (alpha < 0.01) continue;

      const rad = (deg * Math.PI) / 180;
      const x = cx + Math.cos(rad) * radius;
      const y = cy - Math.sin(rad) * radius;

      const gradient = ctx.createLinearGradient(cx, cy, x, y);
      gradient.addColorStop(0, `rgba(6, 182, 212, 0)`);
      gradient.addColorStop(0.3, `rgba(6, 182, 212, ${alpha * 0.02})`);
      gradient.addColorStop(1, `rgba(6, 182, 212, ${alpha * 0.08})`);

      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(x, y);
      ctx.strokeStyle = gradient;
      ctx.lineWidth = 2;
      ctx.stroke();
    }

    // Draw detection points from scan map
    for (let deg = 0; deg <= 90; deg++) {
      const dist = scanMap[deg];
      if (!dist || dist <= 0 || dist > MAX_RANGE) continue;

      const rad = (deg * Math.PI) / 180;
      const r = (dist / MAX_RANGE) * radius;
      const px = cx + Math.cos(rad) * r;
      const py = cy - Math.sin(rad) * r;

      // Color based on distance
      let color, glowColor, size;
      if (dist <= ALERT_DISTANCE) {
        color = COLORS.danger;
        glowColor = COLORS.dangerGlow;
        size = 5;
      } else if (dist <= DETECTION_THRESHOLD) {
        color = COLORS.warning;
        glowColor = 'rgba(250, 204, 21, 0.2)';
        size = 4;
      } else {
        color = COLORS.safe;
        glowColor = 'rgba(34, 211, 238, 0.15)';
        size = 3;
      }

      // Fade based on trail
      const pointAlpha = Math.max(0.15, trail[deg]);

      // Glow
      ctx.beginPath();
      ctx.arc(px, py, size * 2.5, 0, Math.PI * 2);
      ctx.fillStyle = glowColor.replace(/[\d.]+\)$/, `${pointAlpha * 0.4})`);
      ctx.fill();

      // Point
      ctx.beginPath();
      ctx.arc(px, py, size, 0, Math.PI * 2);
      ctx.fillStyle = color.replace(/[\d.]+\)$/, `${pointAlpha})`);
      ctx.fill();
    }

    // Draw sweep beam (bright line at current angle)
    {
      const rad = (localAngle * Math.PI) / 180;
      const bx = cx + Math.cos(rad) * radius;
      const by = cy - Math.sin(rad) * radius;

      // Sweep gradient
      const gradient = ctx.createLinearGradient(cx, cy, bx, by);
      const beamColor = mode === 'tracking' ? 'rgba(239, 68, 68,' : 'rgba(6, 182, 212,';
      gradient.addColorStop(0, `${beamColor} 0)`);
      gradient.addColorStop(0.4, `${beamColor} 0.15)`);
      gradient.addColorStop(0.8, `${beamColor} 0.5)`);
      gradient.addColorStop(1, `${beamColor} 0.9)`);

      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(bx, by);
      ctx.strokeStyle = gradient;
      ctx.lineWidth = 2.5;
      ctx.stroke();

      // Beam tip glow
      ctx.beginPath();
      ctx.arc(bx, by, 4, 0, Math.PI * 2);
      ctx.fillStyle = mode === 'tracking' ? COLORS.danger : COLORS.sweep;
      ctx.fill();

      // Wide sweep cone (triangular glow)
      const coneWidth = 3; // degrees
      const rad1 = ((localAngle - coneWidth) * Math.PI) / 180;
      const rad2 = ((localAngle + coneWidth) * Math.PI) / 180;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.arc(cx, cy, radius, -rad2, -rad1, false);
      ctx.closePath();
      ctx.fillStyle = mode === 'tracking'
        ? 'rgba(239, 68, 68, 0.06)'
        : 'rgba(6, 182, 212, 0.06)';
      ctx.fill();
    }

    // Draw tracking indicator
    if (tracking && (mode === 'tracking' || mode === 'detection')) {
      const tRad = (tracking.angle * Math.PI) / 180;
      const tR = (tracking.distance / MAX_RANGE) * radius;
      const tx = cx + Math.cos(tRad) * tR;
      const ty = cy - Math.sin(tRad) * tR;

      // Pulsing ring
      const pulse = Math.sin(Date.now() / 300) * 0.3 + 0.7;

      // Outer ring
      ctx.beginPath();
      ctx.arc(tx, ty, 14, 0, Math.PI * 2);
      ctx.strokeStyle = `rgba(239, 68, 68, ${pulse * 0.6})`;
      ctx.lineWidth = 2;
      ctx.stroke();

      // Inner ring
      ctx.beginPath();
      ctx.arc(tx, ty, 8, 0, Math.PI * 2);
      ctx.strokeStyle = `rgba(239, 68, 68, ${pulse * 0.9})`;
      ctx.lineWidth = 1.5;
      ctx.stroke();

      // Center dot
      ctx.beginPath();
      ctx.arc(tx, ty, 3, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(239, 68, 68, ${pulse})`;
      ctx.fill();

      // Predicted position (if available)
      if (tracking.predicted != null) {
        const pRad = (tracking.predicted * Math.PI) / 180;
        const px2 = cx + Math.cos(pRad) * tR;
        const py2 = cy - Math.sin(pRad) * tR;
        ctx.beginPath();
        ctx.setLineDash([4, 4]);
        ctx.moveTo(tx, ty);
        ctx.lineTo(px2, py2);
        ctx.strokeStyle = 'rgba(239, 68, 68, 0.4)';
        ctx.lineWidth = 1;
        ctx.stroke();
        ctx.setLineDash([]);

        // Predicted dot
        ctx.beginPath();
        ctx.arc(px2, py2, 3, 0, Math.PI * 2);
        ctx.fillStyle = 'rgba(239, 68, 68, 0.5)';
        ctx.fill();
      }
    }

    // Center point
    ctx.beginPath();
    ctx.arc(cx, cy, 4, 0, Math.PI * 2);
    ctx.fillStyle = COLORS.center;
    ctx.fill();
    ctx.beginPath();
    ctx.arc(cx, cy, 7, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(6, 182, 212, 0.4)';
    ctx.lineWidth = 1;
    ctx.stroke();

    // Status text overlay
    ctx.font = '12px "JetBrains Mono", monospace';
    ctx.textAlign = 'left';

    // Mode indicator
    const modeLabels = {
      patrol: { text: '● PATRULLA', color: COLORS.safe },
      detection: { text: '● DETECCIÓN', color: COLORS.warning },
      tracking: { text: '● RASTREO', color: COLORS.danger },
      manual: { text: '● MANUAL', color: COLORS.textBright },
      off: { text: '○ APAGADO', color: COLORS.text },
    };
    const modeInfo = modeLabels[mode] || modeLabels.patrol;
    ctx.fillStyle = modeInfo.color;
    ctx.fillText(modeInfo.text, 12, 20);

    // Current angle and distance
    ctx.fillStyle = COLORS.textBright;
    ctx.textAlign = 'right';
    const lastDist = radarData?.lastDistance ?? 0;
    ctx.fillText(`${Math.round(localAngle)}° | ${lastDist}cm`, w - 12, 20);

    // Alert flash overlay
    if (alertActive) {
      const flash = Math.sin(Date.now() / 150) * 0.5 + 0.5;
      ctx.fillStyle = `rgba(239, 68, 68, ${flash * 0.05})`;
      ctx.fillRect(0, 0, w, h);
    }

    animFrameRef.current = requestAnimationFrame(draw);
  }, [radarData]);

  useEffect(() => {
    animFrameRef.current = requestAnimationFrame(draw);
    return () => {
      if (animFrameRef.current) cancelAnimationFrame(animFrameRef.current);
    };
  }, [draw]);

  return (
    <canvas
      ref={canvasRef}
      className={`w-full h-full ${className || ''}`}
      style={{ display: 'block' }}
    />
  );
}
