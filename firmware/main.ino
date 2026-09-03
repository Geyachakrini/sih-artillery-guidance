#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Wi-Fi Credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Backend Server URLs
const char* telemetryUrl = "http://192.168.1.50:3000/api/telemetry";
const char* targetUrl = "http://192.168.1.50:3000/api/get-target";

// State Machine Definitions
enum FlightState { STANDBY, LAUNCHED, GUIDED_FLIGHT, DETONATED };
FlightState currentState = STANDBY;

// Target Fuze Parameters fetched from GCS Backend
String targetFuzeMode = "Impact"; // Options: "Proximity", "Time", "Impact"
float targetFuzeTime = 0.0;       // Airburst time (seconds) OR post-impact delay (ms)

// Hardware Pins
const int CANARD1_PIN = 18;
const int CANARD2_PIN = 19;
const int CANARD3_PIN = 21;
const int CANARD4_PIN = 22;
const int DETONATION_LED_PIN = 23;
const int BUZZER_PIN = 4;

// Flight Dynamics & Telemetry Variables
float pitchAngle = 0.0;
float yawAngle = 0.0;
float gForce = 1.0;

float c1Deflection = 0.0;
float c2Deflection = 0.0;
float c3Deflection = 0.0;
float c4Deflection = 0.0;

// Impact & Timer Tracking
unsigned long launchStartTime = 0;
unsigned long impactTime = 0;
bool impactDetected = false;

// MEMS High-G Threshold for Automotive/Collision Detection
const float MEMS_IMPACT_G_THRESHOLD = 15.0; // Detonation G-spike trigger

// PID Variables
float targetAngle = 0.0;
float error = 0.0, previousError = 0.0, integral = 0.0;
float Kp = 1.2, Ki = 0.05, Kd = 0.1;

// Loop Timing Controls
unsigned long lastControlLoopTime = 0;
const int controlIntervalMs = 10; // 100 Hz Flight Loop

unsigned long lastTelemetryTime = 0;
const int telemetryIntervalMs = 1000; // 1 Hz Telemetry Stream

unsigned long lastTargetFetchTime = 0;
const int targetFetchIntervalMs = 2000; // Poll backend target profile every 2s in STANDBY

// Function Declarations
void runFlightStateMachine(float dt);
void fetchTargetFromBackend();
bool checkLaunchTrigger();
bool checkDetonationCondition();
void readIMUSensors();
float computePID(float target, float current, float dt);
void actuate4Canards(float pidOutput);
void sendTelemetryToBackend();
String getStatusString(FlightState state);

