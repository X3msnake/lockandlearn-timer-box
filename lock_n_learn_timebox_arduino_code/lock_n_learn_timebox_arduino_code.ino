/*
 * Timer Box Code
 * Version: 2.2
 * Created: 241209
 * Author: ChatGPT
 *  * Prompt Engineer: X3msnake
 * Repo: https://github.com/X3msnake/lockandlearn-timer-box
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
#include <EEPROM.h>

// EEPROM config
const int lockStateAddress = 0;   // EEPROM address for storing lock state (0 = unlocked, 1 = locked)
const int remainingTimeAddress = 1; // EEPROM address for storing remaining time (in minutes)
const int lockTimeAddress = 2; // EEPROM address to store the lock time (timestamp)

// Pin Configuration
const byte segmentPins[7] = {3, 4, 5, 6, 7, 8, 9}; // A-G segments
const byte digitPins[2] = {10, 11};                // Digit control pins
const byte dotPin = 2;                             // Decimal point (blinking seconds)
const byte switchPin = 13;                         // Lid switch
const byte servoPin = 12;                          // Servo motor

// Constants and Variables
const int defaultLockTime = 1;                     // Default lock time in minutes
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
  
  // Restore lock state and remaining time from EEPROM
  boxLocked = EEPROM.read(lockStateAddress);  // 0 = unlocked, 1 = locked
  remainingTime = EEPROM.read(remainingTimeAddress);

  // If the box was locked previously, initialize the lock

  if (boxLocked) {
    // Load the stored lock time from EEPROM
    long savedLockTimestamp = 0;
    savedLockTimestamp |= EEPROM.read(lockTimeAddress);
    savedLockTimestamp |= (long)EEPROM.read(lockTimeAddress + 1) << 8;
    savedLockTimestamp |= (long)EEPROM.read(lockTimeAddress + 2) << 16;
    savedLockTimestamp |= (long)EEPROM.read(lockTimeAddress + 3) << 24;

    DateTime savedLockTime(savedLockTimestamp);

    // Calculate the time difference between now and when the box was locked
    DateTime now = rtc.now();
    long elapsedSeconds = now.unixtime() - savedLockTime.unixtime(); // Elapsed time in seconds

    // Adjust remaining time based on the elapsed time
    if (remainingTime > 0) {
      remainingTime -= elapsedSeconds / 60;  // Convert elapsed seconds to minutes
    }

    // If remaining time is less than or equal to 0, unlock the box
    if (remainingTime <= 0) {
      unlockBox();
    }
  } else {
    lockServo.write(0);  // Unlock the box if it wasn't locked before
  }

  Serial.println("Timer Box Ready!");
}

void loop() {
  static unsigned long lastSecond = 0;    // For blinking dot every second
  static unsigned long lastMinute = 0;    // For decrementing time every minute
  static unsigned long elapsedTime = 0;   // Track elapsed time in milliseconds
  static bool dotState = false;           // To toggle the dot every second

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

  // Update Time and Display
  if (boxLocked) {
    // Handle blinking of the dot every second (1000 milliseconds)
    if (millis() - lastSecond >= 1000) { 
      lastSecond = millis(); // Update the lastSecond timestamp
      dotState = !dotState;   // Toggle the dot state (on/off)
      digitalWrite(dotPin, dotState);  // Update dot pin
      
      // If it's been a minute, decrement the remaining time
      if (millis() - lastMinute >= 60000) { // Every 60 seconds
        lastMinute = millis();  // Reset the minute timer
        remainingTime--;        // Decrease remaining time by 1 minute

        // Save the updated remaining time to EEPROM
        EEPROM.write(remainingTimeAddress, remainingTime);

        if (remainingTime <= 0) {
          unlockBox();
          Serial.println("Box unlocked!");
        }
      }
    }

    // Refresh the 7-segment display with the remaining time
    refreshDisplay(remainingTime);
  } else {
    refreshDisplay(0); // Display 0 when unlocked
    digitalWrite(dotPin, LOW); // Turn off dot when unlocked
  }
}

// Function to lock the box
void lockBox() {
  boxLocked = true;
  lockServo.write(90); // Rotate servo to lock position
  EEPROM.write(lockStateAddress, 1);  // Save lock state (locked)
  
  // Save the current RTC time when locked
  DateTime now = rtc.now();
  long lockTimestamp = now.unixtime(); // Get Unix timestamp (seconds since 1970)
  
  // Store timestamp in EEPROM (split into four bytes)
  EEPROM.write(lockTimeAddress, lockTimestamp & 0xFF);        // Low byte
  EEPROM.write(lockTimeAddress + 1, (lockTimestamp >> 8) & 0xFF);   // 2nd byte
  EEPROM.write(lockTimeAddress + 2, (lockTimestamp >> 16) & 0xFF);  // 3rd byte
  EEPROM.write(lockTimeAddress + 3, (lockTimestamp >> 24) & 0xFF);  // High byte
}

// Function to unlock the box
void unlockBox() {
  boxLocked = false;
  lockServo.write(0); // Rotate servo to unlock position
  EEPROM.write(lockStateAddress, 0);  // Save lock state (unlocked)
  EEPROM.write(remainingTimeAddress, 0);  // Reset remaining time to 0
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
