"use strict";

const TELEMETRY_PREFIX = "@TELEMETRY ";
const TELEMETRY_VERSION = 1;
const HISTORY_WINDOW_MS = 3000;
const MAX_DISTANCE_CM = 250;
const SERIAL_BAUD_RATE = 115200;
const SENSOR_CONFIGS = [
  { id: "S0", index: 0, direction: "F", label: "Front", x: 320, y: 58, angle: 0, color: "#315ee8" },
  { id: "S1", index: 1, direction: "FL", label: "Front left", x: 220, y: 112, angle: -42, color: "#8058d8" },
  { id: "S2", index: 2, direction: "FR", label: "Front right", x: 420, y: 112, angle: 42, color: "#16a0a5" },
  { id: "S3", index: 3, direction: "LE", label: "Left", x: 164, y: 250, angle: -90, color: "#d28b24" },
  { id: "S4", index: 4, direction: "RI", label: "Right", x: 476, y: 250, angle: 90, color: "#d35d74" },
  { id: "S5", index: 5, direction: "R", label: "Rear", x: 320, y: 448, angle: 180, color: "#3d8b63" },
  { id: "S6", index: 6, direction: "RL", label: "Rear left", x: 218, y: 390, angle: -138, color: "#6878a6" },
  { id: "S7", index: 7, direction: "RR", label: "Rear right", x: 422, y: 390, angle: 138, color: "#a16f45" },
];

const elements = {
  connectionState: document.querySelector("#connectionState"),
  connectionLabel: document.querySelector("#connectionLabel"),
  connectSerial: document.querySelector("#connectSerial"),
  disconnectSerial: document.querySelector("#disconnectSerial"),
  startSimulation: document.querySelector("#startSimulation"),
  stopSimulation: document.querySelector("#stopSimulation"),
  scanNumber: document.querySelector("#scanNumber"),
  scanAge: document.querySelector("#scanAge"),
  validCount: document.querySelector("#validCount"),
  scanDuration: document.querySelector("#scanDuration"),
  scanRate: document.querySelector("#scanRate"),
  sensorLayer: document.querySelector("#sensorLayer"),
  sensorGrid: document.querySelector("#sensorGrid"),
  selectedId: document.querySelector("#selectedId"),
  selectedDirection: document.querySelector("#selectedDirection"),
  selectedDistance: document.querySelector("#selectedDistance"),
  selectedStatus: document.querySelector("#selectedStatus"),
  selectedIndex: document.querySelector("#selectedIndex"),
  selectedFrequency: document.querySelector("#selectedFrequency"),
  selectedUpdated: document.querySelector("#selectedUpdated"),
  selectedDiagnostic: document.querySelector("#selectedDiagnostic"),
  logicState: document.querySelector("#logicState"),
  pageMessage: document.querySelector("#pageMessage"),
};

const sensorStates = new Map(
  SENSOR_CONFIGS.map((sensor) => [
    sensor.id,
    {
      status: "waiting",
      reason: null,
      distanceCm: null,
      history: [],
      lastUpdatedAt: null,
      lastFrequencyHz: null,
    },
  ]),
);

let selectedSensorId = SENSOR_CONFIGS[0].id;
let currentScanNumber = null;
let currentScanRateHz = null;
let lastScanReceivedAt = null;
let lastDeviceTimestamp = null;
let simulationTimer = null;
let simulationScanNumber = 0;
let serialPort = null;
let serialReader = null;
let serialReadTask = null;
let serialLineBuffer = "";
let diagnosticsCount = 0;

function createSvgElement(tagName, attributes = {}) {
  const element = document.createElementNS("http://www.w3.org/2000/svg", tagName);
  for (const [name, value] of Object.entries(attributes)) {
    element.setAttribute(name, String(value));
  }
  return element;
}