void setup() {
    Serial.begin(115200);
    
    pinMode(DETONATION_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(DETONATION_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Configure PWM channels for 4 Canard Servos (50 Hz standard servo frequency)
    ledcSetup(0, 50, 16);
    ledcSetup(1, 50, 16);
    ledcSetup(2, 50, 16);
    ledcSetup(3, 50, 16);

    ledcAttachPin(CANARD1_PIN, 0);
    ledcAttachPin(CANARD2_PIN, 1);
    ledcAttachPin(CANARD3_PIN, 2);
    ledcAttachPin(CANARD4_PIN, 3);

    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi Connected successfully!");
    Serial.print("ESP32 Local IP: ");
    Serial.println(WiFi.localIP());
}

void loop() {
    unsigned long currentTime = millis();
    
    // 100 Hz High-Speed Control Loop
    if (currentTime - lastControlLoopTime >= controlIntervalMs) {
        float dt = (currentTime - lastControlLoopTime) / 1000.0;
        lastControlLoopTime = currentTime;
        runFlightStateMachine(dt);
    }

    // 1 Hz Telemetry Transmission to Backend
    if (currentTime - lastTelemetryTime >= telemetryIntervalMs) {
        lastTelemetryTime = currentTime;
        sendTelemetryToBackend();
    }
}

void runFlightStateMachine(float dt) {
    switch(currentState) {
        case STANDBY:
            // Continually poll GCS backend for pre-launch target & fuze config
            if (millis() - lastTargetFetchTime >= targetFetchIntervalMs) {
                lastTargetFetchTime = millis();
                fetchTargetFromBackend();
            }

            if (checkLaunchTrigger()) {
                currentState = LAUNCHED;
                launchStartTime = millis();
                Serial.println("STATE TRANSITION: LAUNCHED (Ballistic boost initialized)");
            }
            break;

        case LAUNCHED:
            currentState = GUIDED_FLIGHT;
            Serial.println("STATE TRANSITION: GUIDED_FLIGHT (4 Canards operational)");
            break;

        case GUIDED_FLIGHT:
            readIMUSensors(); 
            {
                float correction = computePID(targetAngle, pitchAngle, dt);
                actuate4Canards(correction);
            }

            // Check dynamic fuze logic (Proximity, Time Delay, or MEMS Impact)
            if (checkDetonationCondition()) {
                currentState = DETONATED;
                Serial.println("STATE TRANSITION: DETONATED (Payload Fired)");
            }
            break;

        case DETONATED:
            digitalWrite(DETONATION_LED_PIN, HIGH);
            digitalWrite(BUZZER_PIN, HIGH);
            
            // Neutralize canards on impact/airburst
            ledcWrite(0, 4915);
            ledcWrite(1, 4915);
            ledcWrite(2, 4915);
            ledcWrite(3, 4915);
            break;
    }
}

void fetchTargetFromBackend() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(targetUrl);
        int httpCode = http.GET();

        if (httpCode == 200) {
            String payload = http.getString();
            
            // Extract fuze_mode
            int modeIndex = payload.indexOf("\"fuze_mode\":\"");
            if (modeIndex != -1) {
                int start = modeIndex + 13;
                int end = payload.indexOf("\"", start);
                targetFuzeMode = payload.substring(start, end);
            }

            // Extract hob (serves as fuze time value)
            int hobIndex = payload.indexOf("\"hob\":");
            if (hobIndex != -1) {
                int start = hobIndex + 6;
                int end = payload.indexOf(",", start);
                if (end == -1) end = payload.indexOf("}", start);
                targetFuzeTime = payload.substring(start, end).toFloat();
            }

            Serial.println("Target Profile Downloaded | Mode: " + targetFuzeMode + " | Time/Val: " + String(targetFuzeTime));
        }
        http.end();
    }
}

bool checkLaunchTrigger() {
    // Send 'l' over serial or trigger high acceleration to launch
    return Serial.available() && Serial.read() == 'l';
}

void readIMUSensors() {
    // TODO: Replace with Adafruit_MPU6050 library calls
    pitchAngle = 2.5; 
    yawAngle = 0.5;

    // Simulate high G-spike detection test via Serial input 'i'
    if (Serial.available() && Serial.peek() == 'i') {
        Serial.read();
        gForce = 18.5; // Exceeds 15.0 G impact threshold
    } else {
        gForce = 1.2;
    }

    // MEMS Collision Check
    if (gForce >= MEMS_IMPACT_G_THRESHOLD && !impactDetected) {
        impactDetected = true;
        impactTime = millis();
        Serial.println("MEMS ACCELEROMETER: HIGH-G IMPACT DETECTED!");
    }
}

bool checkDetonationCondition() {
    unsigned long currentMillis = millis();

    // MODE 1: Proximity / Airburst (Time-based airburst from launch)
    if (targetFuzeMode == "Proximity") {
        float elapsedFlightSeconds = (currentMillis - launchStartTime) / 1000.0;
        if (elapsedFlightSeconds >= targetFuzeTime) {
            Serial.println("FUZE TRIGGER: Proximity airburst timer expired!");
            return true;
        }
    }

    // MODE 2: Time Delay (Post-impact delay milliseconds after MEMS hit)
    if (targetFuzeMode == "Time" && impactDetected) {
        if (currentMillis - impactTime >= (unsigned long)targetFuzeTime) {
            Serial.println("FUZE TRIGGER: Post-impact delay expired!");
            return true;
        }
    }

    // MODE 3: Point Detonation / Impact (Instant trigger on MEMS G-spike)
    if (targetFuzeMode == "Impact" && impactDetected) {
        Serial.println("FUZE TRIGGER: Instant MEMS impact detonation!");
        return true;
    }

    return false;
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
    c1Deflection = pidOutput;
    c2Deflection = -pidOutput;
    c3Deflection = pidOutput * 0.8;
    c4Deflection = -pidOutput * 0.8;

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
        http.begin(telemetryUrl);
        http.addHeader("Content-Type", "application/json");

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

        http.POST(payload);
        http.end();
    }
}