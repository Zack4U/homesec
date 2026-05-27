import Database from 'better-sqlite3';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const DATA_DIR = path.join(__dirname, 'data');

let db;

export function initDatabase() {
  if (!fs.existsSync(DATA_DIR)) fs.mkdirSync(DATA_DIR, { recursive: true });
  db = new Database(path.join(DATA_DIR, 'security.db'));
  db.pragma('journal_mode = WAL');
  db.pragma('foreign_keys = ON');

  db.exec(`
    CREATE TABLE IF NOT EXISTS events (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      type TEXT NOT NULL,
      data TEXT,
      timestamp DATETIME DEFAULT (datetime('now','localtime'))
    );
    CREATE TABLE IF NOT EXISTS ble_devices (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      mac TEXT UNIQUE NOT NULL,
      name TEXT NOT NULL,
      rssi INTEGER DEFAULT -100,
      active INTEGER DEFAULT 0,
      last_seen DATETIME
    );
    CREATE TABLE IF NOT EXISTS rfid_cards (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      uid TEXT UNIQUE NOT NULL,
      name TEXT NOT NULL,
      level TEXT DEFAULT 'guest',
      active INTEGER DEFAULT 1,
      created_at DATETIME DEFAULT (datetime('now','localtime'))
    );
    CREATE TABLE IF NOT EXISTS sensor_history (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      sensor TEXT NOT NULL,
      value REAL,
      extra TEXT,
      timestamp DATETIME DEFAULT (datetime('now','localtime'))
    );
    CREATE TABLE IF NOT EXISTS media_records (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      type TEXT NOT NULL,
      action TEXT,
      timestamp DATETIME DEFAULT (datetime('now','localtime')),
      notes TEXT
    );
    CREATE INDEX IF NOT EXISTS idx_events_ts ON events(timestamp DESC);
    CREATE INDEX IF NOT EXISTS idx_sensor_ts ON sensor_history(sensor, timestamp DESC);
  `);
  console.log('[DB] Database initialized');
  return db;
}

// Events
export function addEvent(type, data) {
  const d = typeof data === 'object' ? JSON.stringify(data) : (data || '{}');
  return db.prepare('INSERT INTO events (type, data) VALUES (?, ?)').run(type, d);
}
export function getEvents(limit = 50) {
  return db.prepare('SELECT * FROM events ORDER BY timestamp DESC LIMIT ?').all(limit);
}

// BLE
export function getBLEDevices() {
  return db.prepare('SELECT * FROM ble_devices ORDER BY name').all();
}
export function addBLEDevice(mac, name) {
  return db.prepare('INSERT OR REPLACE INTO ble_devices (mac, name) VALUES (?, ?)').run(mac, name);
}
export function removeBLEDevice(mac) {
  return db.prepare('DELETE FROM ble_devices WHERE mac = ?').run(mac);
}
export function updateBLEDevice(mac, data) {
  const fields = [];
  const vals = [];
  if (data.rssi != null) { fields.push('rssi = ?'); vals.push(data.rssi); }
  if (data.active != null) { fields.push('active = ?'); vals.push(data.active ? 1 : 0); }
  if (data.name) { fields.push('name = ?'); vals.push(data.name); }
  fields.push("last_seen = datetime('now','localtime')");
  vals.push(mac);
  if (fields.length > 1) {
    db.prepare(`UPDATE ble_devices SET ${fields.join(', ')} WHERE mac = ?`).run(...vals);
  }
}

// RFID
export function getRFIDCards() {
  return db.prepare('SELECT * FROM rfid_cards ORDER BY name').all();
}
export function addRFIDCard(uid, name, level = 'guest') {
  return db.prepare('INSERT OR REPLACE INTO rfid_cards (uid, name, level) VALUES (?, ?, ?)').run(uid, name, level);
}
export function removeRFIDCard(uid) {
  return db.prepare('DELETE FROM rfid_cards WHERE uid = ?').run(uid);
}

// Sensors
export function addSensorReading(sensor, value, extra) {
  const e = typeof extra === 'object' ? JSON.stringify(extra) : (extra || null);
  return db.prepare('INSERT INTO sensor_history (sensor, value, extra) VALUES (?, ?, ?)').run(sensor, value, e);
}
export function getSensorHistory(sensor, limit = 100) {
  return db.prepare('SELECT * FROM sensor_history WHERE sensor = ? ORDER BY timestamp DESC LIMIT ?').all(sensor, limit);
}

// Media
export function addMediaRecord(type, action, notes) {
  return db.prepare('INSERT INTO media_records (type, action, notes) VALUES (?, ?, ?)').run(type, action, notes || null);
}
export function getMediaRecords(limit = 50) {
  return db.prepare('SELECT * FROM media_records ORDER BY timestamp DESC LIMIT ?').all(limit);
}
export function deleteMediaRecord(id) {
  try {
    const record = db.prepare('SELECT notes FROM media_records WHERE id = ?').get(id);
    if (record && record.notes) {
      const filePath = path.join(__dirname, 'uploads', record.notes);
      if (fs.existsSync(filePath)) {
        fs.unlinkSync(filePath);
        console.log('[DB] Deleted file from disk:', filePath);
      }
    }
  } catch (err) {
    console.error('[DB] Error deleting media file:', err);
  }
  return db.prepare('DELETE FROM media_records WHERE id = ?').run(id);
}