function sensorHealth(sensorId, now = performance.now()) {
  const state = sensorStates.get(sensorId);
  if (!state || state.status === "waiting") {
    return { label: "Waiting", kind: "waiting", diagnostic: "No samples yet" };
  }
  if (state.lastUpdatedAt !== null && now - state.lastUpdatedAt > 2500) {
    return { label: "Stale", kind: "neutral", diagnostic: "No recent update" };
  }
  if (state.status === "timeout") {
    return { label: "Timeout", kind: "error", diagnostic: "Echo pulse not received before timeout" };
  }
  if (state.status === "invalid") {
    return {
      label: "Invalid",
      kind: "error",
      diagnostic: state.reason === "out_of_range" ? "Outside configured distance range" : "Invalid sensor result",
    };
  }

  const recent = state.history.filter(
    (sample) => sample.distanceCm !== null && now - sample.timeMs <= 2200,
  );
  if (recent.length >= 5) {
    const values = recent.slice(-5).map((sample) => sample.distanceCm);
    const spread = Math.max(...values) - Math.min(...values);
    const baseline = values.reduce((sum, value) => sum + value, 0) / values.length;
    if (spread > Math.max(15, baseline * 0.4)) {
      return { label: "Jump", kind: "warning", diagnostic: "Large change across recent samples" };
    }
  }

  const latest = state.history.filter(
    (sample) => sample.distanceCm !== null && now - sample.timeMs <= HISTORY_WINDOW_MS,
  );
  if (latest.length >= 5 && latest[latest.length - 1].timeMs - latest[0].timeMs >= 2000) {
    const values = latest.map((sample) => sample.distanceCm);
    if (Math.max(...values) - Math.min(...values) < 0.05) {
      return { label: "Possible freeze", kind: "warning", diagnostic: "Distance unchanged for 2+ seconds" };
    }
  }

  return { label: "Valid", kind: "valid", diagnostic: "Within configured range" };
}

function statusClass(kind) {
  if (kind === "valid") return "status-valid";
  if (kind === "warning") return "status-warning";
  if (kind === "error") return "status-error";
  if (kind === "waiting") return "status-waiting";
  return "status-neutral";
}

function displayDistance(distanceCm) {
  return Number.isFinite(distanceCm) ? distanceCm.toFixed(1) : "—";
}

function buildSensorView() {
  for (const sensor of SENSOR_CONFIGS) {
    const radians = (sensor.angle * Math.PI) / 180;
    const beam = createSvgElement("line", {
      x1: sensor.x,
      y1: sensor.y,
      x2: sensor.x + Math.sin(radians) * 28,
      y2: sensor.y - Math.cos(radians) * 28,
      class: "sensor-beam",
      id: `${sensor.id}-beam`,
      style: `--sensor-color:${sensor.color}`,
    });
    elements.sensorLayer.append(beam);

    const marker = createSvgElement("g", {
      class: "sensor-hit is-waiting",
      id: `${sensor.id}-marker`,
      role: "button",
      tabindex: "0",
      "aria-label": `${sensor.id}, ${sensor.label}`,
      "data-sensor-id": sensor.id,
      style: `--sensor-color:${sensor.color}`,
    });
    marker.append(
      createSvgElement("circle", {
        cx: sensor.x,
        cy: sensor.y,
        r: 9,
        class: "sensor-circle",
      }),
      createSvgElement("circle", {
        cx: sensor.x,
        cy: sensor.y,
        r: 3,
        class: "sensor-center",
      }),
    );
    const labelX = sensor.x + (sensor.x < 250 ? -15 : sensor.x > 390 ? 15 : 0);
    const labelY = sensor.y + (sensor.y < 130 ? -15 : sensor.y > 360 ? 20 : -14);
    marker.append(
      createSvgElement("text", {
        x: labelX,
        y: labelY,
        "text-anchor": sensor.x < 250 ? "end" : sensor.x > 390 ? "start" : "middle",
        class: "sensor-marker-label",
      }),
    );
    marker.querySelector("text").textContent = sensor.id;
    marker.addEventListener("click", () => selectSensor(sensor.id));
    marker.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        selectSensor(sensor.id);
      }
    });
    elements.sensorLayer.append(marker);

    const card = document.createElement("article");
    card.className = "sensor-card";
    card.id = `${sensor.id}-card`;
    card.tabIndex = 0;
    card.setAttribute("role", "button");
    card.setAttribute("aria-label", `Select ${sensor.id} ${sensor.label} sensor`);
    card.style.setProperty("--sensor-color", sensor.color);
    card.innerHTML = `
      <div class="sensor-card-header">
        <div class="sensor-name-wrap">
          <i class="sensor-color" aria-hidden="true"></i>
          <h3>${sensor.id} <span class="sensor-card-direction">${sensor.direction} · ${sensor.label}</span></h3>
        </div>
      </div>
      <div class="sensor-card-value-row">
        <strong class="sensor-card-value" id="${sensor.id}-value">— <small>cm</small></strong>
        <span class="status-badge status-waiting" id="${sensor.id}-status">Waiting</span>
      </div>
      <canvas class="sensor-chart" id="${sensor.id}-chart" aria-label="${sensor.id} distance history, last 3 seconds"></canvas>
      <div class="chart-caption"><span>−3 s</span><span>0</span></div>
    `;
    card.addEventListener("click", () => selectSensor(sensor.id));
    card.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        selectSensor(sensor.id);
      }
    });
    elements.sensorGrid.append(card);
  }
}

