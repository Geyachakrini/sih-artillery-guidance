const express = require("express");
const http = require("http");
const { Server } = require("socket.io");
const sqlite3 = require("sqlite3").verbose();
const cors = require("cors");
const path = require("path");
const { SerialPort } = require("serialport");
const { ReadlineParser } = require("@serialport/parser-readline");

const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  cors: { origin: "*" },
});

app.use(express.json());
app.use(cors());
app.use(express.static(path.join(__dirname, "../frontend")));

// Initialize SQLite Database
const db = new sqlite3.Database("./database.sqlite", (err) => {
  if (err) console.error("Database connection error:", err.message);
  else console.log("Connected to SQLite database.");
});

// Create telemetry and target tables
db.serialize(() => {
  db.run(`CREATE TABLE IF NOT EXISTS targets (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        latitude REAL,
        longitude REAL,
        altitude REAL,
        fuze_mode TEXT,
        hob REAL,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);

  db.run(`CREATE TABLE IF NOT EXISTS telemetry (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        pitch REAL,
        yaw REAL,
        g_force REAL,
        status TEXT,
        canard_1 REAL,
        canard_2 REAL,
        canard_3 REAL,
        canard_4 REAL,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);
});

// Serial Port Connection Setup for DSP/FPGA Board
const portName = process.env.SERIAL_PORT || "COM3"; // Use 'COMx' on Windows or '/dev/ttyUSB0' on Linux
let dspPort = null;

try {
  dspPort = new SerialPort({ path: portName, baudRate: 115200 });
  const parser = dspPort.pipe(new ReadlineParser({ delimiter: "\n" }));

  dspPort.on("open", () => {
    console.log(`[Serial] Connected to DSP/FPGA board on ${portName}`);
  });

  // Read Telemetry stream from DSP over Serial
  parser.on("data", (line) => {
    const cleanLine = line.trim();
    if (!cleanLine) return;

    try {
      const data = JSON.parse(cleanLine);

      const pitch = data.pitch !== undefined ? data.pitch : 0.0;
      const yaw = data.yaw !== undefined ? data.yaw : 0.0;
      const g_force = data.g_force !== undefined ? data.g_force : 1.0;
      const status = data.status || "UNKNOWN";
      const c1 = data.canard_1 || 0.0;
      const c2 = data.canard_2 || 0.0;
      const c3 = data.canard_3 || 0.0;
      const c4 = data.canard_4 || 0.0;

      // Save to SQLite
      db.run(
        `INSERT INTO telemetry (pitch, yaw, g_force, status, canard_1, canard_2, canard_3, canard_4) VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
        [pitch, yaw, g_force, status, c1, c2, c3, c4],
        (err) => {
          if (err) console.error("[DB Error]:", err.message);
        },
      );

      // Broadcast live stream to GCS frontend
      io.emit("liveTelemetry", {
        pitch,
        yaw,
        g_force,
        status,
        canard_1: c1,
        canard_2: c2,
        canard_3: c3,
        canard_4: c4,
      });
    } catch (err) {
      // Handles standard debug print statements sent by DSP firmware
      console.log(`[DSP Debug Output]: ${cleanLine}`);
    }
  });

  dspPort.on("error", (err) => {
    console.error(`[Serial Port Error]: ${err.message}`);
  });
} catch (e) {
  console.warn(
    `[Serial Warning] Could not connect to ${portName}. Server running in standalone HTTP mode.`,
  );
}

// API: Save Target from Setter UI & Send to Shell via Serial
app.post("/api/set-target", (req, res) => {
  const { latitude, longitude, altitude, fuze_mode, hob } = req.body;

  db.run(
    `INSERT INTO targets (latitude, longitude, altitude, fuze_mode, hob) VALUES (?, ?, ?, ?, ?)`,
    [latitude, longitude, altitude, fuze_mode, hob],
    function (err) {
      if (err) {
        return res.status(500).json({ error: err.message });
      }

      console.log(
        `Target Set -> Mode: ${fuze_mode}, Lat: ${latitude}, Lon: ${longitude}`,
      );

      const targetPayload = { latitude, longitude, altitude, fuze_mode, hob };

      // Broadcast target update to frontend dashboard via Socket.IO
      io.emit("targetUpdated", targetPayload);

      // Flash target parameters to DSP over Serial
      if (dspPort && dspPort.isOpen) {
        const serialMsg =
          JSON.stringify({ type: "SET_TARGET", ...targetPayload }) + "\n";
        dspPort.write(serialMsg, (writeErr) => {
          if (writeErr) {
            console.error("[Serial Write Error]:", writeErr.message);
          } else {
            console.log(
              "[Serial] Target payload transmitted to DSP successfully.",
            );
          }
        });
      }

      res.json({
        success: true,
        message: "Target configuration transmitted successfully to hardware!",
      });
    },
  );
});

// API: HTTP Telemetry Endpoint (Fallback for testing or HTTP clients)
app.post("/api/telemetry", (req, res) => {
  const pitch =
    req.body.pitch !== undefined ? req.body.pitch : req.body.angle || 0.0;
  const yaw = req.body.yaw !== undefined ? req.body.yaw : 0.0;
  const g_force = req.body.g_force !== undefined ? req.body.g_force : 1.0;
  const status =
    req.body.status !== undefined
      ? req.body.status
      : req.body.state || "UNKNOWN";
  const c1 = req.body.canard_1 || 0.0;
  const c2 = req.body.canard_2 || 0.0;
  const c3 = req.body.canard_3 || 0.0;
  const c4 = req.body.canard_4 || 0.0;

  db.run(
    `INSERT INTO telemetry (pitch, yaw, g_force, status, canard_1, canard_2, canard_3, canard_4) VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
    [pitch, yaw, g_force, status, c1, c2, c3, c4],
    function (err) {
      if (err) return res.status(500).json({ error: err.message });

      io.emit("liveTelemetry", {
        pitch,
        yaw,
        g_force,
        status,
        canard_1: c1,
        canard_2: c2,
        canard_3: c3,
        canard_4: c4,
      });

      res.json({
        success: true,
        message: "Telemetry recorded successfully",
      });
    },
  );
});

// API: Get Latest Target (Used by hardware polling if needed)
app.get("/api/get-target", (req, res) => {
  db.get("SELECT * FROM targets ORDER BY id DESC LIMIT 1", [], (err, row) => {
    if (err) return res.status(500).json({ error: err.message });
    res.json(row || {});
  });
});

// Socket.IO Connection Handler
io.on("connection", (socket) => {
  console.log("Ground Control Client connected:", socket.id);
  socket.on("disconnect", () => {
    console.log("Client disconnected:", socket.id);
  });
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  console.log(`Ground Control Server running on http://localhost:${PORT}`);
});
