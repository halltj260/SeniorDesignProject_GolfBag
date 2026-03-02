/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-button
 */

#define AUTO 21  // GIOP21 pin connected to button
#define MANUAL 22  // GIOP22 pin connected to button
#define FORWARD 32  // GIOP32 pin connected to button
#define BACKWARD 33  // GIOP33 pin connected to button
#define LEFT 26  // GIOP26 pin connected to button
#define RIGHT 27  // GIOP27 pin connected to button


// Variables will change:
int lastState = LOW;  // the previous state from the input pin
int currentState;     // the current reading from the input pin

void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(115200);
  // initialize the pushbutton pin as an pull-up input
  // the pull-up input pin will be HIGH when the switch is open and LOW when the switch is closed.
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // read the state of the switch/button:
  currentState = digitalRead(BUTTON_PIN);

  if (lastState == HIGH && currentState == LOW){
    Serial.println("The button is pressed");
  delay(500);}
  else if (lastState == LOW && currentState == HIGH){
    Serial.println("The button is released");
  delay(500);}

  // save the the last state
  lastState = currentState;
}
