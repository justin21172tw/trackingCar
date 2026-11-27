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
  serialReadState: document.querySelector("#serialReadState"),
  serialDevice: document.querySelector("#serialDevice"),
  serialReceiveCount: document.querySelector("#serialReceiveCount"),
  serialTelemetryCount: document.querySelector("#serialTelemetryCount"),
  serialLastLine: document.querySelector("#serialLastLine"),
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
let currentScanRateHz = null;
let lastScanReceivedAt = null;
let lastDeviceTimestamp = null;
let serialConnectedAt = null;
let staleNoticeShown = false;
let serialPort = null;
let serialReader = null;
let serialReadTask = null;
let serialLineBuffer = "";
let diagnosticsCount = 0;
let serialBytesSeen = 0;
let serialLinesSeen = 0;
let telemetryLinesSeen = 0;
let acceptedTelemetryRecords = 0;
let serialLastByteAt = null;
let serialLastLinePreview = "";
let serialDeviceDescription = "—";
let serialReadError = null;

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
    return { label: "No data", kind: "waiting", diagnostic: "No vehicle telemetry received" };
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

function clearTelemetry() {
  for (const state of sensorStates.values()) {
    state.status = "waiting";
    state.reason = null;
    state.distanceCm = null;
    state.history = [];
    state.lastUpdatedAt = null;
    state.lastFrequencyHz = null;
  }
  currentScanRateHz = null;
  lastScanReceivedAt = null;
  lastDeviceTimestamp = null;
  staleNoticeShown = false;
  diagnosticsCount = 0;
  elements.scanNumber.textContent = "—";
  elements.scanAge.textContent = "No vehicle data";
  elements.validCount.innerHTML = `— <small>/ ${SENSOR_CONFIGS.length}</small>`;
  elements.scanDuration.innerHTML = "— <small>ms</small>";
  elements.scanRate.innerHTML = "— <small>Hz</small>";
  elements.logicState.textContent = "not provided";
  render();
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

function receiveTelemetry(record) {
  if (!validateTelemetry(record)) {
    diagnosticsCount += 1;
    setMessage(`Ignored malformed or unsupported telemetry record (${diagnosticsCount}).`, "warning");
    return;
  }
  acceptedTelemetryRecords += 1;

  if (lastDeviceTimestamp !== null && record.timestamp_ms < lastDeviceTimestamp) {
    clearTelemetry();
    setMessage("Device timestamp restarted; cleared old chart history.");
  }
  lastDeviceTimestamp = record.timestamp_ms;

  const now = performance.now();
  if (lastScanReceivedAt !== null && now > lastScanReceivedAt) {
    currentScanRateHz = 1000 / (now - lastScanReceivedAt);
  }
  lastScanReceivedAt = now;
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
  elements.scanAge.textContent = "just updated";
  elements.scanRate.innerHTML = currentScanRateHz === null
    ? `— <small>Hz</small>`
    : `${currentScanRateHz.toFixed(1)} <small>Hz</small>`;
  const logic = record.logic;
  const targetSummary = logic &&
    typeof logic.target_sensor_id === "string" &&
    Number.isFinite(logic.target_direction_deg) &&
    Number.isFinite(logic.target_distance_cm)
    ? ` · ${logic.target_sensor_id} · ${logic.target_direction_deg.toFixed(1)}° · ${logic.target_distance_cm.toFixed(1)} cm`
    : "";
  elements.logicState.textContent = `${logic?.state ?? "not provided"}${targetSummary}`;
  setConnection("serial", "USB receiving telemetry");
  setMessage(
    `Scan ${record.scan} received from USB serial.${diagnosticsCount ? ` ${diagnosticsCount} malformed record(s) ignored.` : ""}`,
  );
  staleNoticeShown = false;
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
  if (!trimmed) return;
  serialLinesSeen += 1;
  serialLastLinePreview = trimmed.slice(0, 180);
  if (!trimmed.startsWith(TELEMETRY_PREFIX)) return;
  telemetryLinesSeen += 1;

  try {
    receiveTelemetry(JSON.parse(trimmed.slice(TELEMETRY_PREFIX.length)));
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
      serialBytesSeen += value.byteLength;
      serialLastByteAt = performance.now();
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
      clearTelemetry();
      serialConnectedAt = null;
      serialReadError = error.message;
      setConnection("error", "USB read failed");
      setMessage(`Serial read failed; no vehicle data is available: ${error.message}`, "error");
    }
  } finally {
    reader.releaseLock();
    if (serialReader === reader) serialReader = null;
  }
}

async function connectSerial() {
  if (!("serial" in navigator)) {
    setMessage("Web Serial is unavailable in this browser. No vehicle data is available.", "warning");
    return;
  }
  if (!window.isSecureContext) {
    setMessage("Web Serial requires a secure page. Open this page from localhost or HTTPS.", "warning");
    return;
  }
  try {
    const port = await navigator.serial.requestPort();
    await port.open({ baudRate: SERIAL_BAUD_RATE });
    if (!port.readable) {
      await port.close();
      throw new Error("The selected serial port has no readable stream.");
    }
    serialPort = port;
    serialLineBuffer = "";
    serialBytesSeen = 0;
    serialLinesSeen = 0;
    telemetryLinesSeen = 0;
    acceptedTelemetryRecords = 0;
    serialLastByteAt = null;
    serialLastLinePreview = "";
    serialReadError = null;
    const deviceInfo = port.getInfo();
    const vendorId = Number.isInteger(deviceInfo.usbVendorId)
      ? `VID 0x${deviceInfo.usbVendorId.toString(16).padStart(4, "0").toUpperCase()}`
      : null;
    const productId = Number.isInteger(deviceInfo.usbProductId)
      ? `PID 0x${deviceInfo.usbProductId.toString(16).padStart(4, "0").toUpperCase()}`
      : null;
    serialDeviceDescription = [vendorId, productId].filter(Boolean).join(" · ")
      || "USB vendor/product IDs not exposed";
    serialReader = port.readable.getReader();
    clearTelemetry();
    serialConnectedAt = performance.now();
    serialReadTask = readSerialStream(serialReader);
    elements.connectSerial.hidden = true;
    elements.disconnectSerial.hidden = false;
    setConnection("serial", "USB connected · 115200 baud");
    setMessage("USB connected. Waiting for real vehicle telemetry; no readings received yet.");
  } catch (error) {
    setConnection("error", "USB connection failed");
    setMessage(`Could not connect to serial device: ${error.message}`, "error");
  }
}

