import { useState, useEffect } from 'react';
import { ResponsiveDialog } from '@/components/ResponsiveDialog';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Badge } from '@/components/ui/badge';
import { Slider } from '@/components/ui/slider';
import { Bluetooth, Plus, Trash2, Wifi, WifiOff, ShieldCheck, KeyRound } from 'lucide-react';

export default function BLEManager({ open, onOpenChange, discoveredDevices = [], sendCommand }) {
  const [devices, setDevices] = useState([]);
  const [discovered, setDiscovered] = useState([]);
  const [mac, setMac] = useState('');
  const [name, setName] = useState('');
  const [loading, setLoading] = useState(false);

  // RSSI Threshold states
  const [rssiClose, setRssiClose] = useState([-55]);
  const [rssiNear, setRssiNear] = useState([-70]);

  const load = () => {
    fetch('/api/ble-devices').then(r => r.json()).then(setDevices).catch(() => {});
  };

  const loadDiscovered = () => {
    fetch('/api/ble-discovered').then(r => r.json()).then(setDiscovered).catch(() => {});
  };

  useEffect(() => {
    if (open) {
      load();
      loadDiscovered();
    }
  }, [open]);

  useEffect(() => {
    if (discoveredDevices.length > 0) {
      const registeredMacs = devices.map(d => d.mac.toUpperCase());
      const filtered = discoveredDevices.filter(d => !registeredMacs.includes(d.mac.toUpperCase()));
      // Sort by RSSI descending (strongest signal first)
      filtered.sort((a, b) => b.rssi - a.rssi);
      setDiscovered(filtered);
    }
  }, [discoveredDevices, devices]);

  const add = async () => {
    if (!mac || !name) return;
    setLoading(true);
    await fetch('/api/ble-devices', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ mac: mac.toUpperCase().trim(), name: name.trim() })
    });
    setMac('');
    setName('');
    load();
    loadDiscovered();
    setLoading(false);
  };

  const remove = async (m) => {
    await fetch(`/api/ble-devices/${encodeURIComponent(m)}`, { method: 'DELETE' });
    load();
    loadDiscovered();
  };

  const saveThresholds = () => {
    if (sendCommand) {
      sendCommand('config/ble', { rssi_close: rssiClose[0], rssi_near: rssiNear[0] });
    }
  };

  return (
    <ResponsiveDialog open={open} onOpenChange={onOpenChange} title="Dispositivos Bluetooth" description="Administra la detección de proximidad por BLE para desbloqueo automático">
      <div className="space-y-5">
        
        {/* Proximity Sensitivity Controls */}
        <div className="bg-secondary/15 p-4 rounded-xl border border-border/40 space-y-4">
          <h3 className="text-xs font-semibold text-primary uppercase tracking-wider mb-1">Ajustes de Sensibilidad de Proximidad</h3>
          
          {/* Close Threshold (Door open) */}
          <div className="space-y-2">
            <div className="flex justify-between text-xs">
              <span className="flex items-center gap-1 text-emerald-400 font-medium">
                <KeyRound className="h-3.5 w-3.5" /> Apertura de Puerta
              </span>
              <span className="font-mono text-muted-foreground">{rssiClose[0]} dBm</span>
            </div>
            <Slider value={rssiClose} onValueChange={setRssiClose} min={-90} max={-30} step={1} />
            <p className="text-[10px] text-muted-foreground">
              Abre la puerta automáticamente. Valores cercanos a <span className="text-emerald-400 font-mono">-40 dBm</span> requieren estar a escasos centímetros. Valores como <span className="text-emerald-400 font-mono">-65 dBm</span> permiten abrirla desde lejos.
            </p>
          </div>

          {/* Near Threshold (Disable radar alerts) */}
          <div className="space-y-2">
            <div className="flex justify-between text-xs">
              <span className="flex items-center gap-1 text-cyan-400 font-medium">
                <ShieldCheck className="h-3.5 w-3.5" /> Silenciar Alarmas/Radar
              </span>
              <span className="font-mono text-muted-foreground">{rssiNear[0]} dBm</span>
            </div>
            <Slider value={rssiNear} onValueChange={setRssiNear} min={-90} max={-30} step={1} />
            <p className="text-[10px] text-muted-foreground">
              Desactiva el radar y el sensor de intrusión cuando te aproximas. Recomendado: <span className="text-cyan-400 font-mono">-70 dBm</span>.
            </p>
          </div>

          <Button size="sm" onClick={saveThresholds} className="w-full h-8 text-xs bg-primary/20 hover:bg-primary/30 text-primary border border-primary/30">
            Aplicar Umbrales de Señal
          </Button>
        </div>

        {/* Add form */}
        <div className="grid grid-cols-[1fr_1fr_auto] gap-2 items-end bg-secondary/10 p-3 rounded-lg border border-border/40">
          <div>
            <Label className="text-[11px] text-muted-foreground mb-1 block">Dirección MAC</Label>
            <Input placeholder="AA:BB:CC:DD:EE:FF" value={mac} onChange={e => setMac(e.target.value)} className="h-8 text-xs font-mono" />
          </div>
          <div>
            <Label className="text-[11px] text-muted-foreground mb-1 block">Nombre Amigable</Label>
            <Input placeholder="Mi Teléfono" value={name} onChange={e => setName(e.target.value)} className="h-8 text-xs" />
          </div>
          <Button size="sm" onClick={add} disabled={loading || !mac || !name} className="h-8 px-3">
            <Plus className="h-4 w-4 mr-1" /> Registrar
          </Button>
        </div>

        {/* Discovered nearby devices list */}
        <div className="space-y-2 border-t border-border/40 pt-3">
          <div className="flex items-center justify-between">
            <h3 className="text-xs font-semibold text-muted-foreground uppercase tracking-wider">Dispositivos Cercanos Detectados</h3>
            <Badge variant="outline" className="text-[10px] py-0 px-1.5 font-normal">Escaneo Activo</Badge>
          </div>
          {discovered.length === 0 ? (
            <p className="text-xs text-muted-foreground text-center py-4 bg-secondary/5 rounded border border-dashed border-border/30">
              No hay dispositivos detectados cerca. Asegúrate de encender el Bluetooth o la opción de visibilidad en tu dispositivo.
            </p>
          ) : (
            <div className="space-y-1.5 max-h-[140px] overflow-y-auto pr-1">
              {discovered.map((d) => (
                <div key={d.mac} className="flex items-center justify-between px-2.5 py-1.5 rounded bg-secondary/20 border border-border/30 hover:border-primary/20 transition-all text-xs">
                  <div>
                    <p className="font-medium text-foreground">{d.name}</p>
                    <p className="font-mono text-[9px] text-muted-foreground">{d.mac}</p>
                  </div>
                  <div className="flex items-center gap-2">
                    <Badge variant="secondary" className="text-[9px] py-0 px-1 font-mono">
                      <Wifi className="h-2.5 w-2.5 mr-0.5 text-emerald-400 inline" /> {d.rssi}dBm
                    </Badge>
                    <Button size="sm" variant="ghost" className="h-6 px-2 text-[10px] text-primary hover:text-primary-foreground hover:bg-primary" onClick={() => { setMac(d.mac); setName(d.name !== 'Dispositivo Desconocido' ? d.name : ''); }}>
                      Seleccionar
                    </Button>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Registered device list */}
        <div className="space-y-2 border-t border-border/40 pt-3">
          <h3 className="text-xs font-semibold text-muted-foreground uppercase tracking-wider">Llaveros y Dispositivos de Confianza</h3>
          {devices.length === 0 ? (
            <p className="text-xs text-muted-foreground text-center py-6">No hay dispositivos registrados</p>
          ) : (
            <div className="space-y-1.5 max-h-[180px] overflow-y-auto pr-1">
              {devices.map((d) => (
                <div key={d.mac} className="flex items-center justify-between px-3 py-2.5 rounded-lg bg-secondary/50 border border-border">
                  <div className="flex items-center gap-3">
                    <div className={`p-1.5 rounded-md ${d.active ? 'bg-primary/15 text-primary' : 'bg-muted text-muted-foreground'}`}>
                      <Bluetooth className="h-4 w-4" />
                    </div>
                    <div>
                      <p className="text-sm font-medium">{d.name}</p>
                      <p className="text-xs text-muted-foreground font-mono">{d.mac}</p>
                    </div>
                  </div>
                  <div className="flex items-center gap-2">
                    {d.active ? (
                      <Badge variant="success" className="text-xs">
                        <Wifi className="h-3 w-3 mr-1" /> {d.rssi}dBm
                      </Badge>
                    ) : (
                      <Badge variant="secondary" className="text-xs">
                        <WifiOff className="h-3 w-3 mr-1" /> Offline
                      </Badge>
                    )}
                    <Button variant="ghost" size="icon" className="h-7 w-7 text-muted-foreground hover:text-red-400" onClick={() => remove(d.mac)}>
                      <Trash2 className="h-3.5 w-3.5" />
                    </Button>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>
    </ResponsiveDialog>
  );
}
