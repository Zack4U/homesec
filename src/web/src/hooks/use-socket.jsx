import { useState, useEffect, useRef, useCallback } from 'react';
import { io } from 'socket.io-client';

const SOCKET_URL = import.meta.env.DEV ? 'http://localhost:3001' : undefined;

export function useSocket() {
  const socketRef = useRef(null);
  const [connected, setConnected] = useState(false);
  const [mqttConnected, setMqttConnected] = useState(false);
  const [systemStatus, setSystemStatus] = useState({ armed: false, mode: 'disarmed', uptime: 0, freeHeap: 0, online: false });
  const [radarData, setRadarData] = useState({
    scanMap: new Array(181).fill(0),
    currentAngle: 90,
    lastDistance: 0,
    mode: 'patrol',
    tracking: null,
    alertActive: false,
  });
  const [climateData, setClimateData] = useState({ temperature: 0, humidity: 0 });
  const [lightStatus, setLightStatus] = useState({ dark: false, flashlight: false });
  const [doorStatus, setDoorStatus] = useState({ open: false, reason: '' });
  const [cameraActive, setCameraActive] = useState(false);
  const [discoveredBLE, setDiscoveredBLE] = useState([]);
  const [discoveredRFID, setDiscoveredRFID] = useState([]);
  const [events, setEvents] = useState([]);

  useEffect(() => {
    const socket = io(SOCKET_URL, { transports: ['websocket', 'polling'] });
    socketRef.current = socket;

    socket.on('connect', () => setConnected(true));
    socket.on('disconnect', () => setConnected(false));
    socket.on('mqtt:status', (d) => setMqttConnected(d.connected));
    socket.on('system:status', (d) => setSystemStatus(d));
    socket.on('ble:discovered', (d) => setDiscoveredBLE(d));
    socket.on('rfid:discovered', (d) => setDiscoveredRFID(d));

    socket.on('radar:scan', (d) => {
      setRadarData((prev) => {
        const map = [...prev.scanMap];
        const angle = Math.max(0, Math.min(180, Math.round(d.angle)));
        map[angle] = d.distance || 0;
        return { ...prev, scanMap: map, currentAngle: angle, lastDistance: d.distance || 0, mode: d.mode || prev.mode };
      });
    });

    socket.on('radar:alert', (d) => {
      setRadarData((prev) => ({ ...prev, alertActive: true }));
      setTimeout(() => setRadarData((prev) => ({ ...prev, alertActive: false })), 3000);
    });

    socket.on('radar:tracking', (d) => {
      setRadarData((prev) => ({
        ...prev,
        mode: 'tracking',
        tracking: { angle: d.angle, distance: d.distance, predicted: d.predicted_angle },
        currentAngle: d.angle,
        lastDistance: d.distance,
      }));
    });

    socket.on('climate:data', (d) => setClimateData(d));
    socket.on('light:status', (d) => setLightStatus(d));
    socket.on('door:status', (d) => setDoorStatus(d));

    socket.on('camera:command', (d) => {
      if (d.action === 'start_recording' || d.action === 'take_photo') setCameraActive(true);
      else if (d.action === 'stop_recording') setCameraActive(false);
    });

    socket.on('event:new', (ev) => {
      setEvents((prev) => [ev, ...prev].slice(0, 50));
    });

    // Load initial events
    fetch('/api/events?limit=20')
      .then((r) => r.json())
      .then((data) => setEvents(data))
      .catch(() => {});

    return () => { socket.disconnect(); };
  }, []);

  const sendCommand = useCallback((topic, payload) => {
    if (socketRef.current) socketRef.current.emit('command', { topic, payload });
  }, []);

  return { socket: socketRef.current, connected, mqttConnected, systemStatus, radarData, climateData, lightStatus, doorStatus, cameraActive, discoveredBLE, discoveredRFID, events, sendCommand };
}
