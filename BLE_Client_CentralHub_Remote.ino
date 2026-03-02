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



/* ================= NOTIFY CALLBACK ================= */
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* data, 
  size_t length, 
  bool isNotify) {

 if (length != 1) return;

  char cmd = data[0];
  currentMotion = cmd;

  Serial.print("Received: ");
  Serial.println(cmd);

  switch (cmd) {
    case 'F': forward(); break;
    case 'B': backward(); break;
    case 'L': left(); break;
    case 'R': right(); break;
    case 'A': stopMotors(); break;
    case 'M': stopMotors(); break;
    case 'S': stopMotors(); break;
  }
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

  delay(100);
}