function selectSensor(sensorId) {
  if (!sensorStates.has(sensorId)) return;
  selectedSensorId = sensorId;
  render();
}

function setConnection(state, label) {
  elements.connectionState.dataset.state = state;
  elements.connectionLabel.textContent = label;
}

function setMessage(message, kind = "info") {
  elements.pageMessage.textContent = message;
  elements.pageMessage.dataset.kind = kind;
}

function validateTelemetry(record) {
  if (!(
    record !== null &&
    typeof record === "object" &&
    record.v === TELEMETRY_VERSION &&
    record.type === "scan" &&
    Number.isSafeInteger(record.timestamp_ms) &&
    record.timestamp_ms >= 0 &&
    Number.isSafeInteger(record.scan) &&
    record.scan >= 0 &&
    Number.isFinite(record.scan_duration_ms) &&
    record.scan_duration_ms >= 0 &&
    Number.isInteger(record.valid_count) &&
    record.valid_count >= 0 &&
    record.valid_count <= SENSOR_CONFIGS.length &&
    Array.isArray(record.sensors) &&
    record.sensors.length === SENSOR_CONFIGS.length
  )) return false;

  const seen = new Set();
  for (const item of record.sensors) {
    const config = SENSOR_CONFIGS.find((sensor) => sensor.id === item?.id);
    if (
      !config ||
      seen.has(item.id) ||
      item.direction !== config.direction ||
      !["valid", "timeout", "invalid"].includes(item.status) ||
      (item.status === "valid" &&
        (!Number.isFinite(item.distance_cm) ||
          item.distance_cm < 0 ||
          item.distance_cm > MAX_DISTANCE_CM)) ||
      (item.status !== "valid" && item.distance_cm !== null) ||
      (item.reason !== null && typeof item.reason !== "string")
    ) return false;
    seen.add(item.id);
  }
  return (
    seen.size === SENSOR_CONFIGS.length &&
    record.valid_count === record.sensors.filter((item) => item.status === "valid").length
  );
}

function receiveTelemetry(record, source) {
  if (!validateTelemetry(record)) {
    diagnosticsCount += 1;
    setMessage(`Ignored malformed or unsupported telemetry record (${diagnosticsCount}).`, "warning");
    return;
  }

  if (lastDeviceTimestamp !== null && record.timestamp_ms < lastDeviceTimestamp) {
    for (const state of sensorStates.values()) {
      state.history = [];
      state.lastUpdatedAt = null;
      state.lastFrequencyHz = null;
      state.status = "waiting";
      state.distanceCm = null;
    }
    setMessage("Device timestamp restarted; cleared old chart history.");
  }
  lastDeviceTimestamp = record.timestamp_ms;

  const now = performance.now();
  if (lastScanReceivedAt !== null && now > lastScanReceivedAt) {
    currentScanRateHz = 1000 / (now - lastScanReceivedAt);
  }
  lastScanReceivedAt = now;
  currentScanNumber = record.scan;

  for (const item of record.sensors) {
    if (!item || typeof item.id !== "string" || !sensorStates.has(item.id)) continue;
    const state = sensorStates.get(item.id);
    const previousUpdate = state.lastUpdatedAt;
    const valid = item.status === "valid" && Number.isFinite(item.distance_cm);
    const normalizedStatus = valid
      ? "valid"
      : item.status === "timeout"
        ? "timeout"
        : "invalid";
    const distanceCm = valid ? item.distance_cm : null;

    if (previousUpdate !== null && now > previousUpdate) {
      state.lastFrequencyHz = 1000 / (now - previousUpdate);
    }
    state.status = normalizedStatus;
    state.reason = typeof item.reason === "string" ? item.reason : null;
    state.distanceCm = distanceCm;
    state.lastUpdatedAt = now;
    state.history.push({ timeMs: now, distanceCm });
    state.history = state.history.filter(
      (sample) => now - sample.timeMs <= HISTORY_WINDOW_MS,
    );
  }

  const validCount = Number.isFinite(record.valid_count)
    ? record.valid_count
    : record.sensors.filter((item) => item?.status === "valid").length;
  elements.validCount.innerHTML = `${validCount} <small>/ ${SENSOR_CONFIGS.length}</small>`;
  elements.scanDuration.innerHTML = `${Math.round(record.scan_duration_ms)} <small>ms</small>`;
  elements.scanNumber.textContent = String(record.scan);
  elements.scanAge.textContent = `${source} · just updated`;
  elements.scanRate.innerHTML = currentScanRateHz === null
    ? `— <small>Hz</small>`
    : `${currentScanRateHz.toFixed(1)} <small>Hz</small>`;
  elements.logicState.textContent = record.logic?.state ?? "not provided";
  setMessage(
    `Scan ${record.scan} received from ${source.toLowerCase()}. ${diagnosticsCount ? `${diagnosticsCount} record(s) ignored.` : ""}`.trim(),
  );
  render();
}

