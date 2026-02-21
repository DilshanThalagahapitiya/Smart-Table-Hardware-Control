/**
 * @file sketch_feb17a.ino
 * @brief Optimized Smart Table Firmware - PRO GRADE
 * Features: Non-blocking Streams, Instant Button Response, Smooth Ramping.
 */

#include <ESP8266WiFi.h>
#include "MotorControl.h"
#include "ButtonHandler.h"
#include "BuzzerController.h"
#include "FirebaseManager.h"
#include "LedEffects.h"

//================= SETTINGS & PINS =================//
const char* ssid = "SLT-Fiber-2.4G_2450"; 
const char* password = "leaf9572";
#define FIREBASE_HOST "smarttable-66d73-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "B4nbM4Xs9f7NTN4JL4iI8hKMnW1iFK5UuwN8kPRy" 

#define BTN_UP    D5
#define BTN_DOWN  D6
#define BTN_STOP  D0
#define IN1       D3
#define IN2       D4
#define LED_UP    D1
#define LED_DOWN  D2
#define FADE_LED  D8  
#define BUZZER    D7  
// const int wifiLed = LED_BUILTIN; // Removed to avoid conflict with IN2 (D4)

//================= GLOBAL STATE =================//
int heightValue = 70;
int targetHeight = 70;
bool isMuted = false;
bool isSync = true;   // New sync control flag
unsigned long lastStatusPush = 0;

//================= COMPONENT INSTANCES =================//
MotorControl tableMotor(IN1, IN2, LED_UP, LED_DOWN, heightValue, 60, 120);
ButtonHandler btnUp(BTN_UP);
ButtonHandler btnDown(BTN_DOWN);
ButtonHandler btnStop(BTN_STOP);
BuzzerController buzzer(BUZZER);
FirebaseManager firebase(FIREBASE_HOST, FIREBASE_AUTH);
LedEffects statusLed(FADE_LED);

//================= FIREBASE CALLBACKS =================//
void streamCallback(FirebaseStream data) {
    if (!isSync && data.dataPath() != "/isSync") return;

    Serial.printf("Firebase update: %s -> %s\n", data.dataPath().c_str(), data.payload().c_str());
    
    if (data.dataPath() == "/isSync") {
        isSync = data.boolData();
        Serial.printf("Sync logic: %s\n", isSync ? "ENABLED" : "DISABLED");
    } else if (data.dataPath() == "/targetHeight") {
        int cloudTarget = data.intData();
        if (cloudTarget != targetHeight) {
            targetHeight = cloudTarget;
            tableMotor.setTarget(targetHeight);
            statusLed.trigger(); // Visual feedback
            buzzer.playPattern(3); // Soft cloud beep
        }
    } else if (data.dataPath() == "/isMuted") {
        isMuted = data.boolData();
        buzzer.setMuted(isMuted);
    } else if (data.dataPath() == "/" && data.dataType() == "json") {
        FirebaseJson &json = data.jsonObject();
        FirebaseJsonData jsonData;
        if (json.get(jsonData, "targetHeight")) {
            int cloudTarget = jsonData.intValue;
            if (cloudTarget != targetHeight) {
                targetHeight = cloudTarget;
                tableMotor.setTarget(targetHeight);
                statusLed.trigger(); // Visual feedback
                buzzer.playPattern(3); // Soft cloud beep
            }
        }
        if (json.get(jsonData, "isMuted")) {
            isMuted = jsonData.boolValue;
            buzzer.setMuted(isMuted);
        }
        if (json.get(jsonData, "isSync")) {
            isSync = jsonData.boolValue;
        }
    }
}

