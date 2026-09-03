import requests
import time
import random

url = "http://localhost:3000/api/telemetry"

print("Virtual ESP32 started with live simulation. Press Ctrl+C to stop.")

while True:
    # Simulate slight sensor jitter/changes during flight
    payload = {
        "pitch": round(2.5 + random.uniform(-0.8, 0.8), 2),
        "yaw": round(0.5 + random.uniform(-0.3, 0.3), 2),
        "g_force": round(4.2 + random.uniform(-0.2, 0.2), 2),
        "status": "GUIDED_FLIGHT"
    }
    
    try:
        response = requests.post(url, json=payload)
        print(f"Sent: {payload} | Server Response: {response.json()}")
    except requests.exceptions.ConnectionError:
        print("Connection failed. Is your Node.js server running?")
    
    time.sleep(1)