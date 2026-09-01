# SIH 2026: Low-Cost Precision Guidance & Smart Fuze System (PS: SIH26098)

Project repository for converting standard 155 mm artillery shells into precision-guided munitions (CEP <= 30m).

## Folder Structure

- `backend/` - Node.js + Express + SQLite Ground Control & Setter API
- `frontend/` - Ground Station Dashboard UI
- `simulation/` - Python 6-DoF Trajectory & CEP Reduction Simulator
- `firmware/` - ESP32 C++ Microcontroller Code (GNC Loop & Canard Servos)

## Getting Started

1. Clone the repo: `git clone https://github.com/Geyachakrini/sih-artillery-guidance.git`
2. Run simulation: `cd simulation && python sim.py`
3. Start backend: `cd backend && npm install && npm start`
