#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Wi-Fi Credentials (update these with your local network details)
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Backend Server URL (replace with your computer's local IP address)
const char* serverUrl = "http://192.168.1.50:3000/api/telemetry";

// State Machine Definitions
enum FlightState { STANDBY, LAUNCHED, GUIDED_FLIGHT, DETONATED };
FlightState currentState = STANDBY;

// Pin Definitions for 4 Canard Servos
const int CANARD1_PIN = 18; // Top
const int CANARD2_PIN = 19; // Bottom
const int CANARD3_PIN = 21; // Left
const int CANARD4_PIN = 22; // Right

const int DETONATION_LED_PIN = 23;
const int BUZZER_PIN = 4;

// Flight Dynamics & Telemetry Variables
float pitchAngle = 2.5;
float yawAngle = 0.5;
float gForce = 4.2;

float c1Deflection = 15.0;
float c2Deflection = -15.0;
float c3Deflection = 12.5;
float c4Deflection = -12.5;

// PID Variables
float targetAngle = 0.0;
float error = 0.0, previousError = 0.0, integral = 0.0;
float Kp = 1.2, Ki = 0.05, Kd = 0.1;

// Timing Control
unsigned long lastControlLoopTime = 0;
const int controlIntervalMs = 10; // 100 Hz High-Speed Control Loop

unsigned long lastTelemetryTime = 0;
const int telemetryIntervalMs = 1000; // 1 Hz Network Transmission Rate

// Forward Declarations
void runFlightStateMachine(float dt);
bool checkLaunchTrigger();
bool checkDetonationCondition();
void readIMUSensors();
float computePID(float target, float current, float dt);
void actuate4Canards(float pidOutput);
void sendTelemetryToBackend();
String getStatusString(FlightState state);

void setup() {
    Serial.begin(115200);
    
    // Configure hardware pins
    pinMode(DETONATION_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    
    // Initialize PWM channels for 4 Canard Servos (50Hz Servo Standard)
    ledcSetup(0, 50, 16); // Channel 0 -> Canard 1
    ledcSetup(1, 50, 16); // Channel 1 -> Canard 2
    ledcSetup(2, 50, 16); // Channel 2 -> Canard 3
    ledcSetup(3, 50, 16); // Channel 3 -> Canard 4

    ledcAttachPin(CANARD1_PIN, 0);
    ledcAttachPin(CANARD2_PIN, 1);
    ledcAttachPin(CANARD3_PIN, 2);
    ledcAttachPin(CANARD4_PIN, 3);

    // Connect to Wi-Fi
    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected successfully!");
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
    
    Serial.println("SIH 26098: ESP32 4-Canard Flight Controller Initialized.");
}

void loop() {
    unsigned long currentTime = millis();
    
    // Enforce strict 100 Hz control loop timing (every 10 ms)
    if (currentTime - lastControlLoopTime >= controlIntervalMs) {
        float dt = (currentTime - lastControlLoopTime) / 1000.0;
        lastControlLoopTime = currentTime;

        runFlightStateMachine(dt);
    }

    // Rate-limited Telemetry Transmission (every 1 second)
    if (currentTime - lastTelemetryTime >= telemetryIntervalMs) {
        lastTelemetryTime = currentTime;
        sendTelemetryToBackend();
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
            Serial.println("STATE: GUIDED_FLIGHT (4 Canards Active)");
            break;

        case GUIDED_FLIGHT:
            readIMUSensors(); 
            {
                float correction = computePID(targetAngle, pitchAngle, dt);
                actuate4Canards(correction);
            }

            if (checkDetonationCondition()) {
                currentState = DETONATED;
                Serial.println("STATE: DETONATED");
            }
            break;

        case DETONATED:
            digitalWrite(DETONATION_LED_PIN, HIGH);
            digitalWrite(BUZZER_PIN, HIGH);
            
            // Neutralize all 4 Canards (1.5ms pulse ~ 4915 count on 16-bit)
            ledcWrite(0, 4915);
            ledcWrite(1, 4915);
            ledcWrite(2, 4915);
            ledcWrite(3, 4915);
            break;
    }
}

bool checkLaunchTrigger() {
    return Serial.available() && Serial.read() == 'l';
}

bool checkDetonationCondition() {
    return false; 
}

void readIMUSensors() {
    // Read MPU6050 angles & accelerations here
    pitchAngle = 2.5; 
    yawAngle = 0.5;
    gForce = 4.2;
}

float computePID(float target, float current, float dt) {
    error = target - current;
    integral += error * dt;
    float derivative = (error - previousError) / dt;
    float output = (Kp * error) + (Ki * integral) + (Kd * derivative);
    previousError = error;
    return output;
}

void actuate4Canards(float pidOutput) {
    // Example differential deflection assignment
    c1Deflection = pidOutput;
    c2Deflection = -pidOutput;
    c3Deflection = pidOutput * 0.8;
    c4Deflection = -pidOutput * 0.8;

    // Convert angles (-45° to +45°) to 16-bit PWM duties (1ms to 2ms pulse width)
    int duty1 = map((int)c1Deflection, -45, 45, 1638, 7864);
    int duty2 = map((int)c2Deflection, -45, 45, 1638, 7864);
    int duty3 = map((int)c3Deflection, -45, 45, 1638, 7864);
    int duty4 = map((int)c4Deflection, -45, 45, 1638, 7864);

    ledcWrite(0, duty1);
    ledcWrite(1, duty2);
    ledcWrite(2, duty3);
    ledcWrite(3, duty4);
}

String getStatusString(FlightState state) {
    switch(state) {
        case STANDBY: return "STANDBY";
        case LAUNCHED: return "LAUNCHED";
        case GUIDED_FLIGHT: return "GUIDED_FLIGHT";
        case DETONATED: return "DETONATED";
        default: return "UNKNOWN";
    }
}

void sendTelemetryToBackend() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");

        // Structured JSON payload matching backend database schema
        String payload = "{";
        payload += "\"pitch\":" + String(pitchAngle, 2) + ",";
        payload += "\"yaw\":" + String(yawAngle, 2) + ",";
        payload += "\"g_force\":" + String(gForce, 2) + ",";
        payload += "\"status\":\"" + getStatusString(currentState) + "\",";
        payload += "\"canard_1\":" + String(c1Deflection, 2) + ",";
        payload += "\"canard_2\":" + String(c2Deflection, 2) + ",";
        payload += "\"canard_3\":" + String(c3Deflection, 2) + ",";
        payload += "\"canard_4\":" + String(c4Deflection, 2);
        payload += "}";

        int httpResponseCode = http.POST(payload);
        
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Telemetry packet sent. Server ACK: " + response);
        } else {
            Serial.println("Telemetry HTTP error: " + String(httpResponseCode));
        }
        
        http.end();
    } else {
        Serial.println("Wi-Fi disconnected. Reconnecting...");
        WiFi.begin(ssid, password);
    }
}