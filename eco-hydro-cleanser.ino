#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

// ============================================================================
// WI-FI & OTA CONFIGURATION
// ============================================================================
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD"; // Update with your actual Wi-Fi password

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// BTS7960 #1: Left Propulsion Motor
#define PIN_BTS1_RPWM 25
#define PIN_BTS1_LPWM 26

// BTS7960 #2: Right Propulsion Motor
#define PIN_BTS2_RPWM 27
#define PIN_BTS2_LPWM 14

// L298N: Dual Pickup Mechanism Motors
#define PIN_L298N_IN1 32
#define PIN_L298N_IN2 33
#define PIN_L298N_IN3 18
#define PIN_L298N_IN4 19
#define PIN_L298N_ENA 23
#define PIN_L298N_ENB 13

// Ultrasonic Sensor Left
#define PIN_TRIG_LEFT 4
#define PIN_ECHO_LEFT 34

// Ultrasonic Sensor Right
#define PIN_TRIG_RIGHT 5
#define PIN_ECHO_RIGHT 35

// ============================================================================
// CALIBRATION & TUNING CONSTANTS
// ============================================================================

const uint32_t PWM_FREQ = 5000;   // 5 kHz PWM frequency
const uint8_t PWM_RES = 8;        // 8-bit resolution (0 - 255)

const uint8_t SPEED_SEARCH = 140;       // Cruising speed during search
const uint8_t SPEED_APPROACH = 100;     // Slow approach speed
const uint8_t SPEED_TURN_SLOW = 60;      // Slow wheel speed during adjustments
const uint8_t SPEED_LEVER = 130;        // Controlled speed for lever mechanism
const uint8_t SPEED_RETURN = 150;       // Reverse return speed

const float DIST_DETECTION_MAX = 20.0f; // Start approaching if object detected within range
const float DIST_PICKUP_READY = 5.0f;   // Stop and engage lever mechanism at this range
const float DIST_MAX_VALID = 200.0f;    // Max operational limit for valid readings

const uint32_t LEVER_UP_TIME    = 1500;   // Motor time required to bring lever UP
const uint32_t DEBRIS_DROP_TIME = 1000;   // Dwell time for gravity drop at peak angle
const uint32_t LEVER_DOWN_TIME = 1500;   // Motor time required to return lever DOWN
const uint32_t ESCAPE_TIME     = 1800;   // Reverse propulsion duration post-collection
const uint32_t STABILIZE_TIME  = 600;    // Pause before mechanical actions

// ============================================================================
// STATE MACHINE DEFINITIONS
// ============================================================================
enum SystemState {
  STATE_MANUAL_OVERRIDE,
  STATE_START,
  STATE_SEARCH,
  STATE_APPROACH,
  STATE_STABILIZE,
  STATE_PICKUP_UP,
  STATE_DROP,
  STATE_PICKUP_DOWN,
  STATE_RETURN
};

SystemState currentState = STATE_START;

