#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>



/* ================= BLE-START ================= */

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcdefab-1234-1234-1234-abcdefabcdef"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Client connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Client disconnected");
  }
};
/* ================= BLE-END ================= */




/* ================= BUTTONS-START ================= */

#define AUTO      21
#define MANUAL    22
#define FORWARD   32
#define BACKWARD  33
#define LEFT      26
#define RIGHT     27

enum Mode {
  MODE_MANUAL,
  MODE_AUTO
};

Mode currentMode = MODE_MANUAL;

const int buttonPins[] = {AUTO, MANUAL, FORWARD, BACKWARD, LEFT, RIGHT};
const char buttonChars[] = {'A','M','F','B','L','R'};
const int buttonCount = 6;

int lastState[buttonCount];
int currentStateBtn[buttonCount];
/* ================= BUTTONS-END ================= */

void setup() {
  Serial.begin(115200);


/* ================= BUTTONS-START ================= */
for (int i = 0; i < buttonCount; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastState[i] = digitalRead(buttonPins[i]);
  }
/* ================= BUTTONS-END ================= */



/* ================= BLE-START ================= */

  BLEDevice::init("ESP32_Remote");

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  // Enable notifications
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("Remote is advertising...");

/* ================= BLE-END ================= */
}

void sendCommand(char cmd) {
  if (!deviceConnected) return;

  pCharacteristic->setValue((uint8_t*)&cmd, 1);
  pCharacteristic->notify();

  Serial.print("Sent: ");
  Serial.println(cmd);
}


void loop() {
  if (deviceConnected) {
    static unsigned long lastTime = 0;

/* ================= BUTTONS-START ================= */

for (int i = 0; i < buttonCount; i++) {

  currentStateBtn[i] = digitalRead(buttonPins[i]);

  // ===== BUTTON PRESSED =====
  if (lastState[i] == HIGH && currentStateBtn[i] == LOW) {

    if (buttonPins[i] == AUTO) {
      currentMode = MODE_AUTO;
      sendCommand('A');
    }

    else if (buttonPins[i] == MANUAL) {
      currentMode = MODE_MANUAL;
      sendCommand('M');
    }

    else if (currentMode == MODE_MANUAL) {

      if (buttonPins[i] == FORWARD)  sendCommand('F');
      if (buttonPins[i] == BACKWARD) sendCommand('B');
      if (buttonPins[i] == LEFT)     sendCommand('L');
      if (buttonPins[i] == RIGHT)    sendCommand('R');
    }

    delay(20); // debounce
  }

  // ===== BUTTON RELEASED =====
  if (lastState[i] == LOW && currentStateBtn[i] == HIGH) {

    if (currentMode == MODE_MANUAL) {

      // Only stop if a direction button was released
      if (buttonPins[i] == FORWARD ||
          buttonPins[i] == BACKWARD ||
          buttonPins[i] == LEFT ||
          buttonPins[i] == RIGHT) {

        sendCommand('S');  // STOP
      }
    }

    delay(20);
  }

  lastState[i] = currentStateBtn[i];
}


// for (int i = 0; i < buttonCount; i++) {

//     int currentState = digitalRead(buttonPins[i]);

//     // Detect press (HIGH → LOW)
//     if (lastState[i] == HIGH && currentState == LOW) {
//       sendCommand(buttonChars[i]);
//       delay(50); // debounce
//     }

//     lastState[i] = currentState;
//   }
/* ================= BUTTONS-END ================= */
  }
}
