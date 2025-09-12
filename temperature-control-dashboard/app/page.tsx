"use client"

import { useState, useEffect } from "react"
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select"
import { Badge } from "@/components/ui/badge"
import { Separator } from "@/components/ui/separator"
import { Switch } from "@/components/ui/switch"
import { Slider } from "@/components/ui/slider"
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from "recharts"
import { Thermometer, Activity, Settings, Zap, Fan, AlertTriangle, CheckCircle } from "lucide-react"

interface TemperatureData {
  timestamp: string
  sensor1: number
  sensor2: number
  sensor3: number
  sensor4: number
  ambient: number
  setpoint: number
}

interface SystemStatus {
  isConnected: boolean
  mode: "pid" | "onoff" | "manual"
  heaterPower: number
  coolerSpeed: number
  activeNode: number
}

const API_BASE = process.env.NEXT_PUBLIC_API_BASE || "http://192.168.1.39";

export default function TemperatureControlDashboard() {
  const [temperatureData, setTemperatureData] = useState<TemperatureData[]>([])
  const [systemStatus, setSystemStatus] = useState<SystemStatus>({
    isConnected: true,
    mode: "pid",
    heaterPower: 0,
    coolerSpeed: 1,
    activeNode: 0,
  })
  const [pidSettings, setPidSettings] = useState({
    setpoint: 30,
    kp: 1.0,
    ki: 0.1,
    kd: 0.05,
  })
  const [autoRefresh, setAutoRefresh] = useState(true)
  const [refreshInterval, setRefreshInterval] = useState(1000)

  // Poll real API state
  useEffect(() => {
    if (!autoRefresh) return
    let cancelled = false
    const coolerLevelFromPct = (pct: number) => (pct >= 80 ? 3 : pct >= 60 ? 2 : pct >= 30 ? 1 : 0)
    const tick = async () => {
      try {
        const res = await fetch(`${API_BASE}/api/state`)
        const s = await res.json()
        if (cancelled) return
        const now = new Date()
        const ts = now.toLocaleTimeString()
        const nodes: number[] = s?.temperatures?.nodes || []
        const room: number = s?.temperatures?.room ?? NaN
        const setpoint: number = s?.setpoint ?? NaN
        const newData: TemperatureData = {
          timestamp: ts,
          sensor1: nodes[0] ?? NaN,
          sensor2: nodes[1] ?? NaN,
          sensor3: nodes[2] ?? NaN,
          sensor4: nodes[3] ?? NaN,
          ambient: room,
          setpoint,
        }
        setTemperatureData((prev) => [...prev.slice(-49), newData])
        setSystemStatus((prev) => ({
          ...prev,
          isConnected: true,
          mode: (s?.mode || 'pid'),
          heaterPower: typeof s?.control_pct === 'number' ? s.control_pct : prev.heaterPower,
          coolerSpeed: coolerLevelFromPct(s?.cooler_percent ?? 0),
          activeNode: Math.max(0, (s?.node ?? 1) - 1),
        }))
      } catch (e) {
        setSystemStatus((prev) => ({ ...prev, isConnected: false }))
      }
    }
    const id = setInterval(tick, Math.max(200, refreshInterval))
    tick()
    return () => { cancelled = true; clearInterval(id) }
  }, [autoRefresh, refreshInterval])

  const currentTemps = temperatureData.length > 0 ? temperatureData[temperatureData.length - 1] : null

  const handleApplySettings = async () => {
    const coolerPct = [0,33,66,100][Math.max(0, Math.min(3, systemStatus.coolerSpeed))]
    try {
      await fetch(`${API_BASE}/api/cooler?percent=${coolerPct}`, { method: 'POST' })
      if (systemStatus.mode === 'pid' || systemStatus.mode === 'onoff') {
        await fetch(`${API_BASE}/api/mode?type=pid`, { method: 'POST' })
        await fetch(`${API_BASE}/api/node?index=${(systemStatus.activeNode ?? 0) + 1}`, { method: 'POST' })
        await fetch(`${API_BASE}/api/setpoint?temp=${pidSettings.setpoint.toFixed(1)}`, { method: 'POST' })
      } else {
        await fetch(`${API_BASE}/api/mode?type=fixed`, { method: 'POST' })
        await fetch(`${API_BASE}/api/fixed?percent=${Math.round(systemStatus.heaterPower)}`, { method: 'POST' })
      }
    } catch (e) {
      console.error('Error applying settings', e)
    }
  }

  const handleRun = async () => {
    try {
      await fetch(`${API_BASE}/api/run`, { method: 'POST' })
    } catch (e) {
      console.error('Error starting RUN', e)
    }
  }

  const handleStop = async () => {
    try {
      await fetch(`${API_BASE}/api/stop`, { method: 'POST' })
      setSystemStatus((prev) => ({ ...prev, heaterPower: 0 }))
    } catch (e) {
      console.error('Error stopping RUN', e)
    }
  }

  return (
    <div className="min-h-screen bg-background p-4 space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-foreground">Temperature Control Dashboard</h1>
          <p className="text-muted-foreground">Cylindrical Metal Bar Heating System - Real-time Monitoring & Control</p>
        </div>
        <div className="flex items-center gap-4">
          <Badge variant={systemStatus.isConnected ? "default" : "destructive"} className="flex items-center gap-2">
            {systemStatus.isConnected ? <CheckCircle className="w-4 h-4" /> : <AlertTriangle className="w-4 h-4" />}
            {systemStatus.isConnected ? "Connected" : "Disconnected"}
          </Badge>
          <Badge variant="secondary" className="flex items-center gap-2">
            <Activity className="w-4 h-4" />
            Mode: {systemStatus.mode.toUpperCase()}
          </Badge>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Real-time Temperature Chart */}
        <Card className="lg:col-span-2">
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Activity className="w-5 h-5 text-primary" />
              Real-time Temperature Monitoring
            </CardTitle>
            <CardDescription>
              Live temperature readings from 4 sensors along the 12cm cylindrical metal bar
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="h-80">
              <ResponsiveContainer width="100%" height="100%">
                <LineChart data={temperatureData}>
                  <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" />
                  <XAxis dataKey="timestamp" stroke="hsl(var(--muted-foreground))" fontSize={12} />
                  <YAxis stroke="hsl(var(--muted-foreground))" fontSize={12} domain={["dataMin - 2", "dataMax + 2"]} />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: "hsl(var(--card))",
                      border: "1px solid hsl(var(--border))",
                      borderRadius: "8px",
                    }}
                  />
                  <Legend />
                  <Line
                    type="monotone"
                    dataKey="sensor1"
                    stroke="hsl(var(--chart-1))"
                    strokeWidth={2}
                    name="Sensor 1 (0cm)"
                    dot={false}
                  />
                  <Line
                    type="monotone"
                    dataKey="sensor2"
                    stroke="hsl(var(--chart-2))"
                    strokeWidth={2}
                    name="Sensor 2 (4cm)"
                    dot={false}
                  />
                  <Line
                    type="monotone"
                    dataKey="sensor3"
                    stroke="hsl(var(--chart-3))"
                    strokeWidth={2}
                    name="Sensor 3 (8cm)"
                    dot={false}
                  />
                  <Line
                    type="monotone"
                    dataKey="sensor4"
                    stroke="hsl(var(--chart-4))"
                    strokeWidth={2}
                    name="Sensor 4 (12cm)"
                    dot={false}
                  />
                  <Line
                    type="monotone"
                    dataKey="ambient"
                    stroke="hsl(var(--chart-5))"
                    strokeWidth={1}
                    strokeDasharray="5 5"
                    name="Ambient"
                    dot={false}
                  />
                  <Line
                    type="monotone"
                    dataKey="setpoint"
                    stroke="hsl(var(--muted-foreground))"
                    strokeWidth={2}
                    strokeDasharray="10 5"
                    name="Setpoint"
                    dot={false}
                  />
                </LineChart>
              </ResponsiveContainer>
            </div>
            <div className="flex items-center gap-4 mt-4">
              <div className="flex items-center gap-2">
                <Switch checked={autoRefresh} onCheckedChange={setAutoRefresh} />
                <Label>Auto-refresh</Label>
              </div>
              <div className="flex items-center gap-2">
                <Label>Interval (ms):</Label>
                <Input
                  type="number"
                  value={refreshInterval}
                  onChange={(e) => setRefreshInterval(Number(e.target.value))}
                  className="w-20"
                  min={200}
                  step={100}
                />
              </div>
            </div>
          </CardContent>
        </Card>

        {/* Current Temperature Readings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Thermometer className="w-5 h-5 text-primary" />
              Current Readings
            </CardTitle>
            <CardDescription>Live sensor values and system status</CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            {currentTemps && (
              <>
                <div className="grid grid-cols-2 gap-3">
                  <div className="text-center p-3 rounded-lg bg-muted">
                    <div className="text-2xl font-bold text-chart-1">{currentTemps.sensor1.toFixed(1)}°C</div>
                    <div className="text-sm text-muted-foreground">Sensor 1 (0cm)</div>
                  </div>
                  <div className="text-center p-3 rounded-lg bg-muted">
                    <div className="text-2xl font-bold text-chart-2">{currentTemps.sensor2.toFixed(1)}°C</div>
                    <div className="text-sm text-muted-foreground">Sensor 2 (4cm)</div>
                  </div>
                  <div className="text-center p-3 rounded-lg bg-muted">
                    <div className="text-2xl font-bold text-chart-3">{currentTemps.sensor3.toFixed(1)}°C</div>
                    <div className="text-sm text-muted-foreground">Sensor 3 (8cm)</div>
                  </div>
                  <div className="text-center p-3 rounded-lg bg-muted">
                    <div className="text-2xl font-bold text-chart-4">{currentTemps.sensor4.toFixed(1)}°C</div>
                    <div className="text-sm text-muted-foreground">Sensor 4 (12cm)</div>
                  </div>
                </div>

                <Separator />

                <div className="space-y-3">
                  <div className="flex justify-between items-center">
                    <span className="text-sm text-muted-foreground">Ambient Temperature</span>
                    <span className="font-semibold text-chart-5">{currentTemps.ambient.toFixed(1)}°C</span>
                  </div>
                  <div className="flex justify-between items-center">
                    <span className="text-sm text-muted-foreground">Target Temperature</span>
                    <span className="font-semibold">{currentTemps.setpoint.toFixed(1)}°C</span>
                  </div>
                  <div className="flex justify-between items-center">
                    <span className="text-sm text-muted-foreground flex items-center gap-1">
                      <Zap className="w-4 h-4" />
                      Heater Power
                    </span>
                    <span className="font-semibold">{systemStatus.heaterPower.toFixed(0)}%</span>
                  </div>
                  <div className="flex justify-between items-center">
                    <span className="text-sm text-muted-foreground flex items-center gap-1">
                      <Fan className="w-4 h-4" />
                      Cooler Speed
                    </span>
                    <span className="font-semibold">Level {systemStatus.coolerSpeed}</span>
                  </div>
                </div>
              </>
            )}
          </CardContent>
        </Card>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Control Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Settings className="w-5 h-5 text-primary" />
              Control Configuration
            </CardTitle>
            <CardDescription>System control mode and parameters</CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="space-y-2">
              <Label>Control Mode</Label>
              <Select
                value={systemStatus.mode}
                onValueChange={(value: "pid" | "onoff" | "manual") =>
                  setSystemStatus((prev) => ({ ...prev, mode: value }))
                }
              >
                <SelectTrigger>
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="pid">PID Control</SelectItem>
                  <SelectItem value="onoff">ON/OFF Control</SelectItem>
                  <SelectItem value="manual">Manual Control</SelectItem>
                </SelectContent>
              </Select>
            </div>

            {systemStatus.mode !== "manual" && (
              <>
                <div className="space-y-2">
                  <Label>Active Sensor Node (0-3)</Label>
                  <Select
                    value={systemStatus.activeNode.toString()}
                    onValueChange={(value) =>
                      setSystemStatus((prev) => ({ ...prev, activeNode: Number.parseInt(value) }))
                    }
                  >
                    <SelectTrigger>
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      <SelectItem value="0">Sensor 1 (0cm)</SelectItem>
                      <SelectItem value="1">Sensor 2 (4cm)</SelectItem>
                      <SelectItem value="2">Sensor 3 (8cm)</SelectItem>
                      <SelectItem value="3">Sensor 4 (12cm)</SelectItem>
                    </SelectContent>
                  </Select>
                </div>

                <div className="space-y-2">
                  <Label>Target Temperature (°C)</Label>
                  <Input
                    type="number"
                    value={pidSettings.setpoint}
                    onChange={(e) =>
                      setPidSettings((prev) => ({ ...prev, setpoint: Number.parseFloat(e.target.value) }))
                    }
                    min={10}
                    max={80}
                    step={0.1}
                  />
                </div>
              </>
            )}

            <div className="space-y-2">
              <Label>Cooler Speed (0-3)</Label>
              <div className="px-3">
                <Slider
                  value={[systemStatus.coolerSpeed]}
                  onValueChange={(value) => setSystemStatus((prev) => ({ ...prev, coolerSpeed: value[0] }))}
                  max={3}
                  min={0}
                  step={1}
                  className="w-full"
                />
                <div className="flex justify-between text-xs text-muted-foreground mt-1">
                  <span>Off</span>
                  <span>Low</span>
                  <span>Med</span>
                  <span>High</span>
                </div>
              </div>
            </div>

            {systemStatus.mode === "manual" && (
              <div className="space-y-2">
                <Label>Manual Heater Power (%)</Label>
                <div className="px-3">
                  <Slider
                    value={[systemStatus.heaterPower]}
                    onValueChange={(value) => setSystemStatus((prev) => ({ ...prev, heaterPower: value[0] }))}
                    max={100}
                    min={0}
                    step={1}
                    className="w-full"
                  />
                  <div className="flex justify-between text-xs text-muted-foreground mt-1">
                    <span>0%</span>
                    <span>50%</span>
                    <span>100%</span>
                  </div>
                </div>
              </div>
            )}

            <div className="flex gap-2">
              <Button onClick={handleApplySettings} className="flex-1">
                Apply Configuration
              </Button>
              <Button onClick={handleRun} className="bg-green-600 hover:bg-green-700">
                RUN
              </Button>
              <Button onClick={handleStop} variant="destructive">
                STOP
              </Button>
            </div>
          </CardContent>
        </Card>

        {/* PID Parameters */}
        {systemStatus.mode === "pid" && (
          <Card>
            <CardHeader>
              <CardTitle>PID Parameters</CardTitle>
              <CardDescription>Fine-tune the PID controller settings</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="space-y-2">
                <Label>Proportional Gain (Kp)</Label>
                <Input
                  type="number"
                  value={pidSettings.kp}
                  onChange={(e) => setPidSettings((prev) => ({ ...prev, kp: Number.parseFloat(e.target.value) }))}
                  step={0.1}
                  min={0}
                />
              </div>

              <div className="space-y-2">
                <Label>Integral Gain (Ki)</Label>
                <Input
                  type="number"
                  value={pidSettings.ki}
                  onChange={(e) => setPidSettings((prev) => ({ ...prev, ki: Number.parseFloat(e.target.value) }))}
                  step={0.01}
                  min={0}
                />
              </div>

              <div className="space-y-2">
                <Label>Derivative Gain (Kd)</Label>
                <Input
                  type="number"
                  value={pidSettings.kd}
                  onChange={(e) => setPidSettings((prev) => ({ ...prev, kd: Number.parseFloat(e.target.value) }))}
                  step={0.01}
                  min={0}
                />
              </div>

              <div className="pt-4 space-y-2">
                <div className="text-sm text-muted-foreground">Current PID Output</div>
                <div className="text-2xl font-bold text-primary">{systemStatus.heaterPower.toFixed(1)}%</div>
              </div>
            </CardContent>
          </Card>
        )}

        {/* System Status */}
        {systemStatus.mode !== "pid" && (
          <Card>
            <CardHeader>
              <CardTitle>System Status</CardTitle>
              <CardDescription>Current system state and diagnostics</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="grid grid-cols-2 gap-4">
                <div className="text-center p-4 rounded-lg bg-muted">
                  <div className="text-lg font-semibold text-primary">
                    {systemStatus.isConnected ? "Online" : "Offline"}
                  </div>
                  <div className="text-sm text-muted-foreground">ESP32 Status</div>
                </div>
                <div className="text-center p-4 rounded-lg bg-muted">
                  <div className="text-lg font-semibold text-secondary">{systemStatus.mode.toUpperCase()}</div>
                  <div className="text-sm text-muted-foreground">Control Mode</div>
                </div>
              </div>

              <Separator />

              <div className="space-y-3">
                <div className="flex justify-between">
                  <span className="text-sm text-muted-foreground">Last Update</span>
                  <span className="text-sm font-medium">{new Date().toLocaleTimeString()}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-sm text-muted-foreground">Data Points</span>
                  <span className="text-sm font-medium">{temperatureData.length}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-sm text-muted-foreground">Refresh Rate</span>
                  <span className="text-sm font-medium">{refreshInterval}ms</span>
                </div>
              </div>
            </CardContent>
          </Card>
        )}
      </div>
    </div>
  )
}