async function disconnectSerial() {
  const port = serialPort;
  serialPort = null;
  serialConnectedAt = null;
  let closeError = null;
  if (serialReader) {
    try {
      await serialReader.cancel();
    } catch (error) {
      closeError = `Could not stop serial reader cleanly: ${error.message}`;
    }
  }
  if (serialReadTask) {
    await serialReadTask;
    serialReadTask = null;
  }
  if (port) {
    try {
      await port.close();
    } catch (error) {
      closeError = `Could not close USB serial port: ${error.message}`;
    }
  }
  clearTelemetry();
  setConnection(closeError ? "error" : "disconnected", closeError ? "USB close failed" : "No vehicle connected");
  elements.connectSerial.hidden = false;
  elements.disconnectSerial.hidden = true;
  setMessage(
    closeError
      ? `${closeError}. No vehicle data is displayed.`
      : "Vehicle disconnected. No data is available until real telemetry arrives.",
    closeError ? "error" : "info",
  );
}

function updateClock() {
  elements.serialDevice.textContent = serialDeviceDescription;
  elements.serialReceiveCount.textContent = `${serialBytesSeen} bytes · ${serialLinesSeen} lines`;
  elements.serialTelemetryCount.textContent =
    `${telemetryLinesSeen} prefixed · ${acceptedTelemetryRecords} accepted · ${diagnosticsCount} rejected`;
  elements.serialLastLine.textContent = serialLastLinePreview || (
    serialLineBuffer.trim()
      ? `${serialLineBuffer.trim().slice(0, 180)} (waiting for line ending)`
      : "—"
  );
  if (!serialPort) {
    elements.serialReadState.textContent = "Not connected";
  } else if (serialReadError) {
    elements.serialReadState.textContent = `Read failed: ${serialReadError}`;
  } else if (serialLastByteAt === null) {
    const waitSeconds = Math.max(0, (performance.now() - serialConnectedAt) / 1000);
    elements.serialReadState.textContent =
      `Reader active · waiting ${waitSeconds.toFixed(1)} s · 0 bytes`;
  } else {
    const idleSeconds = Math.max(0, (performance.now() - serialLastByteAt) / 1000);
    elements.serialReadState.textContent = idleSeconds > 1.5
      ? `Reader active · no bytes for ${idleSeconds.toFixed(1)} s`
      : "Reader active · receiving bytes";
  }

  if (lastScanReceivedAt === null) {
    if (serialPort && serialConnectedAt !== null &&
        performance.now() - serialConnectedAt > 3000 && !staleNoticeShown) {
      let label;
      let message;
      if (telemetryLinesSeen > 0) {
        label = "USB open · invalid telemetry";
        message = `Received ${telemetryLinesSeen} @TELEMETRY line(s), but none was valid. Check the firmware output format.`;
      } else if (serialLinesSeen > 0) {
        label = "USB open · text only";
        message = `Received ${serialLinesSeen} serial text line(s), but none starts with @TELEMETRY. Upload the updated telemetry firmware.`;
      } else if (serialBytesSeen > 0) {
        label = "USB open · incomplete line";
        message = `Received ${serialBytesSeen} serial byte(s), but no complete text line. Check the serial format.`;
      } else {
        label = "USB open · no bytes";
        message = "No serial bytes received. Close Serial Monitor/other COM readers, then check the selected port and ESP32.";
      }
      setConnection("error", label);
      setMessage(message, "warning");
      staleNoticeShown = true;
    }
    return;
  }
  const ageSeconds = (performance.now() - lastScanReceivedAt) / 1000;
  elements.scanAge.textContent = ageSeconds > 2.5
    ? `${ageSeconds.toFixed(1)} s · stale`
    : `${ageSeconds.toFixed(1)} s since last scan`;
  if (ageSeconds > 2.5 && !staleNoticeShown) {
    setConnection("error", "USB telemetry stale");
    setMessage("No new vehicle telemetry for 2.5 seconds. Displayed readings are stale.", "warning");
    staleNoticeShown = true;
  }
  render();
}

buildSensorView();
render();
elements.connectSerial.addEventListener("click", connectSerial);
elements.disconnectSerial.addEventListener("click", disconnectSerial);
window.addEventListener("resize", render);

if (!("serial" in navigator)) {
  elements.connectSerial.disabled = true;
  elements.connectSerial.title = "Web Serial is not supported in this browser.";
  setMessage("No vehicle connected. Web Serial is unavailable in this browser, so no readings can be displayed.", "warning");
} else if (!window.isSecureContext) {
  setMessage("No vehicle data. Web Serial requires a secure page; open this page from localhost or HTTPS.", "warning");
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