function renderSensorMarkers(now) {
  for (const sensor of SENSOR_CONFIGS) {
    const state = sensorStates.get(sensor.id);
    const health = sensorHealth(sensor.id, now);
    const marker = document.querySelector(`#${sensor.id}-marker`);
    const beam = document.querySelector(`#${sensor.id}-beam`);
    const value = Number.isFinite(state.distanceCm) ? state.distanceCm : 0;
    const length = state.status === "valid"
      ? 16 + (Math.min(value, MAX_DISTANCE_CM) / MAX_DISTANCE_CM) * 78
      : 25;
    const radians = (sensor.angle * Math.PI) / 180;

    beam.setAttribute("x2", String(sensor.x + Math.sin(radians) * length));
    beam.setAttribute("y2", String(sensor.y - Math.cos(radians) * length));
    beam.style.opacity = state.status === "valid" ? "0.68" : "0.18";
    marker.classList.toggle("is-selected", selectedSensorId === sensor.id);
    marker.classList.toggle("is-warning", health.kind === "warning" || health.kind === "error");
    marker.classList.toggle("is-stale", health.kind === "neutral");
    marker.classList.toggle("is-waiting", health.kind === "waiting");
    marker.setAttribute(
      "aria-label",
      `${sensor.id}, ${sensor.label}, ${displayDistance(state.distanceCm)} cm, ${health.label}`,
    );

    const card = document.querySelector(`#${sensor.id}-card`);
    const status = document.querySelector(`#${sensor.id}-status`);
    card.classList.toggle("is-selected", selectedSensorId === sensor.id);
    document.querySelector(`#${sensor.id}-value`).innerHTML =
      `${displayDistance(state.distanceCm)} <small>cm</small>`;
    status.textContent = health.label;
    status.className = `status-badge ${statusClass(health.kind)}`;
    drawChart(sensor, state, now);
  }
}

function drawChart(sensor, state, now) {
  const canvas = document.querySelector(`#${sensor.id}-chart`);
  const bounds = canvas.getBoundingClientRect();
  if (bounds.width === 0 || bounds.height === 0) return;
  const pixelRatio = window.devicePixelRatio || 1;
  canvas.width = Math.round(bounds.width * pixelRatio);
  canvas.height = Math.round(bounds.height * pixelRatio);
  const context = canvas.getContext("2d");
  context.scale(pixelRatio, pixelRatio);
  const width = bounds.width;
  const height = bounds.height;
  const left = 25;
  const right = width - 4;
  const top = 7;
  const bottom = height - 13;

  context.clearRect(0, 0, width, height);
  context.font = "9px system-ui, sans-serif";
  context.textBaseline = "middle";
  context.fillStyle = "#97a2b3";
  context.strokeStyle = "#edf0f5";
  context.lineWidth = 1;

  for (const tick of [0, 0.5, 1]) {
    const y = top + (bottom - top) * tick;
    context.beginPath();
    context.moveTo(left, y);
    context.lineTo(right, y);
    context.stroke();
    context.textAlign = "right";
    context.fillText(`${Math.round(MAX_DISTANCE_CM * (1 - tick))}`, left - 5, y);
  }

  const points = state.history
    .filter((sample) => now - sample.timeMs <= HISTORY_WINDOW_MS)
    .map((sample) => ({
      x: right - ((now - sample.timeMs) / HISTORY_WINDOW_MS) * (right - left),
      y: sample.distanceCm === null
        ? null
        : bottom - (Math.min(sample.distanceCm, MAX_DISTANCE_CM) / MAX_DISTANCE_CM) * (bottom - top),
    }));

  context.strokeStyle = sensor.color;
  context.lineWidth = 1.8;
  context.lineJoin = "round";
  context.lineCap = "round";
  context.beginPath();
  let drawing = false;
  for (const point of points) {
    if (point.y === null) {
      drawing = false;
      continue;
    }
    if (drawing) context.lineTo(point.x, point.y);
    else {
      context.moveTo(point.x, point.y);
      drawing = true;
    }
  }
  context.stroke();

  for (const point of points) {
    if (point.y === null || point.x < left || point.x > right) continue;
    context.beginPath();
    context.arc(point.x, point.y, 2, 0, Math.PI * 2);
    context.fillStyle = sensor.color;
    context.fill();
  }
}

