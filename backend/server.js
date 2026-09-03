const express = require("express");
const http = require("http");
const { Server } = require("socket.io");
const sqlite3 = require("sqlite3").verbose();
const cors = require("cors");
const path = require("path");

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
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);
});

// API: Save Target from Setter UI & Send to Shell
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

      // Broadcast target update to frontend dashboard via Socket.IO
      io.emit("targetUpdated", {
        latitude,
        longitude,
        altitude,
        fuze_mode,
        hob,
      });

      res.json({
        success: true,
        message: "Target configuration transmitted successfully to shell!",
      });
    },
  );
});

// API: Receive Telemetry from Hardware/ESP32 (Updated to support both full fields and state/angle payloads)
app.post("/api/telemetry", (req, res) => {
  const pitch =
    req.body.pitch !== undefined ? req.body.pitch : req.body.angle || 0.0;
  const yaw = req.body.yaw !== undefined ? req.body.yaw : 0.0;
  const g_force = req.body.g_force !== undefined ? req.body.g_force : 1.0;
  const status =
    req.body.status !== undefined
      ? req.body.status
      : req.body.state || "UNKNOWN";
  console.log("Telemetry saved:", req.body);

  db.run(
    `INSERT INTO telemetry (pitch, yaw, g_force, status) VALUES (?, ?, ?, ?)`,
    [pitch, yaw, g_force, status],
    function (err) {
      if (err) return res.status(500).json({ error: err.message });

      // Push live telemetry to frontend clients via Socket.IO
      io.emit("liveTelemetry", { pitch, yaw, g_force, status });
      res.json({
        success: true,
        message: "Telemetry packet recorded by Ground Control Station",
      });
    },
  );
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
