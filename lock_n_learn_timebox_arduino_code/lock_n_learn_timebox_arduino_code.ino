/*
 * Timer Box Code
 * Version: 2.12
 * Created: 241209
 * Author: ChatGPT
 * 
 * Features:
 * - Two 7-segment displays to show remaining time (in minutes).
 * - A decimal point blinks every second (pin 2).
 * - Real Time Clock (RTC, DS1307) keeps track of time even if powered off.
 * - A servo motor locks/unlocks the box.
 * - Switch (pin 13) detects if the lid is closed and locks the box.
 * - Lock time can be set via the Serial Console (default: 60 minutes).
 * - Minimum lock time: 1 minute; Maximum lock time: 99 minutes.
 */

#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>

// Pin Configuration
const byte segmentPins[7] = {3, 4, 5, 6, 7, 8, 9}; // A-G segments
const byte digitPins[2] = {10, 11};                // Digit control pins
const byte dotPin = 2;                             // Decimal point (blinking seconds)
const byte switchPin = 13;                         // Lid switch
const byte servoPin = 12;                          // Servo motor

// Constants and Variables
const int defaultLockTime = 15;                    // Default lock time in minutes
int lockTime = defaultLockTime;                    // Current lock time
int remainingTime = 0;                             // Time left in minutes
bool boxLocked = false;                            // Box lock state
bool switchState = true;                           // Current state of the switch
bool lastSwitchState = false;                      // Previous state of the switch
unsigned long lastDebounceTime = 0;                // Last time the switch state changed
const unsigned long debounceDelay = 1000;          // Debounce delay in milliseconds
RTC_DS1307 rtc;                                    // Real Time Clock
Servo lockServo;

const byte numbers[10][7] = { // Number representation for common-anode displays
  {0, 0, 0, 0, 0, 0, 1}, // 0
  {1, 0, 0, 1, 1, 1, 1}, // 1
  {0, 0, 1, 0, 0, 1, 0}, // 2
  {0, 0, 0, 0, 1, 1, 0}, // 3
  {1, 0, 0, 1, 1, 0, 0}, // 4
  {0, 1, 0, 0, 1, 0, 0}, // 5
  {0, 1, 0, 0, 0, 0, 0}, // 6
  {0, 0, 0, 1, 1, 1, 1}, // 7
  {0, 0, 0, 0, 0, 0, 0}, // 8
  {0, 0, 0, 0, 1, 0, 0}  // 9
};

void setup() {
  Serial.begin(9600);

  // Initialize pins
  for (int i = 0; i < 7; i++) pinMode(segmentPins[i], OUTPUT);
  for (int i = 0; i < 2; i++) pinMode(digitPins[i], OUTPUT);
  pinMode(dotPin, OUTPUT);
  pinMode(switchPin, INPUT_PULLUP);

  for (int i = 0; i < 7; i++) digitalWrite(segmentPins[i], HIGH); // Turn off all segments
  for (int i = 0; i < 2; i++) digitalWrite(digitPins[i], LOW); // Turn off digits
  digitalWrite(dotPin, LOW); // Turn off dot

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1);
  }

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set RTC to compile time
  }

  // Initialize servo
  lockServo.attach(servoPin);
  unlockBox();

  Serial.println("Timer Box Ready!");
}

void loop() {
  static unsigned long lastRefresh = 0;
  static unsigned long lastSecond = 0;
  static bool dotState = false;

  // Handle switch debouncing
  bool currentSwitch = digitalRead(switchPin) == LOW; // LOW means pressed
  if (currentSwitch != lastSwitchState) {
    lastDebounceTime = millis(); // Reset debounce timer
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentSwitch != switchState) {
      switchState = currentSwitch;

      if (switchState && !boxLocked) { // If switch is pressed and box is unlocked
        lockBox();
        remainingTime = lockTime;
        Serial.println("Box locked!");
      }
    }
  }
  lastSwitchState = currentSwitch;

  // Handle Serial Input for Lock Time
  
  if (Serial.available() > 0) {
    int input = Serial.parseInt(); // Read number
    if (input >= 1 && input <= 99) {
      lockTime = input;
      Serial.print("Lock time set to: ");
      Serial.print(lockTime);
      Serial.println(" minutes");
    } else {
      Serial.println("Invalid input. Please enter a value between 1 and 99.");
    }
    // Clear the buffer to handle extra characters like \n or \r
    while (Serial.available()) Serial.read();
  }

  // Update Time and Display
  if (boxLocked) {
    if (millis() - lastSecond >= 1000) { // Every second
      lastSecond = millis();
      remainingTime--;
      dotState = !dotState; // Toggle dot
      digitalWrite(dotPin, dotState);

      if (remainingTime <= 0) {
        unlockBox();
        Serial.println("Box unlocked!");
      }
    }
    refreshDisplay(remainingTime);
  } else {
    refreshDisplay(0); // Display 0 when unlocked
    digitalWrite(dotPin, LOW); // Turn off dot
  }
}

// Function to lock the box
void lockBox() {
  boxLocked = true;
  lockServo.write(90); // Rotate servo to lock position
}

// Function to unlock the box
void unlockBox() {
  boxLocked = false;
  lockServo.write(0); // Rotate servo to unlock position
}

// Function to refresh the 7-segment display
void refreshDisplay(int time) {
  int tens = time / 10;
  int ones = time % 10;

  displayDigit(tens, 0);
  displayDigit(ones, 1);
}

// Function to display a digit on a specific position
void displayDigit(int digit, int digitIndex) {
  digitalWrite(digitPins[digitIndex], HIGH); // Activate digit
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPins[i], numbers[digit][i]);
  }
  delay(5); // Persistence of vision
  digitalWrite(digitPins[digitIndex], LOW); // Deactivate digit
}
