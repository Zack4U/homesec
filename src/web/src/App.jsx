import { useState, useEffect, useRef, useCallback } from 'react';
import { useSocket } from '@/hooks/use-socket';
import RadarCanvas from '@/components/RadarCanvas';
import BLEManager from '@/components/BLEManager';
import RFIDManager from '@/components/RFIDManager';
import ManualControl from '@/components/ManualControl';
import MediaGallery from '@/components/MediaGallery';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Slider } from '@/components/ui/slider';
import {
  Shield, ShieldOff, Wifi, WifiOff, Radio, Thermometer, Droplets, Sun, Moon,
  Bluetooth, CreditCard, Settings, Image, DoorOpen, DoorClosed,
  AlertTriangle, Activity, Eye, Zap, Power, Clock, Cpu,
  Square, Play, RotateCcw, Camera,
} from 'lucide-react';

// ─────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────
function formatElapsed(s) {
  const m = Math.floor(s / 60).toString().padStart(2, '0');
  const sec = (s % 60).toString().padStart(2, '0');
  return `${m}:${sec}`;
}

// ─────────────────────────────────────────────────────────
// Main Component
// ─────────────────────────────────────────────────────────
export default function App() {
  const {
    socket, connected, mqttConnected, systemStatus, radarData, climateData,
    lightStatus, doorStatus, discoveredBLE, discoveredRFID, events, sendCommand,
  } = useSocket();

  // Modal state
  const [bleOpen, setBleOpen] = useState(false);
  const [rfidOpen, setRfidOpen] = useState(false);
  const [manualOpen, setManualOpen] = useState(false);
  const [mediaOpen, setMediaOpen] = useState(false);

  // Camera UI state
  const [webcamActive, setWebcamActive] = useState(false);  // preview visible
  const [isRecording, setIsRecording] = useState(false);
  const [recordingElapsed, setRecordingElapsed] = useState(0);
  const [photoFlash, setPhotoFlash] = useState(false);
  const [webcamError, setWebcamError] = useState(null);
  const [cameraAngle, setCameraAngle] = useState([90]);
  const [timeStr, setTimeStr] = useState('');

  // DOM / stream refs
  const videoRef = useRef(null);
  const streamRef = useRef(null);

  // Recording refs
  const mediaRecorderRef = useRef(null);
  const recordingChunksRef = useRef([]);
  const recordingTimerRef = useRef(null);
  const currentTriggerRef = useRef(null);  // current recording trigger type
  const recordingStartRef = useRef(0);
  const lastBlePhotoRef = useRef(0);

  // Mirror of webcamActive for use inside stable callbacks (no stale closure)
  const webcamActiveRef = useRef(false);
  useEffect(() => { webcamActiveRef.current = webcamActive; }, [webcamActive]);

  // ── Clock ──────────────────────────────────────────────
  useEffect(() => {
    const tick = () => setTimeStr(
      new Date().toLocaleTimeString('es-ES', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
    );
    tick();
    const t = setInterval(tick, 1000);
    return () => clearInterval(t);
  }, []);

  // ── Recording elapsed timer ────────────────────────────
  useEffect(() => {
    if (!isRecording) { setRecordingElapsed(0); return; }
    const id = setInterval(() => {
      setRecordingElapsed(Math.floor((Date.now() - recordingStartRef.current) / 1000));
    }, 1000);
    return () => clearInterval(id);
  }, [isRecording]);

  // ── Attach stream to <video> once it is in the DOM ────
  useEffect(() => {
    if (webcamActive && videoRef.current && streamRef.current) {
      videoRef.current.srcObject = streamRef.current;
    }
  }, [webcamActive]);

  // ─────────────────────────────────────────────────────
  // Camera primitives
  // ─────────────────────────────────────────────────────

  /**
   * Open getUserMedia. Returns the MediaStream or null on failure.
   * Idempotent: if stream is already open, returns it immediately.
   */
  const openStream = useCallback(async () => {
    if (streamRef.current) return streamRef.current;
    setWebcamError(null);
    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        video: { width: { ideal: 1280 }, height: { ideal: 720 } },
        audio: false,
      });
      streamRef.current = stream;
      return stream;
    } catch (err) {
      console.error('[CAM] getUserMedia error:', err);
      setWebcamError(
        err.name === 'NotAllowedError'
          ? 'Permiso de cámara denegado por el navegador'
          : 'No se detectó cámara o está ocupada'
      );
      return null;
    }
  }, []);

  /** Show live preview in the card */
  const showPreview = useCallback(async () => {
    const stream = await openStream();
    if (!stream) return;
    setWebcamActive(true);
    // Let React render the <video> first
    requestAnimationFrame(() => {
      if (videoRef.current) videoRef.current.srcObject = stream;
    });
  }, [openStream]);

  /** Stop the preview AND any active recording, then release the camera */
  const stopPreview = useCallback(() => {
    // Stop recorder first (onstop will fire and upload before we kill the stream)
    if (mediaRecorderRef.current && mediaRecorderRef.current.state !== 'inactive') {
      mediaRecorderRef.current.stop();
    }
    setIsRecording(false);
    currentTriggerRef.current = null;
    if (recordingTimerRef.current) {
      clearTimeout(recordingTimerRef.current);
      recordingTimerRef.current = null;
    }

    // Give onstop ~400 ms to collect the last chunk, then release stream
    setTimeout(() => {
      if (streamRef.current) {
        streamRef.current.getTracks().forEach(t => t.stop());
        streamRef.current = null;
      }
      if (videoRef.current) videoRef.current.srcObject = null;
      setWebcamActive(false);
    }, 400);
  }, []);

  // ─────────────────────────────────────────────────────
  // Upload helper
  // ─────────────────────────────────────────────────────
  // Build filename: YYYYMMDD_HHMMSS_trigger.ext
  const buildFilename = useCallback((action, ext) => {
    const now = new Date();
    const pad = (n, l = 2) => String(n).padStart(l, '0');
    const date = `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}`;
    const time = `${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
    return `${date}_${time}_${action}.${ext}`;
  }, []);

  const uploadBlob = useCallback(async (blob, mediaType, action, ext) => {
    if (!blob || blob.size === 0) {
      console.warn(`[UPLOAD] Blob empty for ${action}, skipping.`);
      return;
    }
    console.log(`[UPLOAD] Uploading ${mediaType}/${action}: ${blob.size} bytes`);

    return new Promise((resolve) => {
      const reader = new FileReader();
      reader.readAsDataURL(blob);
      reader.onloadend = async () => {
        const filename = buildFilename(action, ext);
        try {
          const res = await fetch('/api/media/upload', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ type: mediaType, action, data: reader.result, filename }),
          });
          if (!res.ok) {
            const errText = await res.text();
            console.error(`[UPLOAD] Server error ${res.status}:`, errText);
          } else {
            const json = await res.json();
            console.log(`[UPLOAD] Saved as:`, json.filename);
          }
        } catch (e) {
          console.error('[UPLOAD] Fetch error:', e);
        }
        resolve();
      };
    });
  }, [buildFilename]);

  // ─────────────────────────────────────────────────────
  // Recording
  // ─────────────────────────────────────────────────────

  /**
   * stopRecording — requests the recorder to stop. Upload happens in onstop.
   */
  const stopRecording = useCallback(() => {
    if (recordingTimerRef.current) {
      clearTimeout(recordingTimerRef.current);
      recordingTimerRef.current = null;
    }
    if (mediaRecorderRef.current && mediaRecorderRef.current.state !== 'inactive') {
      // NOTE: Do NOT call requestData() before stop().
      // requestData() drains the internal buffer immediately, making the final
      // ondataavailable from stop() come out empty. stop() alone guarantees
      // one last ondataavailable with all remaining buffered data, then onstop.
      mediaRecorderRef.current.stop();
    } else {
      setIsRecording(false);
      currentTriggerRef.current = null;
    }
  }, []);

  /**
   * startRecording — opens camera (headlessly if preview is off), starts MediaRecorder.
   * Recording and preview are fully independent.
   */
  const startRecording = useCallback(async (triggerType) => {
    // ── Stop any currently active recording first ──
    if (mediaRecorderRef.current && mediaRecorderRef.current.state !== 'inactive') {
      console.log(`[REC] Interrupting (${currentTriggerRef.current}) → starting (${triggerType})`);
      // Wait for onstop to fire before starting new recording
      await new Promise(resolve => {
        const orig = mediaRecorderRef.current.onstop;
        mediaRecorderRef.current.onstop = async (e) => {
          if (orig) await orig(e);
          resolve();
        };
        try { mediaRecorderRef.current.requestData(); } catch (_) {}
        mediaRecorderRef.current.stop();
      });
      await new Promise(r => setTimeout(r, 150));
    }

    // ── Get or open stream ──
    let stream = streamRef.current;
    if (!stream) {
      stream = await openStream();
      if (!stream) return;
      // If preview is showing, make sure the video element gets the stream
      if (webcamActiveRef.current && videoRef.current) {
        videoRef.current.srcObject = stream;
      }
    }

    // ── Set up MediaRecorder ──
    recordingChunksRef.current = [];
    currentTriggerRef.current = triggerType;
    recordingStartRef.current = Date.now();

    const preferredMime = [
      'video/webm;codecs=vp9,opus',
      'video/webm;codecs=vp8,opus',
      'video/webm',
    ].find(t => MediaRecorder.isTypeSupported(t));

    let recorder;
    try {
      recorder = new MediaRecorder(stream, preferredMime ? { mimeType: preferredMime } : {});
    } catch (err) {
      console.error('[REC] MediaRecorder init error:', err);
      return;
    }
    mediaRecorderRef.current = recorder;

    recorder.ondataavailable = (e) => {
      if (e.data && e.data.size > 0) {
        recordingChunksRef.current.push(e.data);
        console.log(`[REC] Chunk: ${e.data.size}B (total ${recordingChunksRef.current.length})`);
      }
    };

    // Capture triggerType via closure — NOT via ref (ref may be cleared before onstop fires)
    const capturedTrigger = triggerType;
    recorder.onstop = async () => {
      console.log(`[REC] onstop: trigger=${capturedTrigger}, chunks=${recordingChunksRef.current.length}`);
      setIsRecording(false);
      currentTriggerRef.current = null;

      const chunks = [...recordingChunksRef.current];
      recordingChunksRef.current = [];

      if (chunks.length > 0) {
        const blob = new Blob(chunks, { type: recorder.mimeType || 'video/webm' });
        await uploadBlob(blob, 'video', capturedTrigger, 'webm');
      } else {
        console.warn('[REC] No chunks — nothing to upload.');
      }
    };

    recorder.onerror = (e) => {
      console.error('[REC] MediaRecorder error:', e.error);
      setIsRecording(false);
    };

    recorder.start(250); // 250 ms timeslice → frequent chunks, avoids empty-blob on short recordings
    setIsRecording(true);
    setRecordingElapsed(0);
    console.log(`[REC] Started: ${triggerType} | mimeType: ${recorder.mimeType}`);

    // Auto-stop for timed triggers
    if (triggerType === 'avoidance') {
      recordingTimerRef.current = setTimeout(() => {
        console.log('[REC] Avoidance 60s → stopping');
        stopRecording();
      }, 60000);
    }
  }, [openStream, uploadBlob, stopRecording]);

  // ─────────────────────────────────────────────────────
  // Photo capture — works with OR without preview active
  // ─────────────────────────────────────────────────────
  const capturePhoto = useCallback(async (triggerType) => {
    console.log(`[PHOTO] Capturing: ${triggerType}`);

    let offscreenEl = null;
    let headlessStream = null;

    try {
      let videoEl;

      if (webcamActiveRef.current && videoRef.current && videoRef.current.videoWidth > 0) {
        // ── Use the live preview ──
        videoEl = videoRef.current;
        // Visual flash feedback
        setPhotoFlash(true);
        setTimeout(() => setPhotoFlash(false), 350);
      } else {
        // ── Headless: open stream and create hidden video ──
        let stream = streamRef.current;
        if (!stream) {
          stream = await openStream();
          if (!stream) return;
          headlessStream = stream;
        }

        offscreenEl = document.createElement('video');
        offscreenEl.srcObject = stream;
        offscreenEl.muted = true;
        offscreenEl.playsInline = true;

        // Must be in DOM (or at least have a container) for readyState to advance
        offscreenEl.style.cssText = 'position:fixed;top:-9999px;left:-9999px;width:1px;height:1px;';
        document.body.appendChild(offscreenEl);

        await new Promise((resolve, reject) => {
          const timeout = setTimeout(() => reject(new Error('Video load timeout')), 6000);
          const finish = () => { clearTimeout(timeout); resolve(); };
          if (offscreenEl.readyState >= 2) return finish();
          offscreenEl.onloadeddata = finish;
          offscreenEl.onerror = (e) => { clearTimeout(timeout); reject(e); };
          offscreenEl.play().catch(reject);
        });

        videoEl = offscreenEl;
      }

      const w = videoEl.videoWidth || 1280;
      const h = videoEl.videoHeight || 720;
      const canvas = document.createElement('canvas');
      canvas.width = w;
      canvas.height = h;
      canvas.getContext('2d').drawImage(videoEl, 0, 0, w, h);

      const blob = await new Promise(r => canvas.toBlob(r, 'image/jpeg', 0.92));
      console.log(`[PHOTO] Captured ${w}×${h}, ${blob?.size} bytes`);
      await uploadBlob(blob, 'photo', triggerType, 'jpg');

    } catch (err) {
      console.error('[PHOTO] Error:', err);
    } finally {
      if (offscreenEl) {
        offscreenEl.pause();
        offscreenEl.srcObject = null;
        if (offscreenEl.parentNode) offscreenEl.parentNode.removeChild(offscreenEl);
      }
      if (headlessStream) {
        headlessStream.getTracks().forEach(t => t.stop());
        if (streamRef.current === headlessStream) streamRef.current = null;
      }
    }
  }, [openStream, uploadBlob]);

  // ─────────────────────────────────────────────────────
  // System state
  // ─────────────────────────────────────────────────────
  const isArmed = systemStatus?.armed ?? false;

  // ── Radar trigger rules ────────────────────────────
  useEffect(() => {
    if (!isArmed) return;
    if (radarData.mode === 'tracking') {
      if (radarData.alertActive) {
        if (currentTriggerRef.current !== 'radar_alert') startRecording('radar_alert');
      } else {
        if (!currentTriggerRef.current || currentTriggerRef.current === 'radar_detect') {
          if (currentTriggerRef.current !== 'radar_detect') startRecording('radar_detect');
        }
      }
    }
  }, [radarData.mode, radarData.alertActive, isArmed, startRecording]);

  useEffect(() => {
    const cur = currentTriggerRef.current;
    if (cur === 'radar_detect' && radarData.mode !== 'tracking') {
      stopRecording();
    } else if (cur === 'radar_alert' && radarData.mode !== 'tracking') {
      if (!recordingTimerRef.current) {
        recordingTimerRef.current = setTimeout(() => stopRecording(), 10000);
      }
    } else if (cur === 'radar_alert' && radarData.mode === 'tracking') {
      if (recordingTimerRef.current) {
        clearTimeout(recordingTimerRef.current);
        recordingTimerRef.current = null;
      }
    }
  }, [radarData.mode, stopRecording]);

  // ── Socket event listeners ─────────────────────────
  useEffect(() => {
    if (!socket) return;

    const onAvoidance = () => {
      if (!isArmed) return;
      startRecording('avoidance');
    };

    const onRadarAlert = () => {
      if (!isArmed) return;
      if (currentTriggerRef.current !== 'radar_alert') startRecording('radar_alert');
    };

    // RFID: photo only on authorized access
    const onRfidAccess = (payload) => {
      if (payload.granted) capturePhoto('rfid_door');
    };

    // BLE: photo only for REGISTERED (known) devices that are very close
    const onBleDevice = (payload) => {
      if (!payload.known) return;             // ignore unregistered devices
      if (!payload.rssi || payload.rssi < -65) return;  // not close enough
      const now = Date.now();
      if (now - lastBlePhotoRef.current < 30000) return; // 30 s cooldown
      lastBlePhotoRef.current = now;
      capturePhoto('ble_proximity');
    };

    // ESP32 camera commands (take_photo / start_recording / stop_recording)
    const onCameraCommand = (payload) => {
      console.log('[SOCKET] camera:command:', payload);
      if (payload.action === 'start_recording') startRecording('esp32');
      else if (payload.action === 'stop_recording') stopRecording();
      else if (payload.action === 'take_photo') capturePhoto('esp32');
    };

    socket.on('avoidance:triggered', onAvoidance);
    socket.on('radar:alert', onRadarAlert);
    socket.on('rfid:access', onRfidAccess);
    socket.on('ble:device', onBleDevice);
    socket.on('camera:command', onCameraCommand);

    return () => {
      socket.off('avoidance:triggered', onAvoidance);
      socket.off('radar:alert', onRadarAlert);
      socket.off('rfid:access', onRfidAccess);
      socket.off('ble:device', onBleDevice);
      socket.off('camera:command', onCameraCommand);
    };
  }, [socket, isArmed, startRecording, stopRecording, capturePhoto]);

  // ─────────────────────────────────────────────────────
  // UI helpers
  // ─────────────────────────────────────────────────────
  const isEspOnline = mqttConnected && (systemStatus?.online ?? false);
  const toggleArm = () => sendCommand('control/arm', { system: 'all', armed: !isArmed });

  const formatTime = (ts) => {
    if (!ts) return '';
    try { return new Date(ts).toLocaleTimeString('es-ES', { hour: '2-digit', minute: '2-digit', second: '2-digit' }); }
    catch { return ''; }
  };

  const eventIcon = (type) => ({
    radar_alert: <AlertTriangle className="h-3.5 w-3.5 text-red-400" />,
    avoidance_triggered: <Zap className="h-3.5 w-3.5 text-yellow-400" />,
    rfid_access: <CreditCard className="h-3.5 w-3.5 text-cyan-400" />,
    door_event: <DoorOpen className="h-3.5 w-3.5 text-emerald-400" />,
    touch_event: <Activity className="h-3.5 w-3.5 text-purple-400" />,
  })[type] || <Eye className="h-3.5 w-3.5 text-muted-foreground" />;

  const eventLabel = (type) => ({
    radar_alert: 'Alerta Radar',
    avoidance_triggered: 'Intrusión Detectada',
    rfid_access: 'Acceso RFID',
    door_event: 'Evento Puerta',
    touch_event: 'Botón Táctil',
  })[type] || type;

  const eventColor = (type) => {
    if (type === 'radar_alert' || type === 'avoidance_triggered') return 'border-l-red-500';
    if (type === 'rfid_access') return 'border-l-cyan-500';
    if (type === 'door_event') return 'border-l-emerald-500';
    return 'border-l-muted';
  };

  const quickActions = [
    { icon: Bluetooth, label: 'Bluetooth', onClick: () => setBleOpen(true), color: 'text-blue-400' },
    { icon: CreditCard, label: 'RFID', onClick: () => setRfidOpen(true), color: 'text-cyan-400' },
    { icon: Settings, label: 'Control', onClick: () => setManualOpen(true), color: 'text-purple-400' },
    { icon: Image, label: 'Media', onClick: () => setMediaOpen(true), color: 'text-amber-400' },
  ];

  const moveServo = (val) => {
    setCameraAngle(val);
    sendCommand('control/camera', { action: 'move', angle: val[0] });
  };

  // ─────────────────────────────────────────────────────
  // Render
  // ─────────────────────────────────────────────────────
  return (
    <div className="min-h-screen lg:h-screen lg:overflow-hidden flex flex-col bg-background">

      {/* ── Header ── */}
      <header className="sticky top-0 z-40 border-b border-border glass-panel flex-shrink-0 h-14 flex items-center">
        <div className="w-full px-6 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <Shield className="h-5 w-5 text-primary" />
            <h1 className="text-sm font-semibold tracking-tight hidden sm:block">Panel de Control de Seguridad IoT</h1>
            <h1 className="text-sm font-semibold tracking-tight sm:hidden">IoT Security</h1>
          </div>
          <div className="flex items-center gap-4">
            <div className="flex items-center gap-1.5">
              <span className={`status-dot ${connected ? 'online' : 'offline'}`} />
              <span className="text-xs text-muted-foreground hidden sm:inline">WS Link</span>
            </div>
            <div className="flex items-center gap-1.5">
              {mqttConnected ? <Wifi className="h-3.5 w-3.5 text-emerald-400" /> : <WifiOff className="h-3.5 w-3.5 text-red-400" />}
              <span className="text-xs text-muted-foreground hidden sm:inline">MQTT Broker</span>
            </div>
            <div className="flex items-center gap-1.5">
              {isEspOnline
                ? <Cpu className="h-3.5 w-3.5 text-emerald-400 animate-pulse" />
                : <Cpu className="h-3.5 w-3.5 text-red-400" />}
              <span className="text-xs text-muted-foreground hidden sm:inline">ESP32 Core</span>
            </div>
            <Badge variant={isArmed ? 'destructive' : 'success'} className="text-xs font-semibold px-2.5 py-0.5">
              {isArmed ? 'ARMADO' : 'DESARMADO'}
            </Badge>
          </div>
        </div>
      </header>

      {/* ── Main grid ── */}
      <main className="w-full flex-1 p-4 grid grid-cols-1 lg:grid-cols-12 gap-4 overflow-y-auto lg:overflow-hidden min-h-0">

        {/* ── Left column: Radar + Camera (7/12) ── */}
        <div className="lg:col-span-7 h-full flex flex-col gap-4 min-h-0">

          {/* Radar */}
          <Card className="flex-1 flex flex-col overflow-hidden card-hover border-glow min-h-0">
            <CardHeader className="pb-2 flex-row items-center justify-between flex-shrink-0">
              <div className="flex items-center gap-2">
                <Radio className="h-4 w-4 text-primary animate-pulse" />
                <CardTitle className="text-sm">Escáner Perimetral Radar (0-90°)</CardTitle>
              </div>
              <div className="flex items-center gap-2">
                {radarData.mode === 'patrol' && <Badge variant="default">Patrullando</Badge>}
                {radarData.mode === 'detection' && <Badge variant="warning">Detectando</Badge>}
                {radarData.mode === 'tracking' && <Badge variant="destructive" className="animate-pulse">Rastreando</Badge>}
                {radarData.mode === 'off' && <Badge variant="secondary">Apagado</Badge>}
              </div>
            </CardHeader>
            <CardContent className="p-2 pt-0 flex-1 min-h-0 flex items-center justify-center bg-[#020a18]/45">
              <div className="w-full h-full rounded-lg overflow-hidden bg-[#020a18] flex items-center justify-center relative">
                <RadarCanvas radarData={radarData} />
              </div>
            </CardContent>
          </Card>

          {/* Camera */}
          <Card className="flex-shrink-0 flex flex-col overflow-hidden card-hover border-glow min-h-0">
            <CardHeader className="pb-2 flex-row items-center justify-between flex-shrink-0">
              <div className="flex items-center gap-2">
                <Camera className="h-4 w-4 text-emerald-400" />
                <CardTitle className="text-sm">Cámara de Seguridad</CardTitle>
              </div>
              {isRecording
                ? <Badge variant="destructive" className="animate-pulse">● REC {formatElapsed(recordingElapsed)}</Badge>
                : webcamActive
                ? <Badge variant="success">PREVIEW</Badge>
                : <Badge variant="secondary">STANDBY</Badge>}
            </CardHeader>

            <CardContent className="p-3 pt-0 flex-shrink-0 space-y-2">

              {/* Video / standby area */}
              {webcamActive ? (
                <div className="w-full aspect-video rounded-lg bg-black border border-border/50 relative overflow-hidden">
                  <video ref={videoRef} autoPlay playsInline muted className="w-full h-full object-cover" />

                  {/* Photo flash overlay */}
                  <div className={`absolute inset-0 bg-white pointer-events-none z-20 transition-opacity duration-300 ${photoFlash ? 'opacity-80' : 'opacity-0'}`} />

                  {/* Tactical frame */}
                  <div className="absolute inset-0 pointer-events-none border border-emerald-500/10 flex items-center justify-center">
                    <div className="absolute top-2 left-2 w-3.5 h-3.5 border-t-2 border-l-2 border-emerald-500/60" />
                    <div className="absolute top-2 right-2 w-3.5 h-3.5 border-t-2 border-r-2 border-emerald-500/60" />
                    <div className="absolute bottom-2 left-2 w-3.5 h-3.5 border-b-2 border-l-2 border-emerald-500/60" />
                    <div className="absolute bottom-2 right-2 w-3.5 h-3.5 border-b-2 border-r-2 border-emerald-500/60" />
                    <div className="w-8 h-8 border border-emerald-500/20 rounded-full flex items-center justify-center">
                      <div className="w-1.5 h-0.5 bg-emerald-500/40" />
                      <div className="h-1.5 w-0.5 bg-emerald-500/40 absolute" />
                    </div>
                  </div>

                  {/* Top HUD */}
                  <div className="absolute top-3 left-3 right-3 flex justify-between items-center pointer-events-none">
                    {isRecording ? (
                      <div className="flex items-center gap-1.5 bg-black/80 px-2 py-1 rounded text-[10px] text-red-400 font-extrabold border border-red-500/30 shadow-lg">
                        <span className="h-2.5 w-2.5 rounded-full bg-red-500 animate-pulse" />
                        REC {formatElapsed(recordingElapsed)}
                      </div>
                    ) : (
                      <div className="bg-black/80 px-2 py-1 rounded text-[10px] text-emerald-400 font-bold border border-emerald-500/20 shadow-lg uppercase tracking-wider">
                        LIVE FEED
                      </div>
                    )}
                    <div className="bg-black/80 px-2 py-1 rounded text-[10px] text-muted-foreground font-mono border border-border/40 shadow-lg">
                      {timeStr}
                    </div>
                  </div>

                  {/* Bottom telemetry */}
                  <div className="absolute bottom-3 left-3 pointer-events-none">
                    <div className="bg-black/80 px-2.5 py-1 rounded text-[10px] text-emerald-400 font-mono border border-emerald-500/25 shadow-lg flex flex-col">
                      <span className="font-bold">CAM-01 (HD)</span>
                      <span>PAN POS: {cameraAngle[0]}°</span>
                    </div>
                  </div>
                </div>
              ) : (
                /* Standby panel */
                <div className="w-full aspect-video rounded-lg bg-[#020a18]/20 border border-border/30 flex flex-col items-center justify-center p-4 text-center">
                  <Shield className="h-10 w-10 text-primary/30 mb-2" />
                  <p className="text-sm font-semibold text-muted-foreground uppercase tracking-wider">Cámara Desconectada</p>
                  <p className="text-[11px] text-muted-foreground mt-1 max-w-[280px] leading-relaxed">
                    Vista previa inactiva. La cámara grabará automáticamente cuando el sistema detecte intrusos.
                  </p>
                  {webcamError && (
                    <div className="mt-2 text-xs text-red-400/90 font-medium bg-red-950/20 border border-red-500/20 px-3 py-1 rounded">
                      ⚠️ {webcamError}
                    </div>
                  )}
                </div>
              )}

              {/* Controls row: servo slider + action buttons */}
              <div className="flex items-center gap-3 px-1">
                {/* Servo slider */}
                <div className="flex-1 flex items-center gap-2 min-w-0">
                  <span className="text-[10px] text-muted-foreground font-mono whitespace-nowrap flex-shrink-0">
                    {cameraAngle[0]}°
                  </span>
                  <Slider value={cameraAngle} onValueChange={moveServo} min={0} max={180} step={1} className="flex-1" />
                  <button
                    onClick={() => moveServo([90])}
                    className="flex-shrink-0 text-muted-foreground hover:text-primary transition-colors"
                    title="Centrar servo (90°)"
                  >
                    <RotateCcw className="h-3.5 w-3.5" />
                  </button>
                </div>

                {/* Action buttons */}
                <div className="flex gap-1.5 flex-shrink-0">
                  {webcamActive ? (
                    <>
                      {/* Photo — only when preview is active */}
                      <Button
                        variant="outline" size="icon" className="h-8 w-8 bg-card hover:bg-emerald-950/30"
                        onClick={() => capturePhoto('manual')}
                        title="Tomar foto"
                      >
                        <Image className="h-3.5 w-3.5 text-emerald-400" />
                      </Button>

                      {/* Record / Stop */}
                      {isRecording ? (
                        <Button variant="destructive" size="icon" className="h-8 w-8" onClick={stopRecording} title="Detener grabación">
                          <Square className="h-3.5 w-3.5" />
                        </Button>
                      ) : (
                        <Button variant="default" size="icon" className="h-8 w-8" onClick={() => startRecording('manual')} title="Grabar">
                          <Play className="h-3.5 w-3.5" />
                        </Button>
                      )}

                      {/* Power off preview */}
                      <Button variant="outline" size="icon" className="h-8 w-8 bg-card hover:bg-red-950/30" onClick={stopPreview} title="Apagar cámara">
                        <Power className="h-3.5 w-3.5 text-red-400" />
                      </Button>
                    </>
                  ) : (
                    <Button variant="outline" size="sm" className="h-8" onClick={showPreview}>
                      <Camera className="h-3.5 w-3.5 mr-1.5 text-emerald-400" />
                      Encender Cámara
                    </Button>
                  )}
                </div>
              </div>
            </CardContent>
          </Card>
        </div>

        {/* ── Right column (5/12) ── */}
        <div className="lg:col-span-5 h-full flex flex-col gap-4 min-h-0">

          {/* Arm / Disarm */}
          <button
            onClick={toggleArm}
            className={`w-full p-4 rounded-xl border-2 transition-all duration-300 flex items-center justify-center gap-3 flex-shrink-0 ${
              isArmed
                ? 'border-red-500/40 bg-red-500/5 hover:bg-red-500/10 glow-red'
                : 'border-emerald-500/40 bg-emerald-500/5 hover:bg-emerald-500/10 glow-green'
            }`}
          >
            <div className={`p-3 rounded-full transition-all ${isArmed ? 'bg-red-500/20 text-red-400' : 'bg-emerald-500/20 text-emerald-400'}`}>
              {isArmed ? <ShieldOff className="h-6 w-6" /> : <Power className="h-6 w-6" />}
            </div>
            <div className="text-left">
              <p className={`text-md font-extrabold uppercase tracking-wide leading-tight ${isArmed ? 'text-red-400' : 'text-emerald-400'}`}>
                {isArmed ? 'DESARMAR SISTEMA' : 'ARMAR SISTEMA'}
              </p>
              <p className="text-[10px] text-muted-foreground mt-0.5">
                {isArmed ? 'Apagar sensores y alertas' : 'Vigilar todo el perímetro'}
              </p>
            </div>
          </button>

          {/* Quick Actions */}
          <div className="grid grid-cols-4 gap-2 flex-shrink-0">
            {quickActions.map((a) => (
              <button key={a.label} onClick={a.onClick}
                className="quick-action flex flex-col items-center gap-1.5 p-3 rounded-lg bg-card border border-border hover:border-primary/30 hover:bg-secondary/20 transition-all">
                <a.icon className={`h-5 w-5 ${a.color}`} />
                <span className="text-[10px] sm:text-xs text-muted-foreground font-medium">{a.label}</span>
              </button>
            ))}
          </div>

          {/* Sensors 2×2 */}
          <div className="grid grid-cols-2 gap-2 flex-shrink-0">
            <Card className="card-hover">
              <CardContent className="p-3">
                <div className="flex items-center gap-2 mb-1">
                  <Thermometer className="h-4 w-4 text-orange-400" />
                  <span className="text-[10px] text-muted-foreground font-medium">Temperatura</span>
                </div>
                <p className="text-xl font-bold font-mono text-orange-400 leading-tight">
                  {climateData.temperature != null ? Number(climateData.temperature).toFixed(1) : '--'}
                  <span className="text-xs text-muted-foreground ml-0.5 font-normal">°C</span>
                </p>
              </CardContent>
            </Card>
            <Card className="card-hover">
              <CardContent className="p-3">
                <div className="flex items-center gap-2 mb-1">
                  <Droplets className="h-4 w-4 text-blue-400" />
                  <span className="text-[10px] text-muted-foreground font-medium">Humedad</span>
                </div>
                <p className="text-xl font-bold font-mono text-blue-400 leading-tight">
                  {climateData.humidity != null ? Number(climateData.humidity).toFixed(0) : '--'}
                  <span className="text-xs text-muted-foreground ml-0.5 font-normal">%</span>
                </p>
              </CardContent>
            </Card>
            <Card className="card-hover">
              <CardContent className="p-3">
                <div className="flex items-center gap-2 mb-1">
                  {lightStatus.dark ? <Moon className="h-4 w-4 text-indigo-400" /> : <Sun className="h-4 w-4 text-yellow-400" />}
                  <span className="text-[10px] text-muted-foreground font-medium font-sans">Iluminación</span>
                </div>
                <p className={`text-xs font-bold uppercase tracking-wide leading-tight ${lightStatus.dark ? 'text-indigo-400' : 'text-yellow-400'}`}>
                  {lightStatus.dark ? 'Oscuro' : 'Iluminado'}
                </p>
                {lightStatus.flashlight && <Badge variant="warning" className="text-[9px] py-0 px-1 mt-1 scale-90 origin-left">Linterna ON</Badge>}
              </CardContent>
            </Card>
            <Card className="card-hover">
              <CardContent className="p-3">
                <div className="flex items-center gap-2 mb-1">
                  {doorStatus.open ? <DoorOpen className="h-4 w-4 text-emerald-400" /> : <DoorClosed className="h-4 w-4 text-muted-foreground" />}
                  <span className="text-[10px] text-muted-foreground font-medium">Puerta</span>
                </div>
                <p className={`text-xs font-bold uppercase tracking-wide leading-tight ${doorStatus.open ? 'text-emerald-400' : 'text-muted-foreground'}`}>
                  {doorStatus.open ? 'Abierta' : 'Cerrada'}
                </p>
              </CardContent>
            </Card>
          </div>

          {/* System Info */}
          <Card className="card-hover flex-shrink-0">
            <CardContent className="p-2 flex items-center justify-between text-[11px] text-muted-foreground font-mono">
              <div className="flex items-center gap-1.5">
                <Activity className="h-3 w-3 text-cyan-400" />
                <span>Uptime: {systemStatus.uptime ? `${Math.floor(systemStatus.uptime / 3600)}h ${Math.floor((systemStatus.uptime % 3600) / 60)}m` : '--'}</span>
              </div>
              <span>RAM: {systemStatus.freeHeap ? `${(systemStatus.freeHeap / 1024).toFixed(0)}KB` : '--'}</span>
            </CardContent>
          </Card>

          {/* Events log */}
          <Card className="flex-1 flex flex-col overflow-hidden card-hover min-h-0">
            <CardHeader className="pb-2 pt-3 flex-row items-center justify-between flex-shrink-0">
              <div className="flex items-center gap-1.5">
                <AlertTriangle className="h-4 w-4 text-yellow-400" />
                <CardTitle className="text-xs font-semibold uppercase tracking-wider">Historial de Eventos</CardTitle>
              </div>
              <Badge variant="secondary" className="text-[10px] py-0.5 px-1.5 font-mono">{events.length}</Badge>
            </CardHeader>
            <CardContent className="p-1 pt-0 flex-1 min-h-0">
              <ScrollArea className="h-full">
                <div className="space-y-1 px-1">
                  {events.length === 0 && (
                    <p className="text-xs text-muted-foreground text-center py-10">Ningún evento registrado aún.</p>
                  )}
                  {events.map((ev, i) => {
                    let data = {};
                    try { data = typeof ev.data === 'string' ? JSON.parse(ev.data) : (ev.data || {}); } catch {}
                    return (
                      <div key={ev.id || i} className={`flex items-start gap-2 px-3 py-2 rounded bg-secondary/20 border-l-2 ${eventColor(ev.type)} hover:bg-secondary/40 transition-colors`}>
                        <div className="mt-0.5">{eventIcon(ev.type)}</div>
                        <div className="flex-1 min-w-0">
                          <div className="flex items-center justify-between gap-1">
                            <span className="text-[11px] font-semibold truncate text-foreground">{eventLabel(ev.type)}</span>
                            <span className="text-[9px] text-muted-foreground font-mono flex-shrink-0 flex items-center gap-0.5">
                              <Clock className="h-2.5 w-2.5" /> {formatTime(ev.timestamp)}
                            </span>
                          </div>
                          {data.distance && <span className="text-[9px] text-muted-foreground block mt-0.5 leading-tight">Distancia: {data.distance}cm • Ángulo: {data.angle}°</span>}
                          {data.uid && <span className="text-[9px] text-muted-foreground block mt-0.5 leading-tight">{data.name || data.uid} — {data.granted ? '✓ Autorizado' : '✗ Denegado'}</span>}
                          {data.open != null && <span className="text-[9px] text-muted-foreground block mt-0.5 leading-tight">{data.open ? 'Puerta Abierta' : 'Puerta Cerrada'} ({data.reason})</span>}
                        </div>
                      </div>
                    );
                  })}
                </div>
              </ScrollArea>
            </CardContent>
          </Card>
        </div>
      </main>

      {/* Modals */}
      <BLEManager open={bleOpen} onOpenChange={setBleOpen} discoveredDevices={discoveredBLE} sendCommand={sendCommand} />
      <RFIDManager open={rfidOpen} onOpenChange={setRfidOpen} discoveredCards={discoveredRFID} />
      <ManualControl open={manualOpen} onOpenChange={setManualOpen} sendCommand={sendCommand} />
      <MediaGallery open={mediaOpen} onOpenChange={setMediaOpen} socket={socket} />
    </div>
  );
}
