import { useState, useEffect } from 'react';
import { ResponsiveDialog } from '@/components/ResponsiveDialog';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Badge } from '@/components/ui/badge';
import { CreditCard, Plus, Trash2, Shield, User, Info } from 'lucide-react';

export default function RFIDManager({ open, onOpenChange, discoveredCards = [] }) {
  const [cards, setCards] = useState([]);
  const [discovered, setDiscovered] = useState([]);
  const [uid, setUid] = useState('');
  const [name, setName] = useState('');
  const [level, setLevel] = useState('guest');
  const [loading, setLoading] = useState(false);

  const load = () => {
    fetch('/api/rfid-cards').then(r => r.json()).then(setCards).catch(() => {});
  };

  const loadDiscovered = () => {
    fetch('/api/rfid-discovered').then(r => r.json()).then(setDiscovered).catch(() => {});
  };

  useEffect(() => {
    if (open) {
      load();
      loadDiscovered();
    }
  }, [open]);

  useEffect(() => {
    if (discoveredCards.length > 0) {
      // Filter out registered UIDs from discovered cards
      const registeredUids = cards.map(c => c.uid.toUpperCase());
      const filtered = discoveredCards.filter(c => !registeredUids.includes(c.uid.toUpperCase()));
      setDiscovered(filtered);
    }
  }, [discoveredCards, cards]);

  const add = async () => {
    if (!uid || !name) return;
    setLoading(true);
    await fetch('/api/rfid-cards', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ uid: uid.toUpperCase().trim(), name: name.trim(), level })
    });
    setUid('');
    setName('');
    setLevel('guest');
    load();
    loadDiscovered();
    setLoading(false);
  };

  const remove = async (u) => {
    await fetch(`/api/rfid-cards/${encodeURIComponent(u)}`, { method: 'DELETE' });
    load();
    loadDiscovered();
  };

  // Convert timestamp to relative time string
  const getRelativeTime = (ts) => {
    const diff = Math.round((Date.now() - ts) / 1000);
    if (diff < 60) return `hace ${diff}s`;
    return `hace ${Math.floor(diff / 60)}m`;
  };

  return (
    <ResponsiveDialog open={open} onOpenChange={onOpenChange} title="Tarjetas RFID" description="Administra las tarjetas de acceso registradas en el sistema">
      <div className="space-y-4">
        {/* Add form */}
        <div className="bg-secondary/10 p-3 rounded-lg border border-border/40 space-y-3">
          <div className="grid grid-cols-[1fr_1fr] gap-2">
            <div>
              <Label className="text-[11px] text-muted-foreground mb-1 block">UID de Tarjeta</Label>
              <Input placeholder="A1B2C3D4" value={uid} onChange={e => setUid(e.target.value)} className="h-8 text-xs font-mono uppercase" />
            </div>
            <div>
              <Label className="text-[11px] text-muted-foreground mb-1 block">Propietario / Nombre</Label>
              <Input placeholder="Juan Pérez" value={name} onChange={e => setName(e.target.value)} className="h-8 text-xs" />
            </div>
          </div>
          <div className="flex items-center justify-between gap-2">
            <div>
              <Label className="text-[11px] text-muted-foreground mb-1 block">Nivel de Acceso</Label>
              <div className="flex gap-1.5">
                <Button variant={level === 'owner' ? 'default' : 'outline'} size="sm" onClick={() => setLevel('owner')} className="h-7 text-xs px-2.5">
                  <Shield className="h-3 w-3 mr-1" /> Propietario
                </Button>
                <Button variant={level === 'guest' ? 'default' : 'outline'} size="sm" onClick={() => setLevel('guest')} className="h-7 text-xs px-2.5">
                  <User className="h-3 w-3 mr-1" /> Invitado
                </Button>
              </div>
            </div>
            <Button size="sm" onClick={add} disabled={loading || !uid || !name} className="h-8 px-4">
              <Plus className="h-4 w-4 mr-1" /> Registrar
            </Button>
          </div>
        </div>

        {/* Discovered RFID cards list */}
        <div className="space-y-2 border-t border-border/40 pt-3">
          <div className="flex items-center justify-between">
            <h3 className="text-xs font-semibold text-muted-foreground uppercase tracking-wider">Últimas Tarjetas Escaneadas</h3>
            <Badge variant="outline" className="text-[10px] py-0 px-1.5 font-normal flex items-center gap-0.5">
              <Info className="h-2.5 w-2.5" /> Acerca la tarjeta al lector
            </Badge>
          </div>
          {discovered.length === 0 ? (
            <p className="text-xs text-muted-foreground text-center py-4 bg-secondary/5 rounded border border-dashed border-border/30">
              Escanea una tarjeta no registrada en el lector físico para que aparezca aquí automáticamente.
            </p>
          ) : (
            <div className="space-y-1.5 max-h-[120px] overflow-y-auto pr-1">
              {discovered.map((c) => (
                <div key={c.uid} className="flex items-center justify-between px-2.5 py-1.5 rounded bg-secondary/20 border border-border/30 hover:border-primary/20 transition-all text-xs">
                  <div className="flex items-center gap-2">
                    <CreditCard className="h-3.5 w-3.5 text-cyan-400" />
                    <div>
                      <p className="font-mono font-medium text-foreground">{c.uid}</p>
                      <p className="text-[9px] text-muted-foreground">{getRelativeTime(c.timestamp)}</p>
                    </div>
                  </div>
                  <Button size="sm" variant="ghost" className="h-6 px-2 text-[10px] text-primary hover:text-primary-foreground hover:bg-primary" onClick={() => setUid(c.uid)}>
                    Seleccionar
                  </Button>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Registered cards list */}
        <div className="space-y-2 border-t border-border/40 pt-3">
          <h3 className="text-xs font-semibold text-muted-foreground uppercase tracking-wider">Tarjetas Registradas</h3>
          {cards.length === 0 ? (
            <p className="text-xs text-muted-foreground text-center py-6">No hay tarjetas de acceso registradas</p>
          ) : (
            <div className="space-y-1.5 max-h-[180px] overflow-y-auto pr-1">
              {cards.map((c) => (
                <div key={c.uid} className="flex items-center justify-between px-3 py-2.5 rounded-lg bg-secondary/50 border border-border">
                  <div className="flex items-center gap-3">
                    <div className="p-1.5 rounded-md bg-primary/15 text-primary">
                      <CreditCard className="h-4 w-4" />
                    </div>
                    <div>
                      <p className="text-sm font-medium">{c.name}</p>
                      <p className="text-xs text-muted-foreground font-mono">{c.uid}</p>
                    </div>
                  </div>
                  <div className="flex items-center gap-2">
                    <Badge variant={c.level === 'owner' ? 'default' : 'warning'} className="text-xs">
                      {c.level === 'owner' ? 'Propietario' : 'Invitado'}
                    </Badge>
                    <Button variant="ghost" size="icon" className="h-7 w-7 text-muted-foreground hover:text-red-400" onClick={() => remove(c.uid)}>
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