function renderSelectedSensor(now) {
  const sensor = SENSOR_CONFIGS.find((item) => item.id === selectedSensorId);
  const state = sensorStates.get(selectedSensorId);
  const health = sensorHealth(selectedSensorId, now);
  elements.selectedId.textContent = sensor.id;
  elements.selectedDirection.textContent = `${sensor.direction} · ${sensor.label}`;
  elements.selectedIndex.textContent = String(sensor.index);
  elements.selectedDistance.textContent = displayDistance(state.distanceCm);
  elements.selectedStatus.textContent = health.label;
  elements.selectedStatus.className = `status-badge ${statusClass(health.kind)}`;
  elements.selectedFrequency.textContent = state.lastFrequencyHz === null
    ? "—"
    : `${state.lastFrequencyHz.toFixed(1)} Hz`;
  elements.selectedUpdated.textContent = state.lastUpdatedAt === null
    ? "—"
    : `${Math.max(0, (now - state.lastUpdatedAt) / 1000).toFixed(1)} s ago`;
  elements.selectedDiagnostic.textContent = health.diagnostic;
}

function render() {
  const now = performance.now();
  renderSensorMarkers(now);
  renderSelectedSensor(now);
}

function processSerialLine(line) {
  const trimmed = line.trim();
  if (!trimmed.startsWith(TELEMETRY_PREFIX)) return;

  try {
    receiveTelemetry(JSON.parse(trimmed.slice(TELEMETRY_PREFIX.length)), "USB serial");
  } catch {
    diagnosticsCount += 1;
    setMessage(`Could not parse telemetry JSON (${diagnosticsCount} malformed record(s)).`, "warning");
  }
}

async function readSerialStream(reader) {
  const decoder = new TextDecoder();
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      serialLineBuffer += decoder.decode(value, { stream: true });
      const lines = serialLineBuffer.split(/\r?\n/);
      serialLineBuffer = lines.pop() ?? "";
      for (const line of lines) processSerialLine(line);
    }
    serialLineBuffer += decoder.decode();
    if (serialLineBuffer.trim()) processSerialLine(serialLineBuffer);
    serialLineBuffer = "";
  } catch (error) {
    if (serialPort) {
      setConnection("error", "USB read failed");
      setMessage(`Serial read failed: ${error.message}`, "error");
    }
  } finally {
    reader.releaseLock();
    if (serialReader === reader) serialReader = null;
  }
}

async function connectSerial() {
  if (!("serial" in navigator)) {
    setMessage("Web Serial is unavailable in this browser. Use a supported Chromium browser or start simulation.", "warning");
    return;
  }
  if (!window.isSecureContext) {
    setMessage("Web Serial requires a secure page. Open this page from localhost or HTTPS.", "warning");
    return;
  }
  if (simulationTimer !== null) stopSimulation();

  try {
    const port = await navigator.serial.requestPort();
    await port.open({ baudRate: SERIAL_BAUD_RATE });
    if (!port.readable) {
      await port.close();
      throw new Error("The selected serial port has no readable stream.");
    }
    serialPort = port;
    serialLineBuffer = "";
    serialReader = port.readable.getReader();
    serialReadTask = readSerialStream(serialReader);
    elements.connectSerial.hidden = true;
    elements.disconnectSerial.hidden = false;
    elements.startSimulation.disabled = true;
    setConnection("serial", "USB connected · 115200 baud");
    setMessage("USB connected. Waiting for the next @TELEMETRY record.");
  } catch (error) {
    setConnection("error", "USB connection failed");
    setMessage(`Could not connect to serial device: ${error.message}`, "error");
  }
}

