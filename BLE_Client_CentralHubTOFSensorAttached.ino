#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLEAdvertisedDevice.h>



//TOF SENSOR HEADERS FROM LIBRARY
#include <Wire.h>
#include <VL53L1X.h>
//TOF LASER SENSORS
// The number of sensors in your system.
const uint8_t sensorCount = 3;
// The Arduino pin connected to the XSHUT pin of each sensor.
const uint8_t xshutPins[sensorCount] = { 5, 18, 19 };
VL53L1X sensors[sensorCount];



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



//TOF Sensor Setup
 //Set Wires (SDA, SCL, FREQ)
  Wire.begin(21,22);
  Wire.setClock(400000); // use 400 kHz I2C

  // Disable/reset all sensors by driving their XSHUT pins low.
  for (uint8_t i = 0; i < sensorCount; i++)
  {
    pinMode(xshutPins[i], OUTPUT);
    digitalWrite(xshutPins[i], LOW);
  }

  // Enable, initialize, and start each sensor, one by one.
  for (uint8_t i = 0; i < sensorCount; i++)
  {
    // Stop driving this sensor's XSHUT low. This should allow the carrier
    // board to pull it high. (We do NOT want to drive XSHUT high since it is
    // not level shifted.) Then wait a bit for the sensor to start up.
    pinMode(xshutPins[i], INPUT);
    delay(10);

    sensors[i].setTimeout(500);
    if (!sensors[i].init())
    {
      Serial.print("Failed to detect and initialize sensor ");
      Serial.println(i);
      while (1);
    }

    // Each sensor must have its address changed to a unique value other than
    // the default of 0x29 (except for the last one, which could be left at
    // the default). To make it simple, we'll just count up from 0x2A.
    sensors[i].setAddress(0x2A + i);

    sensors[i].startContinuous(50);
  }
}



void loop() {
  // If server is discovered but not yet connected
  if (myDevice != nullptr && (pClient == nullptr || !pClient->isConnected())) {
    connectToServer();

    for (uint8_t i = 0; i < sensorCount; i++)
  {
    Serial.print("Sensor ");
    Serial.print(i+1);
    Serial.print(": ");
    Serial.print(sensors[i].read());
    Serial.print(" mm");
    if (sensors[i].timeoutOccurred()) { Serial.print(" TIMEOUT"); }
    Serial.println();
  }
      Serial.println("-------------------------------------");
      delay(1000);



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
