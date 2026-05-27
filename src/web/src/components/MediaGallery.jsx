import { useState, useEffect, useMemo } from 'react';
import { ResponsiveDialog } from '@/components/ResponsiveDialog';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Camera, Image, Video, Download, ExternalLink, Trash2, Clock } from 'lucide-react';

export default function MediaGallery({ open, onOpenChange, socket }) {
  const [records, setRecords] = useState([]);
  const [filter, setFilter] = useState('all'); // 'all', 'photo', 'video'

  const fetchRecords = () => {
    fetch('/api/media-records?limit=50')
      .then((r) => r.json())
      .then((data) => {
        // Map notes as filename for backwards compatibility
        const mapped = data.map(r => ({
          ...r,
          filename: r.notes || ''
        }));
        setRecords(mapped);
      })
      .catch((err) => console.error('[Gallery] Fetch error:', err));
  };

  useEffect(() => {
    if (open) {
      fetchRecords();
    }
  }, [open]);

  // Real-time socket updates
  useEffect(() => {
    if (!socket) return;

    const handleNewMedia = (newRecord) => {
      setRecords((prev) => {
        // Prevent duplicates
        if (prev.some(r => r.notes === newRecord.notes)) return prev;
        return [{ ...newRecord, filename: newRecord.notes }, ...prev];
      });
    };

    const handleDeleteMedia = ({ id }) => {
      setRecords((prev) => prev.filter((r) => r.id !== id));
    };

    socket.on('media:new', handleNewMedia);
    socket.on('media:delete', handleDeleteMedia);

    return () => {
      socket.off('media:new', handleNewMedia);
      socket.off('media:delete', handleDeleteMedia);
    };
  }, [socket]);

  const handleDelete = async (id) => {
    if (!confirm('¿Estás seguro de que deseas eliminar este archivo de media?')) return;
    try {
      const res = await fetch(`/api/media-records/${id}`, { method: 'DELETE' });
      if (res.ok) {
        setRecords((prev) => prev.filter((r) => r.id !== id));
      }
    } catch (err) {
      console.error('[Gallery] Delete error:', err);
    }
  };

  const getBadgeVariant = (action) => {
    switch (action) {
      case 'radar_alert': return 'destructive';
      case 'avoidance': return 'warning';
      case 'rfid_door': return 'success';
      case 'radar_detect': return 'default';
      case 'ble_proximity': return 'secondary';
      default: return 'outline';
    }
  };

  const getActionLabel = (action) => {
    const labels = {
      radar_alert: 'Alerta Radar',
      radar_detect: 'Detección Radar',
      avoidance: 'Intrusión IR',
      rfid_door: 'Acceso Puerta',
      ble_proximity: 'Proximidad BLE',
      manual: 'Manual',
    };
    return labels[action] || action;
  };

  const filteredRecords = useMemo(() => {
    if (filter === 'all') return records;
    return records.filter((r) => r.type === filter);
  }, [records, filter]);

  return (
    <ResponsiveDialog
      open={open}
      onOpenChange={onOpenChange}
      title="Galería de Seguridad"
      description="Historial de capturas de fotos y grabaciones de video guardadas en el sistema"
    >
      <div className="flex flex-col h-[75vh] md:h-[650px] space-y-4">
        {/* Filters */}
        <div className="flex items-center gap-1.5 border-b border-border/40 pb-2">
          <Button
            variant={filter === 'all' ? 'default' : 'outline'}
            size="sm"
            onClick={() => setFilter('all')}
            className="h-8 text-xs font-semibold"
          >
            Todos
          </Button>
          <Button
            variant={filter === 'photo' ? 'default' : 'outline'}
            size="sm"
            onClick={() => setFilter('photo')}
            className="h-8 text-xs font-semibold gap-1"
          >
            <Image className="h-3 w-3" /> Fotos
          </Button>
          <Button
            variant={filter === 'video' ? 'default' : 'outline'}
            size="sm"
            onClick={() => setFilter('video')}
            className="h-8 text-xs font-semibold gap-1"
          >
            <Video className="h-3 w-3" /> Videos
          </Button>
        </div>

        {/* Media Grid */}
        <ScrollArea className="flex-1 pr-2">
          {filteredRecords.length === 0 ? (
            <div className="flex flex-col items-center justify-center py-20 text-center text-muted-foreground">
              <Camera className="h-12 w-12 text-muted-foreground/30 mb-3" />
              <p className="text-sm font-semibold uppercase tracking-wider">Sin Archivos Guardados</p>
              <p className="text-xs mt-1 max-w-[280px]">No se encontraron registros de media del tipo seleccionado.</p>
            </div>
          ) : (
            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 pb-4">
              {filteredRecords.map((r, i) => (
                <div
                  key={r.id || i}
                  className="group relative flex flex-col overflow-hidden rounded-xl border border-border/80 bg-card/60 transition-all duration-300 hover:border-primary/40 hover:shadow-md hover:shadow-primary/5"
                >
                  {/* Media Content Container */}
                  <div className="relative aspect-video w-full overflow-hidden bg-black/80 flex items-center justify-center border-b border-border/40">
                    {r.type === 'photo' ? (
                      <img
                        src={`/uploads/${r.notes}`}
                        alt={r.action}
                        className="h-full w-full object-cover transition-transform duration-500 group-hover:scale-105"
                        loading="lazy"
                      />
                    ) : (
                      <video
                        src={`/uploads/${r.notes}`}
                        className="h-full w-full object-cover"
                        controls
                        preload="metadata"
                      />
                    )}
                  </div>

                  {/* Metadata and Actions */}
                  <div className="p-3 flex flex-col flex-1 gap-2 bg-[#080d19]/40">
                    <div className="flex items-center justify-between gap-2">
                      <Badge variant={getBadgeVariant(r.action)}>
                        {getActionLabel(r.action)}
                      </Badge>
                      <span className="text-[10px] text-muted-foreground font-mono uppercase font-semibold">
                        {r.type === 'photo' ? 'FOTO' : 'VIDEO'}
                      </span>
                    </div>

                    <div className="flex items-center gap-1.5 text-[11px] text-muted-foreground font-mono">
                      <Clock className="h-3 w-3 text-primary/75" />
                      <span>
                        {new Date(r.timestamp).toLocaleString('es-ES', {
                          day: '2-digit',
                          month: '2-digit',
                          hour: '2-digit',
                          minute: '2-digit',
                          second: '2-digit'
                        })}
                      </span>
                    </div>

                    <div className="flex items-center gap-2 mt-2">
                      <a
                        href={`/uploads/${r.notes}`}
                        download={r.notes}
                        className="flex-1"
                      >
                        <Button variant="outline" size="sm" className="w-full h-8 text-xs gap-1 hover:bg-primary/10">
                          <Download className="h-3 w-3" /> Descargar
                        </Button>
                      </a>
                      <a
                        href={`/uploads/${r.notes}`}
                        target="_blank"
                        rel="noreferrer"
                      >
                        <Button variant="ghost" size="icon" className="h-8 w-8 hover:bg-secondary" title="Abrir en pestaña nueva">
                          <ExternalLink className="h-3.5 w-3.5" />
                        </Button>
                      </a>
                      <Button
                        variant="ghost"
                        size="icon"
                        onClick={() => handleDelete(r.id)}
                        className="h-8 w-8 text-red-400 hover:text-red-300 hover:bg-red-950/20"
                        title="Eliminar registro"
                      >
                        <Trash2 className="h-3.5 w-3.5" />
                      </Button>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          )}
        </ScrollArea>
      </div>
    </ResponsiveDialog>
  );
}
