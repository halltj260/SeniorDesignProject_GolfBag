/*
  ESP8266 Blink by Simon Peter
  Blink the blue LED on the ESP-01 module
  This example code is in the public domain

  The blue LED on the ESP-01 module is connected to GPIO1
  (which is also the TXD pin; so we cannot use Serial.print() at the same time)

  Note that this sketch uses LED_BUILTIN to find the pin with the internal LED
*/
// #define LED D2
// void setup() {
//   pinMode(LED, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
// }

// // the loop function runs over and over again forever
// void loop() {
//   digitalWrite(LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
//   // but actually the LED is on; this is because
//   // it is active low on the ESP-01)

//   delay(2000);                      // Wait for a second
//   digitalWrite(LED, HIGH);  // Turn the LED off by making the voltage HIGH
//   delay(2000);                      // Wait for two seconds (to demonstrate the active low LED)
// }


#define LED_BUILTIN D2

void setup() {
  // put your setup code here, to run once:
  Serial.begin (115200);
  Serial.println ("Blink with ESP32");

  pinMode (LED_BUILTIN, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite (LED_BUILTIN, HIGH);
  delay (500);

  digitalWrite (LED_BUILTIN, LOW);
  delay (500);
}


