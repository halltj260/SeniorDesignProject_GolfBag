#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLEAdvertisedDevice.h>

#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcdefab-1234-1234-1234-abcdefabcdef"

BLEAdvertisedDevice* myDevice = nullptr;
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

/* ================= VL53L1X ================= */
const uint8_t sensorCount = 3;
const uint8_t xshutPins[sensorCount] = { 5, 18, 19 };
VL53L1X sensors[sensorCount];

#define STOP_DISTANCE_MM 200

volatile bool obstacleActive = false;

/* ================= IBT-2 MOTOR PINS ================= */
#define LEFT_RPWM 32
#define LEFT_LPWM 25
#define LEFT_R_EN 33
#define LEFT_L_EN 26

#define RIGHT_RPWM 27
#define RIGHT_LPWM 12
#define RIGHT_R_EN 14
#define RIGHT_L_EN 13

#define MOTOR_SPEED 200



/* ================= MOTOR COMMANDS ================= */
#define TARGET_DISTANCE 1.5  // meters
#define TURN_THRESHOLD .15   // deadband or difference between base stations before turning 1.0 = 1 meter
// char currentMotion = 'S';


/* ================= MOTOR FUNCTIONS ================= */
enum ControlMode { MANUAL,
                   AUTO };
ControlMode currentMode = MANUAL;  // or switch dynamically
enum MotionState { STOPPED,
                   FORWARD,
                   BACKWARD,
                   LEFT,
                   RIGHT };
MotionState currentMotion = STOPPED;

bool hasSlowStopped = false;
void stopMotors() {

  // AUTO MODE → always hard stop
  if (currentMode == AUTO) {
    Serial.println("AUTO: Soft STOP");

    int steps = 8;  // steps / 10 = how many seconds to slow
    int delayPerStep = 100;
    if (!hasSlowStopped) {
      for (int i = steps; i >= 0; i--) {
        int speed = (MOTOR_SPEED * i) / steps;

        analogWrite(LEFT_RPWM, speed);
        analogWrite(LEFT_LPWM, 0);
        analogWrite(RIGHT_RPWM, speed);
        analogWrite(RIGHT_LPWM, 0);

        delay(delayPerStep);  // <-- REQUIRED
      }
      currentMotion = STOPPED;
      return;
    }
    hasSlowStopped = true;
  }

  // MANUAL MODE → allow one-time slow stop

  if (!hasSlowStopped && currentMotion != STOPPED && currentMotion != LEFT && currentMotion != RIGHT) {
    Serial.println("Soft STOP (direction-aware)");

    int steps = 12;  // steps / 10 = how many seconds to slow
    int delayPerStep = 100;

    for (int i = steps; i >= 0; i--) {
      int speed = (MOTOR_SPEED * i) / steps;

      switch (currentMotion) {

        case BACKWARD:
          analogWrite(LEFT_RPWM, 0);
          analogWrite(LEFT_LPWM, speed);
          analogWrite(RIGHT_RPWM, 0);
          analogWrite(RIGHT_LPWM, speed);
          break;

        case FORWARD:
          analogWrite(LEFT_RPWM, speed);
          analogWrite(LEFT_LPWM, 0);
          analogWrite(RIGHT_RPWM, speed);
          analogWrite(RIGHT_LPWM, 0);
          break;

        case LEFT:
          // left motor backward, right forward
          analogWrite(LEFT_RPWM, speed);
          analogWrite(LEFT_LPWM, 0);
          analogWrite(RIGHT_RPWM, 0);
          analogWrite(RIGHT_LPWM, speed);
          break;

        case RIGHT:
          // left motor forward, right backward
          analogWrite(LEFT_RPWM, 0);
          analogWrite(LEFT_LPWM, speed);
          analogWrite(RIGHT_RPWM, speed);
          analogWrite(RIGHT_LPWM, 0);
          break;

        default:
          break;
      }

      delay(delayPerStep);
    }

    hasSlowStopped = true;
  }

  // Hard stop always executes
  Serial.println("Hard STOP");

  analogWrite(LEFT_RPWM, 0);
  analogWrite(LEFT_LPWM, 0);
  analogWrite(RIGHT_RPWM, 0);
  analogWrite(RIGHT_LPWM, 0);

  currentMotion = STOPPED;
}