async function disconnectSerial() {
  const port = serialPort;
  serialPort = null;
  if (serialReader) {
    try {
      await serialReader.cancel();
    } catch (error) {
      setMessage(`Could not stop serial reader cleanly: ${error.message}`, "warning");
    }
  }
  if (serialReadTask) {
    await serialReadTask;
    serialReadTask = null;
  }
  if (port) {
    try {
      await port.close();
      setMessage("USB serial disconnected.");
    } catch (error) {
      setMessage(`Could not close USB serial port: ${error.message}`, "error");
    }
  }
  setConnection("disconnected", "Not connected");
  elements.connectSerial.hidden = false;
  elements.disconnectSerial.hidden = true;
  elements.startSimulation.disabled = false;
}

function mockTelemetry() {
  simulationScanNumber += 1;
  const elapsed = performance.now();
  const sensors = SENSOR_CONFIGS.map((sensor) => {
    const wobble = Math.sin(elapsed / 520 + sensor.index * 0.9) * 7;
    let status = "valid";
    let reason = null;
    let distance = 48 + sensor.index * 15 + wobble;
    if (simulationScanNumber % 9 === 0 && sensor.index === 4) {
      status = "timeout";
      reason = "timeout";
      distance = null;
    } else if (simulationScanNumber % 13 === 0 && sensor.index === 1) {
      status = "invalid";
      reason = "out_of_range";
      distance = null;
    }
    return {
      id: sensor.id,
      direction: sensor.direction,
      distance_cm: status === "valid" ? Number(distance.toFixed(1)) : null,
      status,
      reason,
    };
  });
  return {
    v: TELEMETRY_VERSION,
    type: "scan",
    timestamp_ms: Math.round(elapsed),
    scan: simulationScanNumber,
    scan_duration_ms: 480,
    valid_count: sensors.filter((sensor) => sensor.status === "valid").length,
    logic: { state: null, target_direction_deg: null },
    sensors,
  };
}

function startSimulation() {
  if (simulationTimer !== null) return;
  if (serialPort) {
    setMessage("Disconnect USB serial before starting simulation.", "warning");
    return;
  }
  simulationScanNumber = 0;
  setConnection("simulation", "Simulation mode");
  elements.startSimulation.disabled = true;
  elements.stopSimulation.disabled = false;
  receiveTelemetry(mockTelemetry(), "Simulation");
  simulationTimer = window.setInterval(
    () => receiveTelemetry(mockTelemetry(), "Simulation"),
    480,
  );
}

function stopSimulation() {
  if (simulationTimer !== null) {
    window.clearInterval(simulationTimer);
    simulationTimer = null;
  }
  elements.startSimulation.disabled = false;
  elements.stopSimulation.disabled = true;
  if (!serialPort) setConnection("disconnected", "Not connected");
  setMessage("Simulation stopped. Existing readings remain visible.");
}

function updateClock() {
  if (lastScanReceivedAt === null) return;
  const ageSeconds = (performance.now() - lastScanReceivedAt) / 1000;
  elements.scanAge.textContent = `${ageSeconds.toFixed(1)} s since last scan`;
  render();
}

buildSensorView();
render();
elements.connectSerial.addEventListener("click", connectSerial);
elements.disconnectSerial.addEventListener("click", disconnectSerial);
elements.startSimulation.addEventListener("click", startSimulation);
elements.stopSimulation.addEventListener("click", stopSimulation);
window.addEventListener("resize", render);

if (!("serial" in navigator)) {
  elements.connectSerial.disabled = true;
  elements.connectSerial.title = "Web Serial is not supported in this browser.";
  setMessage("Web Serial is unavailable in this browser. Use a supported Chromium browser or start simulation.", "warning");
} else if (!window.isSecureContext) {
  setMessage("Web Serial requires a secure page. Open this page from localhost or HTTPS.", "warning");
}

if ("serial" in navigator) {
  navigator.serial.addEventListener("disconnect", (event) => {
    if (event.target === serialPort) {
      setConnection("error", "USB device disconnected");
      setMessage("The USB serial device was disconnected.", "error");
      disconnectSerial();
    }
  });
}

window.setInterval(updateClock, 250);
