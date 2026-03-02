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

Mode currentMode = MODE_MANUAL;   // Default mode

const int buttonPins[] = {AUTO, MANUAL, FORWARD, BACKWARD, LEFT, RIGHT};
const char* buttonNames[] = {
  "AUTO",
  "MANUAL",
  "FORWARD",
  "BACKWARD",
  "LEFT",
  "RIGHT"
};

const int buttonCount = 6;

int lastState[buttonCount];
int currentStateBtn[buttonCount];

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < buttonCount; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastState[i] = digitalRead(buttonPins[i]);
  }

  Serial.println("Starting in MANUAL mode");
}

void loop() {

  for (int i = 0; i < buttonCount; i++) {

    currentStateBtn[i] = digitalRead(buttonPins[i]);

    // Detect button press (HIGH → LOW)
    if (lastState[i] == HIGH && currentStateBtn[i] == LOW) {

      // ===== MODE BUTTONS =====
      if (buttonPins[i] == AUTO) {
        currentMode = MODE_AUTO;
        Serial.println("Mode changed to AUTO");
      }

      else if (buttonPins[i] == MANUAL) {
        currentMode = MODE_MANUAL;
        Serial.println("Mode changed to MANUAL");
      }

      // ===== DIRECTION BUTTONS =====
      else {
        if (currentMode == MODE_MANUAL) {
          Serial.print(buttonNames[i]);
          Serial.println(" pressed (MANUAL control active)");
          
          // <-- Put motor control code here
        }
        else {
          // In AUTO mode: ignore direction buttons
          Serial.print(buttonNames[i]);
          Serial.println(" ignored (AUTO mode active)");
        }
      }

      delay(50);  // simple debounce
    }

    lastState[i] = currentStateBtn[i];
  }
}