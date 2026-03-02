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

// Callback to handle incoming notifications
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  uint16_t* distances = (uint16_t*)pData;

  for (int i = 0; i < length / 2; i++) {
    Serial.print("Sensor ");
    Serial.print(i+1);
    Serial.print(": ");
    Serial.println(distances[i]);
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