// Global Tracking Variables
float distLeft = 0.0f;
float distRight = 0.0f;
uint32_t stateTimer = 0;
uint32_t serialTelemetryTimer = 0;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
void stopAllMotors();
void setPropulsion(int16_t leftSpeed, int16_t rightSpeed);
void setPickupLever(bool enable, bool directionUp);
float readUltrasonic(uint8_t trigPin, uint8_t echoPin);
void updateSensors();
void processSerialCommands();
void logTelemetry();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize Motor Pins
  pinMode(PIN_L298N_IN1, OUTPUT);
  pinMode(PIN_L298N_IN2, OUTPUT);
  pinMode(PIN_L298N_IN3, OUTPUT);
  pinMode(PIN_L298N_IN4, OUTPUT);

  // Configure ESP32 LEDC PWM Outputs
  ledcAttach(PIN_BTS1_RPWM, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_BTS1_LPWM, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_BTS2_RPWM, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_BTS2_LPWM, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_L298N_ENA, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_L298N_ENB, PWM_FREQ, PWM_RES);

  // Initialize Ultrasonic Pins
  pinMode(PIN_TRIG_LEFT, OUTPUT);
  pinMode(PIN_ECHO_LEFT, INPUT);
  pinMode(PIN_TRIG_RIGHT, OUTPUT);
  pinMode(PIN_ECHO_RIGHT, INPUT);
  
  digitalWrite(PIN_TRIG_LEFT, LOW);
  digitalWrite(PIN_TRIG_RIGHT, LOW);

  stopAllMotors();

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Setup ArduinoOTA Parameters
  ArduinoOTA.setHostname("ESP32-DebrisCollector");
  
  ArduinoOTA.onStart([]() {
    stopAllMotors();
    Serial.println("\n[OTA] Update started...");
  });
  
  ArduinoOTA.begin();

  Serial.println(F("\n=========================================="));
  Serial.println(F(" Debris Collector (Headless Mode) Ready    "));
  Serial.print(F(" IP Address: "));
  Serial.println(WiFi.localIP());
  Serial.println(F("=========================================="));

  delay(1000);
  currentState = STATE_SEARCH;
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // OTA Handler for Wireless Updates
  ArduinoOTA.handle();

  processSerialCommands();

  if (currentState != STATE_MANUAL_OVERRIDE) {
    updateSensors();
  }

  // Periodic Telemetry Logging (Every 500 ms)
  if (millis() - serialTelemetryTimer >= 500) {
    serialTelemetryTimer = millis();
    logTelemetry();
  }

  // Automatic State Machine Logic
  switch (currentState) {

    case STATE_MANUAL_OVERRIDE:
      break;

    case STATE_START:
      stopAllMotors();
      currentState = STATE_SEARCH;
      break;

    case STATE_SEARCH:
      setPropulsion(SPEED_SEARCH, SPEED_SEARCH);
      if ((distLeft > 0 && distLeft <= DIST_DETECTION_MAX) ||
          (distRight > 0 && distRight <= DIST_DETECTION_MAX)) {
        stopAllMotors();
        currentState = STATE_APPROACH;
      }
      break;

    case STATE_APPROACH:
      if ((distLeft > 0 && distLeft <= DIST_PICKUP_READY) ||
          (distRight > 0 && distRight <= DIST_PICKUP_READY)) {
        stopAllMotors();
        stateTimer = millis();
        currentState = STATE_STABILIZE;
      } 
      else if (distLeft > 0 && distRight > 0) {
        if (distLeft < (distRight - 2.0f)) {
          setPropulsion(SPEED_TURN_SLOW, SPEED_APPROACH);
        } else if (distRight < (distLeft - 2.0f)) {
          setPropulsion(SPEED_APPROACH, SPEED_TURN_SLOW);
        } else {
          setPropulsion(SPEED_APPROACH, SPEED_APPROACH);
        }
      } else {
        setPropulsion(SPEED_APPROACH, SPEED_APPROACH);
      }
      break;

    case STATE_STABILIZE:
      stopAllMotors();
      if (millis() - stateTimer >= STABILIZE_TIME) {
        stateTimer = millis();
        setPickupLever(true, true);
        currentState = STATE_PICKUP_UP;
      }
      break;

    case STATE_PICKUP_UP:
      if (millis() - stateTimer >= LEVER_UP_TIME) {
        setPickupLever(false, false);
        stateTimer = millis();
        currentState = STATE_DROP;
      }
      break;

    case STATE_DROP:
      stopAllMotors();
      if (millis() - stateTimer >= DEBRIS_DROP_TIME) {
        stateTimer = millis();
        setPickupLever(true, false);
        currentState = STATE_PICKUP_DOWN;
      }
      break;

    case STATE_PICKUP_DOWN:
      if (millis() - stateTimer >= LEVER_DOWN_TIME) {
        setPickupLever(false, false);
        stateTimer = millis();
        setPropulsion(-SPEED_RETURN, -SPEED_RETURN);
        currentState = STATE_RETURN;
      }
      break;

    case STATE_RETURN:
      if (millis() - stateTimer >= ESCAPE_TIME) {
        stopAllMotors();
        currentState = STATE_SEARCH;
      }
      break;
  }

  delay(20);
}

// ============================================================================
// HARDWARE DRIVERS
// ============================================================================

void setPropulsion(int16_t leftSpeed, int16_t rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  if (leftSpeed > 0) {
    ledcWrite(PIN_BTS1_LPWM, 0);
    ledcWrite(PIN_BTS1_RPWM, leftSpeed);
  } else if (leftSpeed < 0) {
    ledcWrite(PIN_BTS1_RPWM, 0);
    ledcWrite(PIN_BTS1_LPWM, -leftSpeed);
  } else {
    ledcWrite(PIN_BTS1_RPWM, 0);
    ledcWrite(PIN_BTS1_LPWM, 0);
  }

  if (rightSpeed > 0) {
    ledcWrite(PIN_BTS2_LPWM, 0);
    ledcWrite(PIN_BTS2_RPWM, rightSpeed);
  } else if (rightSpeed < 0) {
    ledcWrite(PIN_BTS2_RPWM, 0);
    ledcWrite(PIN_BTS2_LPWM, -rightSpeed);
  } else {
    ledcWrite(PIN_BTS2_RPWM, 0);
    ledcWrite(PIN_BTS2_LPWM, 0);
  }
}

