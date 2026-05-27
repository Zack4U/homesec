import { useState } from 'react';
import { ResponsiveDialog } from '@/components/ResponsiveDialog';
import { Button } from '@/components/ui/button';
import { Slider } from '@/components/ui/slider';
import { Label } from '@/components/ui/label';
import { Camera, RotateCcw, Play, Square, Image } from 'lucide-react';

export default function CameraControl({ open, onOpenChange, sendCommand, cameraActive }) {
  const [angle, setAngle] = useState([90]);

  const moveCamera = (val) => {
    setAngle(val);
    sendCommand('control/camera', { action: 'move', angle: val[0] });
  };

  return (
    <ResponsiveDialog open={open} onOpenChange={onOpenChange} title="Control de Cámara" description="Control manual del servo de la cámara y grabación">
      <div className="space-y-6">
        {/* Camera status */}
        <div className={`flex items-center justify-center gap-3 p-4 rounded-lg border ${cameraActive ? 'border-red-500/30 bg-red-500/5' : 'border-border bg-secondary/30'}`}>
          <div className={`p-2 rounded-full ${cameraActive ? 'bg-red-500/20 text-red-400 animate-pulse' : 'bg-muted text-muted-foreground'}`}>
            <Camera className="h-5 w-5" />
          </div>
          <span className={`text-sm font-medium ${cameraActive ? 'text-red-400' : 'text-muted-foreground'}`}>
            {cameraActive ? '● GRABANDO' : '○ Inactiva'}
          </span>
        </div>

        {/* Angle control */}
        <div>
          <Label className="text-xs text-muted-foreground mb-3 block">Ángulo del Servo: {angle[0]}°</Label>
          <Slider value={angle} onValueChange={moveCamera} max={180} step={1} className="mt-2" />
          <div className="flex justify-between text-xs text-muted-foreground mt-1">
            <span>0°</span><span>90°</span><span>180°</span>
          </div>
        </div>

        {/* Actions */}
        <div className="grid grid-cols-3 gap-2">
          <Button variant="outline" size="sm" onClick={() => sendCommand('control/camera', { action: 'take_photo' })}>
            <Image className="h-4 w-4 mr-1" /> Foto
          </Button>
          <Button variant={cameraActive ? 'destructive' : 'default'} size="sm" onClick={() => sendCommand('control/camera', { action: cameraActive ? 'stop_recording' : 'start_recording' })}>
            {cameraActive ? <><Square className="h-3.5 w-3.5 mr-1" /> Parar</> : <><Play className="h-3.5 w-3.5 mr-1" /> Grabar</>}
          </Button>
          <Button variant="outline" size="sm" onClick={() => { setAngle([90]); sendCommand('control/camera', { action: 'move', angle: 90 }); }}>
            <RotateCcw className="h-4 w-4 mr-1" /> Reset
          </Button>
        </div>
      </div>
    </ResponsiveDialog>
  );
}
