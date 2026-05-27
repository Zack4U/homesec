import mqtt from 'mqtt';
import * as db from './database.js';

let client = null;

export function initMQTT(io) {
  const brokerUrl = process.env.MQTT_BROKER_URL || 'mqtt://192.168.0.110:1883';
  const opts = {
    username: process.env.MQTT_USER || 'esp32',
    password: process.env.MQTT_PASS || 'esp32pass',
    clientId: 'WebDashboard_' + Math.random().toString(16).slice(2, 8),
    rejectUnauthorized: false,
    reconnectPeriod: 5000,
    connectTimeout: 10000,
  };

  console.log(`[MQTT] Connecting to ${brokerUrl}...`);
  client = mqtt.connect(brokerUrl, opts);

  client.on('connect', () => {
    console.log('[MQTT] Connected to broker');
    client.subscribe('home/security/#', (err) => {
      if (err) console.error('[MQTT] Subscribe error:', err);
      else console.log('[MQTT] Subscribed to home/security/#');
    });
    io.emit('mqtt:status', { connected: true });
  });

  client.on('error', (err) => {
    console.error('[MQTT] Error:', err.message);
  });

  client.on('offline', () => {
    console.log('[MQTT] Offline');
    io.emit('mqtt:status', { connected: false });
  });

  client.on('reconnect', () => {
    console.log('[MQTT] Reconnecting...');
  });

  client.on('message', (topic, message) => {
    try {
      const payload = JSON.parse(message.toString());
      const shortTopic = topic.replace('home/security/', '');
      handleMessage(io, shortTopic, payload);
    } catch (e) {
      console.error('[MQTT] Parse error:', e.message);
    }
  });

  return client;
}

export const discoveredDevices = {
  ble: [],
  rfid: []
};

function handleMessage(io, topic, payload) {
  switch (topic) {
    case 'radar/scan':
      io.emit('radar:scan', payload);
      break;

    case 'radar/alert':
      io.emit('radar:alert', payload);
      db.addEvent('radar_alert', payload);
      io.emit('event:new', { type: 'radar_alert', data: JSON.stringify(payload), timestamp: new Date().toISOString() });
      break;

    case 'radar/tracking':
      io.emit('radar:tracking', payload);
      break;

    case 'climate/data':
      io.emit('climate:data', payload);
      db.addSensorReading('temperature', payload.temperature, payload);
      db.addSensorReading('humidity', payload.humidity, payload);
      break;

    case 'light/status':
      io.emit('light:status', payload);
      break;

    case 'avoidance/triggered':
      io.emit('avoidance:triggered', payload);
      db.addEvent('avoidance_triggered', payload);
      io.emit('event:new', { type: 'avoidance_triggered', data: JSON.stringify(payload), timestamp: new Date().toISOString() });
      break;

    case 'ble/device':
      io.emit('ble:device', payload);
      if (payload.mac) {
        db.updateBLEDevice(payload.mac, { rssi: payload.rssi, active: true, name: payload.name });
        
        // Track recently discovered devices for easy registration
        const idx = discoveredDevices.ble.findIndex(d => d.mac === payload.mac);
        const devData = { mac: payload.mac, name: payload.name || 'Dispositivo Desconocido', rssi: payload.rssi, timestamp: Date.now() };
        if (idx >= 0) {
          discoveredDevices.ble[idx] = devData;
        } else {
          discoveredDevices.ble.push(devData);
          if (discoveredDevices.ble.length > 20) discoveredDevices.ble.shift();
        }
        io.emit('ble:discovered', discoveredDevices.ble);
      }
      break;

    case 'rfid/access':
      io.emit('rfid:access', payload);
      db.addEvent('rfid_access', payload);
      io.emit('event:new', { type: 'rfid_access', data: JSON.stringify(payload), timestamp: new Date().toISOString() });
      
      // Auto-discover unregistered RFID cards
      if (!payload.granted && payload.uid) {
        const idx = discoveredDevices.rfid.findIndex(c => c.uid === payload.uid);
        const cardData = { uid: payload.uid, timestamp: Date.now() };
        if (idx >= 0) {
          discoveredDevices.rfid[idx] = cardData;
        } else {
          discoveredDevices.rfid.push(cardData);
          if (discoveredDevices.rfid.length > 5) discoveredDevices.rfid.shift();
        }
        io.emit('rfid:discovered', discoveredDevices.rfid);
      }
      break;

    case 'door/status':
      io.emit('door:status', payload);
      db.addEvent('door_event', payload);
      io.emit('event:new', { type: 'door_event', data: JSON.stringify(payload), timestamp: new Date().toISOString() });
      break;

    case 'camera/command':
      io.emit('camera:command', payload);
      db.addMediaRecord('camera', payload.action, null);
      break;

    case 'system/status':
      io.emit('system:status', payload);
      break;

    case 'touch/event':
      io.emit('touch:event', payload);
      db.addEvent('touch_event', payload);
      io.emit('event:new', { type: 'touch_event', data: JSON.stringify(payload), timestamp: new Date().toISOString() });
      break;

    default:
      break;
  }
}

export function publishCommand(topic, payload) {
  if (!client || !client.connected) {
    console.warn('[MQTT] Not connected, cannot publish');
    return false;
  }
  const fullTopic = topic.startsWith('home/') ? topic : `home/security/${topic}`;
  client.publish(fullTopic, JSON.stringify(payload));
  console.log(`[MQTT] Published: ${fullTopic}`);
  return true;
}

export function isConnected() {
  return client ? client.connected : false;
}