void streamTimeoutCallback(bool timeout) {
    if (timeout) {
        Serial.print("Stream timed out. Reason: ");
        // We can't easily get the reason from inside this specific callback without the fbdo
        // but we can signal the main loop to check health.
        Serial.println("Network instability detected.");
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n######################################");
    Serial.println("!!!       PRO BOARD RESTART        !!!");
    Serial.println("######################################");

    tableMotor.begin();
    btnUp.begin();
    btnDown.begin();
    btnStop.begin();
    buzzer.begin();
    statusLed.begin();
    
    // pinMode(wifiLed, OUTPUT); // Removed to avoid conflict
    // digitalWrite(wifiLed, HIGH);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");
    // digitalWrite(wifiLed, LOW); // Removed to avoid conflict

    firebase.begin();
    
    // Setup stream for real-time app control (Zero lag)
    if (!firebase.setStream("tableControl", streamCallback, streamTimeoutCallback)) {
        Serial.println("Stream setup failed!");
    }

    // Initial Sync: Push local height to Firebase
    firebase.setInt("tableControl/height", heightValue);
    firebase.setString("tableControl/ledStatus", "ACTIVE");
    Serial.println("--- SYSTEM READY ---");
}

void loop() {
    // 0. CONNECTIVITY WATCHDOG (Critical for Cloud Sync)
    static unsigned long lastConnCheck = 0;
    if (millis() - lastConnCheck > 5000) {
        lastConnCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi Lost! Attempting reconnect...");
            WiFi.begin(ssid, password);
        } else if (!firebase.ready()) {
            Serial.println("Firebase not ready. Checking authentication...");
        }
    }

    // 1. UPDATE HARDWARE (Highest Priority)
    tableMotor.update();
    buzzer.update();
    statusLed.update();

    // Link LED: Keep bright while moving, then 1s hold (internal to statusLed)
    if (tableMotor.isMoving()) {
        statusLed.trigger();
    }

    // 2. INSTANT BUTTON RESPONSE
    int upClick = btnUp.getClickType();
    int downClick = btnDown.getClickType();
    int stopClick = btnStop.getClickType();

    // Immediate action on Single Tap
    if (upClick == 1) { 
        Serial.println("BUTTON: UP Single Click");
        targetHeight = min(120, heightValue + 1);
        tableMotor.setTarget(targetHeight);
        buzzer.playPattern(0); 
        statusLed.trigger();
    } else if (upClick == 2) { // Instant Double Tap
        Serial.println("BUTTON: UP Double Click");
        targetHeight = 120;
        tableMotor.setTarget(targetHeight);
        buzzer.playPattern(1);
        statusLed.trigger();
    }
    
    if (downClick == 1) {
        Serial.println("BUTTON: DOWN Single Click");
        targetHeight = max(60, heightValue - 1);
        tableMotor.setTarget(targetHeight);
        buzzer.playPattern(0);
        statusLed.trigger();
    } else if (downClick == 2) {
        Serial.println("BUTTON: DOWN Double Click");
        targetHeight = 60;
        tableMotor.setTarget(targetHeight);
        buzzer.playPattern(1);
        statusLed.trigger();
    }

    if (stopClick > 0) {
        tableMotor.stop();
        targetHeight = heightValue;
        buzzer.playPattern(2);
        statusLed.trigger();
    }

    // 3. RESPONSIVE HOLD (No wait)
    static unsigned long lastHoldCheck = 0;
    if (millis() - lastHoldCheck > 100) { 
        lastHoldCheck = millis();
        if (btnUp.isPressed() && !tableMotor.isMoving()) {
            tableMotor.setTarget(120);
        } else if (btnDown.isPressed() && !tableMotor.isMoving()) {
            tableMotor.setTarget(60);
        }
    }

    // 4. PERIODIC TELEMETRY (Push on change/stop to reduce traffic)
    if (isSync && firebase.ready()) {
        static int lastPushedHeight = -1;
        static int lastPushedTarget = -1; // Track targetHeight for cloud sync
        static unsigned long lastPushTime = 0;
        static bool reportedDiming = false;
        
        // Push targetHeight if it was changed locally
        if (targetHeight != lastPushedTarget) {
            lastPushedTarget = targetHeight;
            firebase.setInt("tableControl/targetHeight", targetHeight);
            reportedDiming = false; // Reset dimming report on new target
            Serial.printf("Telemetry: targetHeight Sync -> %d\n", targetHeight);
        }

        if (tableMotor.isMoving()) {
            if (millis() - lastPushTime > 500) {
                lastPushTime = millis();
                firebase.setInt("tableControl/height", heightValue);
                firebase.setString("tableControl/ledStatus", "MOVING");
                reportedDiming = false;
            }
        } else {
            if (heightValue != lastPushedHeight) {
                lastPushedHeight = heightValue;
                firebase.setInt("tableControl/height", heightValue);
                firebase.setString("tableControl/ledStatus", "ACTIVE");
                reportedDiming = false;
            } else if (statusLed.isDimming() && !reportedDiming) {
                firebase.setString("tableControl/ledStatus", "DIMING");
                reportedDiming = true;
                Serial.println("Telemetry: Status updated to DIMING");
            }
        }
    }
}