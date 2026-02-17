#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

//================= WIFI =================//
const char* ssid = "SLT-Fiber-2.4G_2450"; 
const char* password = "leaf9572";

//================= FIREBASE =================//
#define FIREBASE_HOST "smarttable-66d73-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "YOUR_FIREBASE_DATABASE_SECRET" 

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//================= PINS =================//
#define BTN_UP    D5
#define BTN_DOWN  D6
#define BTN_STOP  D7
#define IN1       D3
#define IN2       D4
#define LED_UP    D1
#define LED_DOWN  D2
#define LED_STOP  D8
const int wifiLed = LED_BUILTIN;

//================= SETTINGS =================//
int heightValue = 70;
int targetHeight = 70;
int lastFirebaseTarget = 70; 
const int maxHeight = 120;
const int minHeight = 60;
String currentCommand = "STOP";

//================= FUNCTIONS =================//

void updateMotorLEDs(String state) {
  digitalWrite(LED_UP, state == "UP");
  digitalWrite(LED_DOWN, state == "DOWN");
  digitalWrite(LED_STOP, state == "STOP");
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  updateMotorLEDs("STOP");
}

void updateHeightFirebase() {
  if (Firebase.ready()) {
    Firebase.RTDB.setInt(&fbdo, "/tableControl/height", heightValue);
  }
}

// Syncs the new targetHeight to Firebase so the Web Dashboard stays updated
void updateTargetInFirebase() {
  if (Firebase.ready()) {
    Firebase.RTDB.setInt(&fbdo, "/tableControl/targetHeight", targetHeight);
    lastFirebaseTarget = targetHeight; // Update local tracker so readFirebase doesn't treat it as a new web update
  }
}

void moveTowardsTarget() {
  if (heightValue == targetHeight) {
    stopMotor();
    return;
  }

  if (heightValue < targetHeight && heightValue < maxHeight) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    heightValue++;
    updateHeightFirebase();
    updateMotorLEDs("UP");
    Serial.printf("Target: %d | Current: %d (UP)\n", targetHeight, heightValue);
  } 
  else if (heightValue > targetHeight && heightValue > minHeight) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    heightValue--;
    updateHeightFirebase();
    updateMotorLEDs("DOWN");
    Serial.printf("Target: %d | Current: %d (DOWN)\n", targetHeight, heightValue);
  }
}

void readFirebase() {
  if (!Firebase.ready()) return;

  // 1. Check for Target Height Change from Dashboard
  if (Firebase.RTDB.getInt(&fbdo, "/tableControl/targetHeight")) {
    int fbTarget = fbdo.intData();
    if (fbTarget != lastFirebaseTarget) {
      targetHeight = fbTarget;
      lastFirebaseTarget = fbTarget;
      Serial.printf("Web Update -> New Target: %d\n", targetHeight);
    }
  }

  // 2. Check for Manual Commands (STOP still works as an emergency override)
  if (Firebase.RTDB.getString(&fbdo, "/tableControl/command")) {
    currentCommand = fbdo.stringData();
    if (currentCommand == "STOP") {
      targetHeight = heightValue;
      updateTargetInFirebase();
      Firebase.RTDB.setString(&fbdo, "/tableControl/command", "AUTO"); 
    }
  }
}

void handleButtons() {
  // UP Button: Increment Target by 1
  if (digitalRead(BTN_UP) == LOW) {
    if (targetHeight < maxHeight) {
      targetHeight++;
      updateTargetInFirebase();
      Serial.printf("Button UP -> Target set to: %d\n", targetHeight);
    }
    delay(200); // Debounce delay
  }

  // DOWN Button: Decrement Target by 1
  if (digitalRead(BTN_DOWN) == LOW) {
    if (targetHeight > minHeight) {
      targetHeight--;
      updateTargetInFirebase();
      Serial.printf("Button DOWN -> Target set to: %d\n", targetHeight);
    }
    delay(200); // Debounce delay
  }

  // STOP Button: Match target to current height immediately
  if (digitalRead(BTN_STOP) == LOW) {
    targetHeight = heightValue;
    updateTargetInFirebase();
    stopMotor();
    delay(200);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP); pinMode(BTN_STOP, INPUT_PULLUP);
  pinMode(LED_UP, OUTPUT); pinMode(LED_DOWN, OUTPUT); pinMode(LED_STOP, OUTPUT);
  pinMode(wifiLed, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(wifiLed, !digitalRead(wifiLed));
  }
  Serial.println("\nConnected!");

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  handleButtons();
  readFirebase();
  moveTowardsTarget();
  delay(100); 
}