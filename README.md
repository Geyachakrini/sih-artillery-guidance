# SIH 2026: Low-Cost Precision Guidance & Smart Fuze System (PS: SIH26098)

A precision guidance conversion kit for standard 155 mm artillery shells targeting a Circular Error Probable (CEP) <= 30 m. The system features a gun-hardened DSP/FPGA flight computer running a deterministic RTOS, 4-canard aerodynamic trajectory correction, multi-mode electronic fuze control, and a real-time Ground Control Station (GCS).

---

## System Architecture

- **Flight Computer**: Radiation-tolerant, potting-encapsulated DSP/FPGA running a 500 Hz RTOS filter loop (NavIC/GPS + IMU + Magnetometer fusion).
- **GNC Sensors**: Tactical gun-hardened MEMS IMU (>30,000 g survival), dual-constellation NavIC + GPS receiver, and tri-axial magnetometer.
- **Pre-Launch Interface**: Nose-tip inductive coil setter (NATO STANAG 4369 / Epi-Setter standard) flashed via UART/RS-422 serial from the GCS.
- **Ground Control Station**: Node.js middleware bridge translating high-speed serial streams from the flight controller into real-time Socket.IO events for dashboard analytics and SQLite logging.

---

## Folder Structure

- `backend/` - Node.js + Express + SQLite + Socket.IO Ground Control Server & Serial Bridge (`serialport`)
- `frontend/` - Real-time GCS Dashboard UI (Pitch/Yaw dynamics, 4-Canard deflection HUD, Fuze programming)
- `simulation/` - Python 6-DoF Trajectory & CEP Reduction Simulator

---

## Serial Data Protocol (GCS <-> DSP Interface)

The GCS communicates with the flight computer over UART/RS-422 at **115200 baud rate** using structured JSON strings ending with a newline (`\n`).

### 1. Downlink Telemetry (DSP -> GCS)

```json
{
  "pitch": 12.5,
  "yaw": -2.3,
  "g_force": 4.1,
  "status": "GUIDED_FLIGHT",
  "canard_1": 10.0,
  "canard_2": -10.0,
  "canard_3": 5.0,
  "canard_4": -5.0
}
```
