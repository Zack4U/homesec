import { useState } from 'react';
import { ResponsiveDialog } from '@/components/ResponsiveDialog';
import { Button } from '@/components/ui/button';
import { Slider } from '@/components/ui/slider';
import { Label } from '@/components/ui/label';
import { Switch } from '@/components/ui/switch';
import { Settings, Radar, Lightbulb, DoorOpen } from 'lucide-react';

export default function ManualControl({ open, onOpenChange, sendCommand }) {
  const [radarAngle, setRadarAngle] = useState([90]);
  const [doorOpen, setDoorOpen] = useState(false);
  const [flashlight, setFlashlight] = useState(false);

  return (
    <ResponsiveDialog open={open} onOpenChange={onOpenChange} title="Control Manual" description="Control manual de servos, linterna y otros actuadores">
      <div className="space-y-6">
        {/* Radar servo */}
        <div className="space-y-2">
          <div className="flex items-center gap-2">
            <Radar className="h-4 w-4 text-primary" />
            <Label className="text-sm font-medium">Servo Radar: {radarAngle[0]}°</Label>
          </div>
          <Slider value={radarAngle} onValueChange={(v) => { setRadarAngle(v); sendCommand('control/servo', { target: 'radar', angle: v[0] }); }} max={180} step={1} />
          <div className="flex justify-between text-xs text-muted-foreground"><span>0°</span><span>90°</span><span>180°</span></div>
        </div>

        {/* Flashlight */}
        <div className="flex items-center justify-between p-3 rounded-lg bg-secondary/50 border border-border">
          <div className="flex items-center gap-3">
            <Lightbulb className={`h-5 w-5 ${flashlight ? 'text-yellow-400' : 'text-muted-foreground'}`} />
            <div>
              <p className="text-sm font-medium">Linterna LED</p>
              <p className="text-xs text-muted-foreground">GPIO 27 - PWM</p>
            </div>
          </div>
          <Switch checked={flashlight} onCheckedChange={(v) => { setFlashlight(v); sendCommand('control/flashlight', { on: v }); }} />
        </div>

        {/* Door */}
        <div className="flex items-center justify-between p-3 rounded-lg bg-secondary/50 border border-border">
          <div className="flex items-center gap-3">
            <DoorOpen className={`h-5 w-5 ${doorOpen ? 'text-emerald-400' : 'text-muted-foreground'}`} />
            <div>
              <p className="text-sm font-medium">Puerta</p>
              <p className="text-xs text-muted-foreground">Servo GPIO 17</p>
            </div>
          </div>
          <Switch checked={doorOpen} onCheckedChange={(v) => { setDoorOpen(v); sendCommand('control/door', { open: v }); }} />
        </div>

        {/* Radar mode presets */}
        <div>
          <Label className="text-xs text-muted-foreground mb-2 block">Modo Radar</Label>
          <div className="grid grid-cols-2 gap-2">
            <Button variant="outline" size="sm" onClick={() => sendCommand('config/radar', { min_angle: 0, max_angle: 90 })}>0° – 90°</Button>
            <Button variant="outline" size="sm" onClick={() => sendCommand('config/radar', { min_angle: 0, max_angle: 180 })}>0° – 180°</Button>
            <Button variant="outline" size="sm" onClick={() => sendCommand('config/radar', { min_angle: 45, max_angle: 135 })}>45° – 135°</Button>
            <Button variant="outline" size="sm" onClick={() => sendCommand('config/detection', { threshold: 100 })}>Rango 100cm</Button>
          </div>
        </div>
      </div>
    </ResponsiveDialog>
  );
}