void backward() {
  currentMotion = BACKWARD;
  hasSlowStopped = false;
  Serial.println("Backward");
  analogWrite(LEFT_RPWM, 0);
  analogWrite(LEFT_LPWM, MOTOR_SPEED);
  analogWrite(RIGHT_RPWM, 0);
  analogWrite(RIGHT_LPWM, MOTOR_SPEED);
}

void forward() {
  currentMotion = FORWARD;
  hasSlowStopped = false;
  Serial.println("Forward");
  analogWrite(LEFT_RPWM, MOTOR_SPEED);
  analogWrite(LEFT_LPWM, 0);
  analogWrite(RIGHT_RPWM, MOTOR_SPEED);
  analogWrite(RIGHT_LPWM, 0);
}

void left() {
  currentMotion = LEFT;
  hasSlowStopped = false;

  Serial.println("Left");

  // Left motor backward
  analogWrite(LEFT_RPWM, MOTOR_SPEED - 50);
  analogWrite(LEFT_LPWM, 0);

  // Right motor forward
  analogWrite(RIGHT_RPWM, 0);
  analogWrite(RIGHT_LPWM, MOTOR_SPEED - 50);
}

void right() {
  currentMotion = RIGHT;
  hasSlowStopped = false;
  Serial.println("Right");

  // Left motor forward
  analogWrite(LEFT_RPWM, 0);
  analogWrite(LEFT_LPWM, MOTOR_SPEED - 50);

  // Right motor backward
  analogWrite(RIGHT_RPWM, MOTOR_SPEED - 50);
  analogWrite(RIGHT_LPWM, 0);
}

/* ================= SENSOR CHECK ================= */
bool tooClose() {
  for (uint8_t i = 0; i < sensorCount; i++) {
    int dist = sensors[i].read();
    if (!sensors[i].timeoutOccurred() && dist > 0 && dist <= STOP_DISTANCE_MM) {
      Serial.print("STOP: Sensor ");
      Serial.print(i);
      Serial.print(" = ");
      Serial.println(dist);
      return true;
    }
  }
  return false;
}


/* ================= MOTOR COMMANDS PACKET ================= */


#define NUM_BASES 4

struct ControlPacket {
  uint8_t header;
  uint8_t mode;
  uint8_t buttons;
  float distances[NUM_BASES];
};




#define NOTIFY_INTERVAL 50  // ms (10 Hz)

static unsigned long lastNotifyTime = 0;

