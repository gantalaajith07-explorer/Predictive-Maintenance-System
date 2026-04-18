import json
import math
import os
import socket
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


QNX_HOST = os.environ.get("QNX_HOST", "127.0.0.1")
QNX_PORT = int(os.environ.get("QNX_PORT", "8888"))
DASHBOARD_PORT = int(os.environ.get("DASHBOARD_PORT", "8050"))
MAX_POINTS = 80


class DashboardStore:
    def __init__(self):
        self.lock = threading.Lock()
        self.timeline = deque(maxlen=MAX_POINTS)
        self.vibration = deque(maxlen=MAX_POINTS)
        self.temperature = deque(maxlen=MAX_POINTS)
        self.accel = deque(maxlen=MAX_POINTS)
        self.health = deque(maxlen=MAX_POINTS)
        self.rul = deque(maxlen=MAX_POINTS)
        self.alerts = deque(maxlen=12)
        self.state = "NORMAL"
        self.sample = 0
        self.confidence = 0
        self.ipc_round_trips = 0
        self.pulses = 0
        self.timeouts = 0
        self.watchdog = {"sensor": 0, "ipc": 0, "alert": 0, "timer": 0}
        self.restarts = {"sensor": 0, "ipc": 0, "alert": 0, "timer": 0}
        self.connected = False
        self.source = "demo"

    def apply_packet(self, packet, source):
        now = time.strftime("%H:%M:%S")
        state = packet.get("state", "NORMAL")

        with self.lock:
            self.timeline.append(now)
            self.vibration.append(float(packet.get("vib", 0)))
            self.temperature.append(float(packet.get("temp", 0)))
            self.accel.append(float(packet.get("accel", 0)))
            self.health.append(float(packet.get("health", 100)))
            self.rul.append(float(packet.get("rul", 500)))
            self.state = state
            self.sample = int(packet.get("sample", self.sample + 1))
            self.confidence = float(packet.get("conf", packet.get("confidence", 0)))
            self.ipc_round_trips = int(packet.get("ipc", packet.get("ipc_round_trips", 0)))
            self.pulses = int(packet.get("pulses", 0))
            self.timeouts = int(packet.get("timeouts", 0))
            self.watchdog = {
                "sensor": int(packet.get("wd_sensor", packet.get("wd_s", 1))),
                "ipc": int(packet.get("wd_ipc", packet.get("wd_p", 1))),
                "alert": int(packet.get("wd_alert", packet.get("wd_a", 1))),
                "timer": int(packet.get("wd_timer", packet.get("wd_l", 1))),
            }
            self.restarts = {
                "sensor": int(packet.get("rs_sensor", packet.get("rs_s", 0))),
                "ipc": int(packet.get("rs_ipc", packet.get("rs_p", 0))),
                "alert": int(packet.get("rs_alert", packet.get("rs_a", 0))),
                "timer": int(packet.get("rs_timer", packet.get("rs_l", 0))),
            }
            self.connected = source == "qnx"
            self.source = source

            if state != "NORMAL":
                self.alerts.appendleft(
                    f"{now}  {state}  health={self.health[-1]:.1f}%  rul={self.rul[-1]:.1f}h"
                )

    def snapshot(self):
        with self.lock:
            return {
                "timeline": list(self.timeline),
                "vibration": list(self.vibration),
                "temperature": list(self.temperature),
                "accel": list(self.accel),
                "health": list(self.health),
                "rul": list(self.rul),
                "state": self.state,
                "sample": self.sample,
                "confidence": self.confidence,
                "ipc_round_trips": self.ipc_round_trips,
                "pulses": self.pulses,
                "timeouts": self.timeouts,
                "watchdog": dict(self.watchdog),
                "restarts": dict(self.restarts),
                "connected": self.connected,
                "source": self.source,
                "alerts": list(self.alerts),
            }


store = DashboardStore()


def qnx_receiver():
    while True:
        try:
            with socket.create_connection((QNX_HOST, QNX_PORT), timeout=3) as sock:
                print(f"[dashboard] connected to QNX stream {QNX_HOST}:{QNX_PORT}")
                buffer = ""
                while True:
                    chunk = sock.recv(1024).decode("utf-8", errors="ignore")
                    if not chunk:
                        break
                    buffer += chunk
                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        if line.strip():
                            store.apply_packet(json.loads(line), "qnx")
        except Exception:
            with store.lock:
                store.connected = False
                store.source = "demo"
            time.sleep(2)


