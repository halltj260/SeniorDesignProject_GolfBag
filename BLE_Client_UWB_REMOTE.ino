#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLEAdvertisedDevice.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcdefab-1234-1234-1234-abcdefabcdef"

BLEAdvertisedDevice* myDevice = nullptr;
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

/* ================= MOTOR PINS ================= */
#define IN1 12
#define IN2 13
#define IN3 32
#define IN4 33
/* ================= MOTOR PINS ================= */


/* ================= MOTOR COMMANDS ================= */
#define TARGET_DISTANCE .      // meters
#define TURN_THRESHOLD  .25     // deadband or difference between base stations before turning 1.0 = 1 meter
char currentMotion = 'S';
void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void forward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void left() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void right() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}
/* ================= MOTOR COMMANDS ================= */



#define NUM_BASES 4

struct ControlPacket {
  uint8_t header;
  uint8_t mode;
  uint8_t buttons;
  float distances[NUM_BASES];
};




#define NOTIFY_INTERVAL 500   // ms (10 Hz)

static unsigned long lastNotifyTime = 0;

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* data,
  size_t length,
  bool isNotify) {

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

  /* ================= AUTO MODE ================= */

  
  if (packet.mode == 1) {

    float d0 = packet.distances[2];
    float d1 = (packet.distances[3]);

    // Basic validation
    if (d0 <= 0 || d1 <= 0) {
      stopMotors();
      return;
    }

    float error = d0 - d1;
    float avgDistance = (d0 + d1) / 2.0;

    Serial.print("AUTO | d0: "); Serial.print(d0);
    Serial.print(" d1: "); Serial.print(d1);
    Serial.print(" error: "); Serial.print(error);
    Serial.print(" avg: "); Serial.println(avgDistance);

    // Stop when target distance reached
    if (avgDistance <= TARGET_DISTANCE) {
      Serial.println("Target reached - STOP");
      stopMotors();
      return;
    }

    // Steering deadband
    if (abs(error) > TURN_THRESHOLD) {

      if (error < 0) {
        Serial.println("Turning LEFT");
        left();
      } else {
        Serial.println("Turning RIGHT");
        right();
      }

    } else {
      Serial.println("Moving FORWARD");
      forward();
    }

    return;
  }

  /* ================= MANUAL MODE ================= */

  bool forwardBtn  = packet.buttons & (1 << 0);
  bool backwardBtn = packet.buttons & (1 << 1);
  bool leftBtn     = packet.buttons & (1 << 2);
  bool rightBtn    = packet.buttons & (1 << 3);

  if (forwardBtn)        forward();
  else if (backwardBtn)  backward();
  else if (leftBtn)      left();
  else if (rightBtn)     right();
  else                   stopMotors();
}


// Scan callback
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.println("Found device: " + String(advertisedDevice.toString().c_str()));
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
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

/* ================= MOTOR OUTPUTS ================= */
pinMode(IN1, OUTPUT);
pinMode(IN2, OUTPUT);
pinMode(IN3, OUTPUT);
pinMode(IN4, OUTPUT);
/* ================= MOTOR OUTPUTS ================= */



/* ================= BLE-SCAN ================= */

  BLEDevice::init("");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30, false);  // scan for 30s
  /* ================= BLE-SCAN ================= */

}

void loop() {
  // If server is discovered but not yet connected
  if (myDevice != nullptr && (pClient == nullptr || !pClient->isConnected())) {
    connectToServer();
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

  delay(500);
}