static void notifyCallback(


  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* data,
  size_t length,
  bool isNotify) {


  if (obstacleActive) {
    return;  // 🚫 Ignore ALL BLE commands while obstacle is present
  }

  // === GLOBAL RATE LIMIT ===
  if (millis() - lastNotifyTime < NOTIFY_INTERVAL) return;
  lastNotifyTime = millis();

  if (length != sizeof(ControlPacket)) {
    Serial.print("Unexpected packet size: ");
    Serial.println(length);
    return;
  }

  ControlPacket packet;
  memcpy(&packet, data, sizeof(ControlPacket));

  if (packet.header != 0xAA) {
    Serial.println("Invalid header");
    return;
  }

  Serial.println("==== PACKET RECEIVED ====");

  Serial.print("Mode: ");
  Serial.println(packet.mode == 0 ? "MANUAL" : "AUTO");

  Serial.print("Buttons bitmask: ");
  Serial.println(packet.buttons, BIN);

  Serial.print("Distances: ");
  for (int i = 0; i < NUM_BASES; i++) {
    Serial.print(packet.distances[i], 3);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println("=========================");

  /* ================= AUTO MODE (PD-CONTROL, TUNED) ================= */

  // if (packet.mode == 1) {
  //   currentMode = AUTO;

  //   float d0 = packet.distances[2];   // Left sensor
  //   float d1 = packet.distances[3];   // Right sensor

  //   /* ===== BASIC VALIDATION ===== */
  //   if (d0 <= 0 || d1 <= 0) {
  //     stopMotors();
  //     return;
  //   }

  //   /* ===== FILTERING ===== */
  //   static float d0_filt = 0;
  //   static float d1_filt = 0;

  //   float alpha = 0.4;
  //   // ↑ Increase for faster response (0.3–0.5 recommended)

  //   d0_filt = alpha * d0 + (1 - alpha) * d0_filt;
  //   d1_filt = alpha * d1 + (1 - alpha) * d1_filt;

  //   /* ===== OPTIONAL SENSOR BIAS COMPENSATION ===== */
  //   float d1_bias = 0.0;  // set to ~0.1 if needed after testing
  //   float d1_corrected = d1_filt - d1_bias;

  //   /* ===== ERROR CALCULATION ===== */
  //   float error = d0_filt - d1_corrected;

  //   /* ===== DEAD ZONE (improves turn exit) ===== */
  //   float deadZone = 0.02;  // ~2 cm
  //   if (abs(error) < deadZone) {
  //     error = 0;
  //   }

  //   float avgDistance = (d0_filt + d1_corrected) / 2.0;

  //   Serial.print("AUTO | error: ");
  //   Serial.print(error);
  //   Serial.print(" avg: ");
  //   Serial.println(avgDistance);

  //   /* ===== STOP CONDITION ===== */
  //   if (avgDistance <= TARGET_DISTANCE) {
  //     Serial.println("Target reached - STOP");
  //     stopMotors();
  //     return;
  //   }

  //   /* ===== PD CONTROLLER ===== */

  //   float Kp = 100;   // ↑ Increase for faster turn entry (80–120)
  //   float Kd = 30;    // ↑ Helps with fast turn exit & reduces overshoot

  //   static float lastError = 0;

  //   float derivative = error - lastError;

  //   float correction = Kp * error + Kd * derivative;

  //   lastError = error;

  //   /* ===== BASE SPEED ===== */
  //   int baseSpeed = MOTOR_SPEED;
  //   // Try 100–150 for stability

  //   int leftSpeed  = baseSpeed - correction;
  //   int rightSpeed = baseSpeed + correction;

  //   /* ===== CORRECTION CLAMP (prevents aggressive spikes) ===== */
  //   int maxCorrection = 120;
  //   leftSpeed  = constrain(leftSpeed,  0, 255);
  //   rightSpeed = constrain(rightSpeed, 0, 255);

  //   /* ===== APPLY MOTION ===== */
  //   currentMotion = FORWARD;

  //   // Forward motion (swap pins if direction is reversed)
  //   analogWrite(LEFT_RPWM, leftSpeed);
  //   analogWrite(LEFT_LPWM, 0);

  //   analogWrite(RIGHT_RPWM, rightSpeed);
  //   analogWrite(RIGHT_LPWM, 0);

  //   return;
  // }
  // else {
  //   currentMode = MANUAL;
  // }
  /* ================= AUTO MODE (works-ish) ================= */


  static unsigned long forwardLockUntil = 0;
  static unsigned long counterTurnUntil = 0;

  static int counterTurnDir = 0;  // -1 = right, +1 = left

#define FORWARD_LOCK_TIME 700   // ms
#define COUNTER_TURN_TIME 1000  // ms (tune 80–180)



  if (packet.mode == 1) {
    currentMode = AUTO;

    float d0 = packet.distances[2];
    float d1 = packet.distances[3];

    static float lastGood_d0 = 0;
    static float lastGood_d1 = 0;

    bool badD0 = (d0 <= 0);
    bool badD1 = (d1 <= 0);

    // If both bad → truly ignore frame
    if (badD0 && badD1) {
      return;  // just skip update, keep last motor command running
    }

    // Replace invalid values with last known good
    if (!badD0) lastGood_d0 = d0;
    if (!badD1) lastGood_d1 = d1;

    // Use filtered inputs
    d0 = lastGood_d0;
    d1 = lastGood_d1;

    /* ===== FILTERING (smooth noisy UWB data) ===== */
    static float d0_filt = 0;
    static float d1_filt = 0;

    float alpha = 0.3;  // lower = smoother (try 0.2–0.4)

    d0_filt = alpha * d0 + (1 - alpha) * d0_filt;
    d1_filt = alpha * d1 + (1 - alpha) * d1_filt;

    float error = d0_filt - d1_filt;
    float avgDistance = (d0_filt + d1_filt) / 2.0;

    Serial.print("AUTO | d0: ");
    Serial.print(d0_filt);
    Serial.print(" d1: ");
    Serial.print(d1_filt);
    Serial.print(" error: ");
    Serial.print(error);
    Serial.print(" avg: ");
    Serial.println(avgDistance);

    /* ===== STOP CONDITION ===== */
    if (avgDistance <= TARGET_DISTANCE) {
      Serial.println("Target reached - STOP");
      stopMotors();
      return;
    }

/* ===== ASYMMETRIC HYSTERESIS TURN CONTROL ===== */
#define TURN_ENTER_LEFT 0.52
#define TURN_ENTER_RIGHT 0.35  // easier to trigger right

#define TURN_EXIT_LEFT 0.35
#define TURN_EXIT_RIGHT 0.20

// SOFT TURN
#define SOFT_ENTER_LEFT 0.25
#define SOFT_EXIT_LEFT 0.20

#define SOFT_ENTER_RIGHT 0.2
#define SOFT_EXIT_RIGHT 0.05

    static int softState = 0;
    // -1 = soft right, 0 = none, 1 = soft left

    static int turnState = 0;
    // -1 = right, 0 = straight, 1 = left

    if (turnState == 0) {
      if (error > TURN_ENTER_LEFT) {
        turnState = 1;
        Serial.println("ENTER LEFT");
      } else if (error < -TURN_ENTER_RIGHT) {
        turnState = -1;
        Serial.println("ENTER RIGHT");
      }
    } else if (turnState == 1) {  // was turning LEFT
      if (error < TURN_EXIT_LEFT) {
        turnState = 0;

        // Apply brief opposite turn (RIGHT)
        counterTurnDir = -1;
        counterTurnUntil = millis() + COUNTER_TURN_TIME;

        // Then forward lock
        forwardLockUntil = counterTurnUntil + FORWARD_LOCK_TIME;

        Serial.println("EXIT LEFT → COUNTER RIGHT → LOCK");
      }
    } else if (turnState == -1) {  // was turning RIGHT
      if (error > -TURN_EXIT_RIGHT) {
        turnState = 0;

        // Apply brief opposite turn (LEFT)
        counterTurnDir = 1;
        counterTurnUntil = millis() + COUNTER_TURN_TIME;

        // Then forward lock
        forwardLockUntil = counterTurnUntil + FORWARD_LOCK_TIME;

        Serial.println("EXIT RIGHT → COUNTER LEFT → LOCK");
      }
    }

    /* ===== SOFT TURN HYSTERESIS ===== */

    if (turnState == 0) {  // only allow soft turns if NOT hard turning

      if (softState == 0) {
        if (error > SOFT_ENTER_LEFT) {
          softState = 1;
          Serial.println("ENTER SOFT LEFT");
        } else if (error < -SOFT_ENTER_RIGHT) {
          softState = -1;
          Serial.println("ENTER SOFT RIGHT");
        }
      } else if (softState == 1) {
        if (error < SOFT_EXIT_LEFT) {
          softState = 0;
          forwardLockUntil = millis() + FORWARD_LOCK_TIME;
          Serial.println("EXIT SOFT LEFT → LOCK");
        }
      } else if (softState == -1) {
        if (error > -SOFT_EXIT_RIGHT) {
          softState = 0;
          forwardLockUntil = millis() + FORWARD_LOCK_TIME;
          Serial.println("EXIT SOFT RIGHT → LOCK");
        }
      }

    } else {
      // If hard turning, disable soft turning
      softState = 0;
    }



    // ===== COUNTER TURN PHASE =====
    if (millis() < counterTurnUntil) {
      if (counterTurnDir == 1) {
        Serial.println("COUNTER LEFT");
        left();
      } else if (counterTurnDir == -1) {
        Serial.println("COUNTER RIGHT");
        right();
      }
      return;
    }

    // ===== FORWARD LOCK ENFORCEMENT =====
    if (millis() < forwardLockUntil) {
      Serial.println("FORWARD LOCK ACTIVE");
      forward();
      return;
    }

    /* ===== APPLY MOTION ===== */

    int baseSpeed = MOTOR_SPEED;
    int softDelta = 40;  // tune 30–60

    // HARD TURN (priority)
    if (turnState == 1) {
      left();
    } else if (turnState == -1) {
      right();
    }

    // SOFT TURN
    else if (softState == 1) {
      // Soft LEFT
      analogWrite(LEFT_RPWM, baseSpeed - softDelta);
      analogWrite(LEFT_LPWM, 0);

      analogWrite(RIGHT_RPWM, baseSpeed);
      analogWrite(RIGHT_LPWM, 0);

      Serial.println("SOFT LEFT");
    } else if (softState == -1) {
      // Soft RIGHT
      analogWrite(LEFT_RPWM, baseSpeed);
      analogWrite(LEFT_LPWM, 0);

      analogWrite(RIGHT_RPWM, baseSpeed - softDelta);
      analogWrite(RIGHT_LPWM, 0);

      Serial.println("SOFT RIGHT");
    }

    // STRAIGHT
    else {
      forward();
    }
    return;
  } else {
    currentMode = MANUAL;
  }



  /* ================= MANUAL MODE ================= */

  bool forwardBtn = packet.buttons & (1 << 0);
  bool backwardBtn = packet.buttons & (1 << 1);
  bool leftBtn = packet.buttons & (1 << 2);
  bool rightBtn = packet.buttons & (1 << 3);

  if (forwardBtn) forward();
  else if (backwardBtn) backward();
  else if (leftBtn) left();
  else if (rightBtn) right();
  else stopMotors();
}


// Scan callback
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.println("Found device: " + String(advertisedDevice.toString().c_str()));
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      Serial.println("Found our server!");
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
    }
  }
};