def demo_feed():
    sample = 0
    while True:
        snap = store.snapshot()
        if not snap["connected"]:
            sample += 1
            damage = min(1.0, (sample % 120) / 100.0)
            vibration = 0.8 + damage * 4.0 + math.sin(sample / 4.0) * 0.25
            temperature = 35.0 + damage * 34.0 + math.sin(sample / 8.0) * 1.5
            accel = 1.0 + damage * 4.2 + abs(math.sin(sample / 5.0)) * 0.4
            health = max(0.0, 100.0 - (vibration * 11.0 + (temperature - 30.0) * 0.55 + accel * 5.0))
            state = "CRITICAL" if health < 35 else "WARNING" if health < 60 else "NORMAL"
            store.apply_packet(
                {
                    "sample": sample,
                    "vib": vibration,
                    "temp": temperature,
                    "accel": accel,
                    "health": health,
                    "rul": max(0.0, health * 4.8),
                    "conf": min(99.0, 55.0 + abs(70.0 - health)),
                    "state": state,
                    "ipc": sample,
                    "pulses": sample * 2,
                    "timeouts": sample // 45,
                    "wd_sensor": 1,
                    "wd_ipc": 1,
                    "wd_alert": 1,
                    "wd_timer": 1,
                },
                "demo",
            )
        time.sleep(0.75)


