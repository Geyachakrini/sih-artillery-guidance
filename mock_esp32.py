import requests
import time
import random

url = "http://localhost:3000/api/telemetry"

print("Virtual ESP32 started with 4-Canard telemetry. Press Ctrl+C to stop.")

while True:
    payload = {
        "pitch": round(2.5 + random.uniform(-0.8, 0.8), 2),
        "yaw": round(0.5 + random.uniform(-0.3, 0.3), 2),
        "g_force": round(4.2 + random.uniform(-0.2, 0.2), 2),
        "status": "GUIDED_FLIGHT",
        # 4 Canard Deflection Angles (in degrees)
        "canard_1": round(15.0 + random.uniform(-1.0, 1.0), 2),
        "canard_2": round(-15.0 + random.uniform(-1.0, 1.0), 2),
        "canard_3": round(12.5 + random.uniform(-1.0, 1.0), 2),
        "canard_4": round(-12.5 + random.uniform(-1.0, 1.0), 2),
    }
    
    try:
        response = requests.post(url, json=payload)
        print(f"Sent 4-Canard Packet | Server Response: {response.json()}")
    except requests.exceptions.ConnectionError:
        print("Connection failed. Is your Node.js server running?")
    
    time.sleep(1)