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
#define FADE_LED  D0  
const int wifiLed = LED_BUILTIN;

//================= ADJUSTABLE SPEED ARGUMENTS =================//
int motorStepInterval = 150;  
int buttonRepeatDelay = 150;  

//================= SETTINGS =================//
int heightValue = 70;
int targetHeight = 70;
int lastFirebaseTarget = 70; 
const int maxHeight = 120;
const int minHeight = 60;

String currentLedStatus = "ACTIVE"; 
String lastLedStatus = ""; 

bool needFirebaseSync = false;
unsigned long lastSyncTime = 0;
unsigned long lastMoveTime = 0; 
unsigned long lastMotorStepTime = 0;

//================= LED LOGIC VARIABLES =================//
unsigned long lastBlinkToggle = 0;
bool blinkState = false;

//================= MOTOR FUNCTIONS =================//

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

void moveTowardsTarget() {
  if (heightValue == targetHeight) {
    stopMotor();
    return;
  }

  if (millis() - lastMotorStepTime > (unsigned long)motorStepInterval) { 
    if (heightValue < targetHeight && heightValue < maxHeight) {
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      heightValue++;
      updateMotorLEDs("UP");
    } 
    else if (heightValue > targetHeight && heightValue > minHeight) {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      heightValue--;
      updateMotorLEDs("DOWN");
    }
    lastMotorStepTime = millis();
  }
}

//================= ATMOSPHERE LED LOGIC =================//

void updateAtmosphereLED() {
  unsigned long now = millis();
  bool isMovingManual = (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW);
  bool isMovingAuto = (heightValue != targetHeight);

  if (isMovingManual || isMovingAuto) {
    lastMoveTime = now; 
    currentLedStatus = "ACTIVE";
    if (now - lastBlinkToggle > 300) { 
      blinkState = !blinkState;
      analogWrite(FADE_LED, blinkState ? 255 : 0);
      lastBlinkToggle = now;
    }
  } 
  else if (now - lastMoveTime < 20000) {
    currentLedStatus = "ACTIVE";
    analogWrite(FADE_LED, 255); 
  } 
  else {
    // START DIMMING
    currentLedStatus = "DIMING STARTED"; 
    float phase = (now - lastMoveTime - 20000) / 2000.0; 
    int brightness = (int)(127.5 * (1.0 + sin(PI * phase - (PI / 2.0))));
    analogWrite(FADE_LED, brightness);
  }

  // Update Firebase string path only when status changes
  if (currentLedStatus != lastLedStatus) {
    if (Firebase.ready()) {
      Firebase.RTDB.setString(&fbdo, "/tableControl/ledStatus", currentLedStatus);
      lastLedStatus = currentLedStatus;
    }
  }
}

//================= FIREBASE SYNC FUNCTIONS =================//

void updateHeightFirebase() {
  static unsigned long lastHeightUpdate = 0;
  if (Firebase.ready() && millis() - lastHeightUpdate > 2000) { 
    Firebase.RTDB.setInt(&fbdo, "/tableControl/height", heightValue);
    lastHeightUpdate = millis();
  }
}

void readFirebase() {
  if (!Firebase.ready() || needFirebaseSync) return;
  if (Firebase.RTDB.getInt(&fbdo, "/tableControl/targetHeight")) {
    int fbTarget = fbdo.intData();
    if (fbTarget != lastFirebaseTarget) {
      targetHeight = fbTarget;
      lastFirebaseTarget = fbTarget;
    }
  }
}

void syncToFirebase() {
  if (needFirebaseSync && (millis() - lastSyncTime > 1000)) {
    if (Firebase.ready()) {
      Firebase.RTDB.setInt(&fbdo, "/tableControl/targetHeight", targetHeight);
      lastFirebaseTarget = targetHeight;
      needFirebaseSync = false;
    }
  }
}

//================= BUTTON HANDLING =================//

void handleButtons() {
  bool anyPressed = false;

  if (digitalRead(BTN_UP) == LOW) {
    if (targetHeight < maxHeight) {
      targetHeight++;
      anyPressed = true;
    }
  }
  else if (digitalRead(BTN_DOWN) == LOW) {
    if (targetHeight > minHeight) {
      targetHeight--;
      anyPressed = true;
    }
  }

  if (digitalRead(BTN_STOP) == LOW) {
    targetHeight = heightValue;
    stopMotor();
    anyPressed = true;
  }

  if (anyPressed) {
    needFirebaseSync = true;
    lastSyncTime = millis();
    delay(buttonRepeatDelay); 
  }
}

//================= SETUP & LOOP =================//

void setup() {
  Serial.begin(115200);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP); pinMode(BTN_STOP, INPUT_PULLUP);
  pinMode(LED_UP, OUTPUT); pinMode(LED_DOWN, OUTPUT); pinMode(LED_STOP, OUTPUT);
  pinMode(FADE_LED, OUTPUT); 
  pinMode(wifiLed, OUTPUT);

  analogWriteRange(255);
  analogWriteFreq(1000); 

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    digitalWrite(wifiLed, !digitalRead(wifiLed));
  }

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  handleButtons();
  moveTowardsTarget();
  updateAtmosphereLED(); 
  updateHeightFirebase(); 
  readFirebase();
  syncToFirebase();
  delay(10); 
}