PAGE = r"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>PMS QNX Dashboard</title>
  <style>
    body { margin: 0; font-family: Arial, sans-serif; background: #101418; color: #edf2f7; }
    header { padding: 18px 22px; border-bottom: 1px solid #2b333d; display: flex; justify-content: space-between; gap: 16px; align-items: center; }
    h1 { margin: 0; font-size: 24px; }
    main { padding: 18px; display: grid; gap: 16px; }
    .cards { display: grid; grid-template-columns: repeat(6, 1fr); gap: 12px; }
    .card, .panel { background: #171d23; border: 1px solid #2b333d; border-radius: 8px; padding: 14px; }
    .label { color: #9aa7b2; font-size: 12px; text-transform: uppercase; }
    .value { font-size: 24px; font-weight: 700; margin-top: 6px; }
    .charts3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 16px; }
    .lower { display: grid; grid-template-columns: 2fr 1fr 1fr; gap: 16px; }
    .bottom { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
    canvas { width: 100%; height: 210px; background: #11161b; border-radius: 8px; }
    .gauge { height: 210px; display: grid; place-items: center; font-size: 34px; font-weight: 700; border-radius: 8px; background: radial-gradient(circle, #202932 0 42%, #11161b 43%); }
    .alerts { min-height: 180px; line-height: 1.7; color: #ffd166; }
    .watch-row { display: flex; justify-content: space-between; padding: 6px 0; border-bottom: 1px solid #26303a; }
    .ok { color: #2dd36f; } .warn { color: #ffd166; } .bad { color: #ff4d4f; }
    @media (max-width: 900px) { .cards, .charts3, .lower, .bottom { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
  <header>
    <div>
      <h1>Predictive Maintenance System</h1>
      <div class="label">QNX RTOS Concepts Dashboard</div>
    </div>
    <div id="link" class="value warn">WAITING</div>
  </header>
  <main>
    <section class="cards">
      <div class="card"><div class="label">State</div><div id="state" class="value">NORMAL</div></div>
      <div class="card"><div class="label">Health</div><div id="health" class="value">100%</div></div>
      <div class="card"><div class="label">RUL</div><div id="rul" class="value">500h</div></div>
      <div class="card"><div class="label">Samples</div><div id="sample" class="value">0</div></div>
      <div class="card"><div class="label">IPC Trips</div><div id="ipc" class="value">0</div></div>
      <div class="card"><div class="label">Timer Pulses</div><div id="pulses" class="value">0</div></div>
    </section>
    <section class="charts3">
      <div class="panel">
        <div class="label">Vibration (g)</div>
        <canvas id="vibChart" width="480" height="210"></canvas>
      </div>
      <div class="panel">
        <div class="label">Temperature (C)</div>
        <canvas id="tempChart" width="480" height="210"></canvas>
      </div>
      <div class="panel">
        <div class="label">Acceleration RMS (g)</div>
        <canvas id="accelChart" width="480" height="210"></canvas>
      </div>
    </section>
    <section class="lower">
      <div class="panel">
        <div class="label">Machine Health Trend</div>
        <canvas id="healthChart" width="700" height="210"></canvas>
      </div>
      <div class="panel">
        <div class="label">RUL Gauge</div>
        <div id="rulGauge" class="gauge">500h</div>
      </div>
      <div class="panel">
        <div class="label">Health Gauge</div>
        <div id="healthGauge" class="gauge">100%</div>
      </div>
    </section>
    <section class="bottom">
      <div class="panel">
        <div class="label">Watchdog Monitor</div>
        <div id="watchdog"></div>
      </div>
      <div class="panel">
        <div class="label">Alert Log</div>
        <div id="alerts" class="alerts">No alerts yet</div>
      </div>
    </section>
  </main>
<script>
function drawChart(id, values, color, maxValue) {
  const chart = document.getElementById(id);
  const ctx = chart.getContext("2d");
  ctx.clearRect(0, 0, chart.width, chart.height);
  ctx.strokeStyle = "#27313a";
  for (let i = 1; i < 5; i++) {
    ctx.beginPath();
    ctx.moveTo(0, i * chart.height / 5);
    ctx.lineTo(chart.width, i * chart.height / 5);
    ctx.stroke();
  }
  if (!values.length) return;
  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  values.forEach((v, i) => {
    const x = (i / Math.max(1, values.length - 1)) * chart.width;
    const y = chart.height - Math.max(0, Math.min(1, v / maxValue)) * chart.height;
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.stroke();
}

function render(data) {
  const state = data.state || "NORMAL";
  document.getElementById("state").textContent = state;
  document.getElementById("state").className = "value " + (state === "CRITICAL" ? "bad" : state === "WARNING" ? "warn" : "ok");
  document.getElementById("health").textContent = (data.health.at(-1) || 0).toFixed(1) + "%";
  document.getElementById("rul").textContent = (data.rul.at(-1) || 0).toFixed(1) + "h";
  document.getElementById("sample").textContent = data.sample;
  document.getElementById("ipc").textContent = data.ipc_round_trips;
  document.getElementById("pulses").textContent = data.pulses;
  document.getElementById("link").textContent = data.connected ? "QNX LIVE" : "DEMO MODE";
  document.getElementById("link").className = "value " + (data.connected ? "ok" : "warn");
  document.getElementById("rulGauge").textContent = (data.rul.at(-1) || 0).toFixed(1) + "h";
  document.getElementById("healthGauge").textContent = (data.health.at(-1) || 0).toFixed(1) + "%";
  document.getElementById("alerts").innerHTML = data.alerts.length ? data.alerts.map(a => "<div>" + a + "</div>").join("") : "No alerts yet";
  document.getElementById("watchdog").innerHTML = ["sensor", "ipc", "alert", "timer"].map(k => {
    const ok = data.watchdog[k] ? "ok" : "bad";
    return `<div class="watch-row"><span class="${ok}">● ${k.toUpperCase()}</span><span>Restarts: ${data.restarts[k] || 0}</span></div>`;
  }).join("");

  drawChart("vibChart", data.vibration, "#ffd166", 7);
  drawChart("tempChart", data.temperature, "#ff4d4f", 90);
  drawChart("accelChart", data.accel, "#4dabf7", 7);
  drawChart("healthChart", data.health, "#2dd36f", 100);
}

async function tick() {
  const response = await fetch("/api/snapshot");
  render(await response.json());
}
setInterval(tick, 700);
tick();
</script>
</body>
</html>
"""


class DashboardHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/api/snapshot":
            body = json.dumps(store.snapshot()).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        body = PAGE.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        return


def main():
    threading.Thread(target=qnx_receiver, daemon=True).start()
    threading.Thread(target=demo_feed, daemon=True).start()
    server = ThreadingHTTPServer(("127.0.0.1", DASHBOARD_PORT), DashboardHandler)
    print("=" * 56)
    print(" PMS QNX Dashboard running")
    print(f" Open: http://127.0.0.1:{DASHBOARD_PORT}")
    print(f" QNX stream: {QNX_HOST}:{QNX_PORT}")
    print(" It uses demo data until the QNX app connects.")
    print("=" * 56)
    server.serve_forever()


if __name__ == "__main__":
    main()
