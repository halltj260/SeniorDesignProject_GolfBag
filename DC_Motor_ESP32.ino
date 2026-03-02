/*==========================================================================
// Author : Handson Technology
// Project : Arduino Uno
// Description : L298N Motor Driver
// Source-Code : L298N_Motor.ino
// Program: Control 2 DC motors using L298N H Bridge Driver
//==========================================================================
*/
// Definitions Arduino pins connected to input H Bridge
int IN1 = 32;
int IN2 = 33;
int IN3 = 12;
int IN4 = 13;
void setup()
{

  Serial.begin(115200);
// Set the output pins
pinMode(IN1, OUTPUT);
pinMode(IN2, OUTPUT);
pinMode(IN3, OUTPUT);
pinMode(IN4, OUTPUT);
}
void loop()
{
// Rotate the Motor A clockwise
digitalWrite(IN1, HIGH);
digitalWrite(IN2, LOW);
// Rotate the Motor B clockwise
digitalWrite(IN3, HIGH);
digitalWrite(IN4, LOW);
Serial.println("Clockwise");
delay(2000);
// Motor A
digitalWrite(IN1, HIGH);
digitalWrite(IN2, HIGH);
// Motor B
digitalWrite(IN3, HIGH);
digitalWrite(IN4, HIGH);
// delay(500);
// Rotates the Motor A counter-clockwise
digitalWrite(IN1, LOW);
digitalWrite(IN2, HIGH);
// Rotates the Motor B counter-clockwise
digitalWrite(IN3, LOW);
digitalWrite(IN4, HIGH);
Serial.println("Counter-Clockwise");
delay(2000);
// Motor A
digitalWrite(IN1, HIGH);
digitalWrite(IN2, HIGH);
// Motor B
digitalWrite(IN3, HIGH);
digitalWrite(IN4, HIGH);
// delay(500);
}

