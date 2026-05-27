import dotenv from 'dotenv';
import express from 'express';
import { createServer } from 'http';
import { Server } from 'socket.io';
import cors from 'cors';
import path from 'path';
import fs from 'fs';
import { fileURLToPath } from 'url';
import { initDatabase, getEvents, getBLEDevices, addBLEDevice, removeBLEDevice,
         getRFIDCards, addRFIDCard, removeRFIDCard, getSensorHistory, getMediaRecords, addMediaRecord, deleteMediaRecord } from './database.js';
import { initMQTT, publishCommand, isConnected, discoveredDevices } from './mqtt-handler.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
dotenv.config({ path: path.join(__dirname, '.env'), override: true });
const PORT = process.env.PORT || 3001;
const app = express();
const server = createServer(app);
const io = new Server(server, { cors: { origin: '*' } });

app.use(cors());
app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ limit: '50mb', extended: true }));

// Setup and serve uploads folder
const uploadsPath = path.join(__dirname, 'uploads');
if (!fs.existsSync(uploadsPath)) {
  fs.mkdirSync(uploadsPath, { recursive: true });
}
app.use('/uploads', express.static(uploadsPath));

// Serve static frontend in production
const distPath = path.join(__dirname, 'dist');
app.use(express.static(distPath));

// ── REST API ──
app.get('/api/status', (_, res) => {
  res.json({ mqtt: isConnected(), uptime: process.uptime() | 0 });
});

app.get('/api/events', (req, res) => {
  res.json(getEvents(Number(req.query.limit) || 50));
});

app.get('/api/ble-devices', (_, res) => res.json(getBLEDevices()));
app.post('/api/ble-devices', (req, res) => {
  const { mac, name } = req.body;
  if (!mac || !name) return res.status(400).json({ error: 'mac and name required' });
  addBLEDevice(mac, name);
  publishCommand('ble/register', { action: 'add', mac, name });
  res.json({ ok: true });
});
app.delete('/api/ble-devices/:mac', (req, res) => {
  removeBLEDevice(req.params.mac);
  publishCommand('ble/register', { action: 'delete', mac: req.params.mac });
  res.json({ ok: true });
});

app.get('/api/rfid-cards', (_, res) => res.json(getRFIDCards()));
app.post('/api/rfid-cards', (req, res) => {
  const { uid, name, level } = req.body;
  if (!uid || !name) return res.status(400).json({ error: 'uid and name required' });
  addRFIDCard(uid, name, level || 'guest');
  publishCommand('rfid/register', { action: 'add', uid, name, level: level || 'guest' });
  res.json({ ok: true });
});
app.delete('/api/rfid-cards/:uid', (req, res) => {
  removeRFIDCard(req.params.uid);
  publishCommand('rfid/register', { action: 'delete', uid: req.params.uid });
  res.json({ ok: true });
});

app.get('/api/sensor-history/:sensor', (req, res) => {
  res.json(getSensorHistory(req.params.sensor, Number(req.query.limit) || 100));
});

app.get('/api/media-records', (req, res) => {
  res.json(getMediaRecords(Number(req.query.limit) || 50));
});

app.post('/api/media/upload', (req, res) => {
  const { type, action, data, filename } = req.body;
  if (!type || !data || !filename) {
    return res.status(400).json({ error: 'type, data and filename required' });
  }

  try {
    // ── Decode base64 (handle data-URL prefix and possible line breaks) ──
    const sepIdx = data.indexOf(';base64,');
    const raw = sepIdx >= 0 ? data.slice(sepIdx + 8) : data;
    const buffer = Buffer.from(raw.replace(/\s/g, ''), 'base64');

    if (buffer.length === 0) {
      return res.status(400).json({ error: 'decoded buffer is empty' });
    }

    // ── Date-organized subdirectory: uploads/YYYY/MM/DD/HH/ ──
    const now = new Date();
    const year  = now.getFullYear().toString();
    const month = String(now.getMonth() + 1).padStart(2, '0');
    const day   = String(now.getDate()).padStart(2, '0');
    const hour  = String(now.getHours()).padStart(2, '0');

    const subDir = path.join(uploadsPath, year, month, day, hour);
    if (!fs.existsSync(subDir)) {
      fs.mkdirSync(subDir, { recursive: true });
    }

    // Relative path stored in DB and used for serving: YYYY/MM/DD/HH/filename
    const relPath  = `${year}/${month}/${day}/${hour}/${filename}`;
    const filePath = path.join(subDir, filename);

    fs.writeFile(filePath, buffer, (err) => {
      if (err) {
        console.error('[Upload] Error saving file:', err);
        return res.status(500).json({ error: 'failed to save file' });
      }

      addMediaRecord(type, action || 'unknown', relPath);
      io.emit('media:new', { type, action, notes: relPath, timestamp: new Date().toISOString() });
      console.log(`[Upload] Saved ${type} (${action}): ${relPath} (${buffer.length} bytes)`);
      res.json({ ok: true, filename: relPath });
    });
  } catch (err) {
    console.error('[Upload] Exception during file save:', err);
    res.status(500).json({ error: 'internal server error during upload' });
  }
});

app.delete('/api/media-records/:id', (req, res) => {
  const { id } = req.params;
  try {
    deleteMediaRecord(Number(id));
    io.emit('media:delete', { id: Number(id) });
    res.json({ ok: true });
  } catch (err) {
    console.error('[API] Error deleting media record:', err);
    res.status(500).json({ error: 'failed to delete record' });
  }
});

app.get('/api/ble-discovered', (_, res) => {
  const registered = getBLEDevices().map(d => d.mac.toUpperCase());
  const filtered = discoveredDevices.ble.filter(d => !registered.includes(d.mac.toUpperCase()));
  res.json(filtered);
});

app.get('/api/rfid-discovered', (_, res) => {
  const registered = getRFIDCards().map(c => c.uid.toUpperCase());
  const filtered = discoveredDevices.rfid.filter(c => !registered.includes(c.uid.toUpperCase()));
  res.json(filtered);
});

app.post('/api/command', (req, res) => {
  const { topic, payload } = req.body;
  if (!topic) return res.status(400).json({ error: 'topic required' });
  const ok = publishCommand(topic, payload || {});
  res.json({ ok });
});

// SPA fallback
app.get('*', (_, res) => {
  res.sendFile(path.join(distPath, 'index.html'));
});

// ── Socket.IO ──
io.on('connection', (socket) => {
  console.log(`[WS] Client connected: ${socket.id}`);
  socket.emit('mqtt:status', { connected: isConnected() });
  
  // Send initial lists of auto-discovered (and unregistered) devices
  const regBle = getBLEDevices().map(d => d.mac.toUpperCase());
  socket.emit('ble:discovered', discoveredDevices.ble.filter(d => !regBle.includes(d.mac.toUpperCase())));
  
  const regRfid = getRFIDCards().map(c => c.uid.toUpperCase());
  socket.emit('rfid:discovered', discoveredDevices.rfid.filter(c => !regRfid.includes(c.uid.toUpperCase())));

  socket.on('command', ({ topic, payload }) => publishCommand(topic, payload));
  socket.on('disconnect', () => console.log(`[WS] Client disconnected: ${socket.id}`));
});

// ── Start ──
initDatabase();
initMQTT(io);

server.listen(PORT, () => {
  console.log(`\n═══════════════════════════════════════`);
  console.log(` IoT Security Dashboard Server`);
  console.log(` http://localhost:${PORT}`);
  console.log(`═══════════════════════════════════════\n`);
});