void setPickupLever(bool enable, bool directionUp) {
  if (!enable) {
    digitalWrite(PIN_L298N_IN1, LOW);
    digitalWrite(PIN_L298N_IN2, LOW);
    digitalWrite(PIN_L298N_IN3, LOW);
    digitalWrite(PIN_L298N_IN4, LOW);
    ledcWrite(PIN_L298N_ENA, 0);
    ledcWrite(PIN_L298N_ENB, 0);
    return;
  }

  if (directionUp) {
    digitalWrite(PIN_L298N_IN1, HIGH);
    digitalWrite(PIN_L298N_IN2, LOW);
    digitalWrite(PIN_L298N_IN3, LOW);
    digitalWrite(PIN_L298N_IN4, HIGH);
  } else {
    digitalWrite(PIN_L298N_IN1, LOW);
    digitalWrite(PIN_L298N_IN2, HIGH);
    digitalWrite(PIN_L298N_IN3, HIGH);
    digitalWrite(PIN_L298N_IN4, LOW);
  }

  ledcWrite(PIN_L298N_ENA, SPEED_LEVER);
  ledcWrite(PIN_L298N_ENB, SPEED_LEVER);
}

void stopAllMotors() {
  setPropulsion(0, 0);
  setPickupLever(false, false);
}

float readUltrasonic(uint8_t trigPin, uint8_t echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, 15000);
  if (duration == 0) return 999.0f;

  float distance = (duration * 0.0343f) / 2.0f;
  if (distance > DIST_MAX_VALID) return 999.0f;

  return distance;
}

void updateSensors() {
  distLeft = readUltrasonic(PIN_TRIG_LEFT, PIN_ECHO_LEFT);
  delay(10);
  distRight = readUltrasonic(PIN_TRIG_RIGHT, PIN_ECHO_RIGHT);
}

void logTelemetry() {
  Serial.print("[STATE: ");
  switch (currentState) {
    case STATE_SEARCH:           Serial.print("SEARCH"); break;
    case STATE_APPROACH:         Serial.print("APPROACH"); break;
    case STATE_STABILIZE:        Serial.print("STABILIZE"); break;
    case STATE_PICKUP_UP:        Serial.print("PICKUP_UP"); break;
    case STATE_DROP:             Serial.print("DROP"); break;
    case STATE_PICKUP_DOWN:      Serial.print("PICKUP_DOWN"); break;
    case STATE_RETURN:           Serial.print("RETURN"); break;
    case STATE_MANUAL_OVERRIDE:  Serial.print("MANUAL"); break;
    default:                     Serial.print("START"); break;
  }
  Serial.print("] | Left: ");
  if (distLeft > 200) Serial.print("CLEAR"); else Serial.print(distLeft, 1);
  Serial.print(" cm | Right: ");
  if (distRight > 200) Serial.print("CLEAR"); else Serial.print(distRight, 1);
  Serial.println(" cm");
}

void processSerialCommands() {
  if (!Serial.available()) return;

  char cmd = toupper(Serial.read());
  switch (cmd) {
    case 'S':
      stopAllMotors();
      currentState = STATE_MANUAL_OVERRIDE;
      Serial.println(F("[MANUAL] Stopped All Motors."));
      break;
    case 'A':
      currentState = STATE_START;
      Serial.println(F("[AUTO] Resuming Automatic Mode."));
      break;
    case 'F':
      currentState = STATE_MANUAL_OVERRIDE;
      setPickupLever(false, false);
      setPropulsion(SPEED_SEARCH, SPEED_SEARCH);
      break;
    case 'B':
      currentState = STATE_MANUAL_OVERRIDE;
      setPickupLever(false, false);
      setPropulsion(-SPEED_SEARCH, -SPEED_SEARCH);
      break;
    case 'L':
      currentState = STATE_MANUAL_OVERRIDE;
      setPickupLever(false, false);
      setPropulsion(-SPEED_TURN_SLOW, SPEED_SEARCH);
      break;
    case 'R':
      currentState = STATE_MANUAL_OVERRIDE;
      setPickupLever(false, false);
      setPropulsion(SPEED_SEARCH, -SPEED_TURN_SLOW);
      break;
    case 'U':
      currentState = STATE_MANUAL_OVERRIDE;
      setPropulsion(0, 0);
      setPickupLever(true, true);
      break;
    case 'D':
      currentState = STATE_MANUAL_OVERRIDE;
      setPropulsion(0, 0);
      setPickupLever(true, false);
      break;
  }
}