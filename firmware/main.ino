#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Wi-Fi Credentials (update these with your local network details)
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Backend Server URL (replace with your computer's local IP address if running on another machine)
const char* serverUrl = "http://192.168.1.50:3000/api/telemetry";

// State Machine Definitions
enum FlightState { STANDBY, LAUNCHED, GUIDED_FLIGHT, DETONATED };
FlightState currentState = STANDBY;

// Pin Definitions for Benchtop Demo
const int PITCH_SERVO_PIN = 18;
const int YAW_SERVO_PIN = 19;
const int DETONATION_LED_PIN = 23;
const int BUZZER_PIN = 4;

// PID Variables
float targetAngle = 0.0;
float currentAngle = 0.0;
float error = 0.0, previousError = 0.0, integral = 0.0;
float Kp = 1.2, Ki = 0.05, Kd = 0.1;
unsigned long lastLoopTime = 0;
const int intervalMs = 10; // 100 Hz high-speed control loop

void setup() {
    Serial.begin(115200);
    
    // Configure hardware pins
    pinMode(DETONATION_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    
    // Initialize PWM channels for high-speed servo response
    ledcSetup(0, 50, 16); // Channel 0, 50Hz, 16-bit
    ledcAttachPin(PITCH_SERVO_PIN, 0);

    // Connect to Wi-Fi
    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected successfully!");
    
    Serial.println("SIH 26098: ESP32 Flight Controller Initialized.");
}

void loop() {
    unsigned long currentTime = millis();
    
    // Enforce strict 100 Hz loop timing (every 10 ms)
    if (currentTime - lastLoopTime >= intervalMs) {
        float dt = (currentTime - lastLoopTime) / 1000.0;
        lastLoopTime = currentTime;

        runFlightStateMachine(dt);
    }
}

void runFlightStateMachine(float dt) {
    switch(currentState) {
        case STANDBY:
            if (checkLaunchTrigger()) {
                currentState = LAUNCHED;
                Serial.println("STATE: LAUNCHED (High G-force detected!)");
            }
            break;

        case LAUNCHED:
            currentState = GUIDED_FLIGHT;
            Serial.println("STATE: GUIDED_FLIGHT (Canards active)");
            break;

        case GUIDED_FLIGHT:
            currentAngle = readIMUAngles(); 
            float correction = computePID(targetAngle, currentAngle, dt);
            actuateWings(correction);
            
            // Send telemetry packet over Wi-Fi to Node.js backend
            sendTelemetryToBackend();

            if (checkDetonationCondition()) {
                currentState = DETONATED;
                Serial.println("STATE: DETONATED");
            }
            break;

        case DETONATED:
            digitalWrite(DETONATION_LED_PIN, HIGH);
            digitalWrite(BUZZER_PIN, HIGH);
            ledcWrite(0, 4915); // Center neutral position
            break;
    }
}

bool checkLaunchTrigger() {
    return Serial.available() && Serial.read() == 'l';
}

bool checkDetonationCondition() {
    return false; 
}

float readIMUAngles() {
    return 2.5; // Dummy drift angle until MPU-6050 physical wiring is done
}

float computePID(float target, float current, float dt) {
    error = target - current;
    integral += error * dt;
    float derivative = (error - previousError) / dt;
    float output = (Kp * error) + (Ki * integral) + (Kd * derivative);
    previousError = error;
    return output;
}

void actuateWings(float pidOutput) {
    int dutyCycle = map((int)pidOutput, -45, 45, 1638, 7864);
    ledcWrite(0, dutyCycle);
}

void sendTelemetryToBackend() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");

        // Format telemetry data into a JSON string payload
        String payload = "{\"state\":\"GUIDED_FLIGHT\",\"angle\":" + String(currentAngle) + "}";

        int httpResponseCode = http.POST(payload);
        
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Telemetry sent successfully. Server response: " + response);
        } else {
            Serial.println("Error sending telemetry. HTTP Error code: " + String(httpResponseCode));
        }
        
        http.end();
    } else {
        Serial.println("Wi-Fi disconnected. Reconnecting...");
        WiFi.begin(ssid, password);
    }
}