void connectToServer() {
  Serial.println("Connecting to server...");
  pClient = BLEDevice::createClient();

  if (pClient->connect(myDevice)) {
    Serial.println("Connected to server!");

    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
      Serial.println("Failed to find service.");
      pClient->disconnect();
      return;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.println("Failed to find characteristic.");
      pClient->disconnect();
      return;
    }

    if (pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

    Serial.println("Subscribed to notifications!");
  } else {
    Serial.println("Failed to connect, retrying in 5s...");
    delay(5000);
  }
}



void setup() {
  Serial.begin(115200);
  Serial.println("Starting Carrier...");

  hasSlowStopped = true;

  pinMode(LEFT_RPWM, OUTPUT);
  pinMode(LEFT_LPWM, OUTPUT);
  pinMode(RIGHT_RPWM, OUTPUT);
  pinMode(RIGHT_LPWM, OUTPUT);

  pinMode(LEFT_R_EN, OUTPUT);
  pinMode(LEFT_L_EN, OUTPUT);
  pinMode(RIGHT_R_EN, OUTPUT);
  pinMode(RIGHT_L_EN, OUTPUT);

  digitalWrite(LEFT_R_EN, HIGH);
  digitalWrite(LEFT_L_EN, HIGH);
  digitalWrite(RIGHT_R_EN, HIGH);
  digitalWrite(RIGHT_L_EN, HIGH);

  stopMotors();

  Wire.begin(21, 22);
  Wire.setClock(400000);

  for (uint8_t i = 0; i < sensorCount; i++) {
    pinMode(xshutPins[i], OUTPUT);
    digitalWrite(xshutPins[i], LOW);
  }

  for (uint8_t i = 0; i < sensorCount; i++) {
    pinMode(xshutPins[i], INPUT);
    delay(10);

    sensors[i].setTimeout(500);
    if (!sensors[i].init()) {
      Serial.println("Sensor init failed");
      while (1)
        ;
    }

    sensors[i].setAddress(0x2A + i);
    sensors[i].startContinuous(50);
  }

  // BLE setup
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30, false);  // scan for 30s
}


void loop() {
  // If server is discovered but not yet connected
  if (myDevice != nullptr && (pClient == nullptr || !pClient->isConnected())) {
    connectToServer();
  }

  if (tooClose()) {
    obstacleActive = true;
  } else {
    obstacleActive = false;
  }

  if (obstacleActive) {
    stopMotors();
    delay(500);
    backward();
    delay(1000);  // short and acceptable here (not in callback)
    stopMotors();
    return;  // 🚨 prevents normal control from running
  }

  // If client disconnects, reset to scan again
  if (pClient != nullptr && !pClient->isConnected()) {
    Serial.println("Disconnected, scanning again...");
    pClient->disconnect();
    pClient = nullptr;
    pRemoteCharacteristic = nullptr;
    myDevice = nullptr;

    BLEDevice::getScan()->start(30, false);  // scan again
  }

  delay(100);
}
