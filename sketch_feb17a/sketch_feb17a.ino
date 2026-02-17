#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

//================= WIFI & FIREBASE =================//
const char* ssid = "SLT-Fiber-2.4G_2450"; 
const char* password = "leaf9572";
#define FIREBASE_HOST "smarttable-66d73-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "YOUR_FIREBASE_DATABASE_SECRET" 

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//================= PINS =================//
#define BTN_UP    D5
#define BTN_DOWN  D6
#define BTN_STOP  D0
#define IN1       D3
#define IN2       D4
#define LED_UP    D1
#define LED_DOWN  D2
#define FADE_LED  D8  
#define BUZZER    D7  
const int wifiLed = LED_BUILTIN;

//================= SETTINGS =================//
int motorStepInterval = 60;     
int buttonRepeatDelay = 40;     
int heightValue = 70;
int targetHeight = 70;
int lastFirebaseTarget = 70; 
const int maxHeight = 120;
const int minHeight = 60;

// Buzzer Settings
bool isMuted = false; 
int buzzerPattern = 0; 

String currentLedStatus = "ACTIVE"; 
String lastLedStatus = ""; 

bool needFirebaseSync = false;
unsigned long lastSyncTime = 0;
unsigned long lastMoveTime = 0; 
unsigned long lastMotorStepTime = 0;

//================= TIMER VARIABLES =================//
unsigned long lastBlinkToggle = 0;
bool blinkState = false;
unsigned long lastBuzzerAction = 0;
bool buzzerState = false;
int doubleTapStep = 0;

//================= FAST BUZZER ENGINE =================//

void runBuzzerEngine() {
  bool isMoving = (heightValue != targetHeight);
  if (!isMoving || isMuted) {
    digitalWrite(BUZZER, LOW);
    doubleTapStep = 0;
    return;
  }

  unsigned long now = millis();
  switch (buzzerPattern) {
    case 0: // ULTRA RAPID
      if (now - lastBuzzerAction > 50) {
        buzzerState = !buzzerState;
        digitalWrite(BUZZER, buzzerState);
        lastBuzzerAction = now;
      }
      break;
    case 1: // MEDIUM
      if (now - lastBuzzerAction > 200) {
        buzzerState = !buzzerState;
        digitalWrite(BUZZER, buzzerState);
        lastBuzzerAction = now;
      }
      break;
    case 2: // FAST DOUBLE TAP
      if (doubleTapStep == 0 && now - lastBuzzerAction > 60) { digitalWrite(BUZZER, HIGH); lastBuzzerAction = now; doubleTapStep = 1; }
      else if (doubleTapStep == 1 && now - lastBuzzerAction > 60) { digitalWrite(BUZZER, LOW); lastBuzzerAction = now; doubleTapStep = 2; }
      else if (doubleTapStep == 2 && now - lastBuzzerAction > 60) { digitalWrite(BUZZER, HIGH); lastBuzzerAction = now; doubleTapStep = 3; }
      else if (doubleTapStep == 3 && now - lastBuzzerAction > 60) { digitalWrite(BUZZER, LOW); lastBuzzerAction = now; doubleTapStep = 4; }
      else if (doubleTapStep == 4 && now - lastBuzzerAction > 300) { doubleTapStep = 0; lastBuzzerAction = now; }
      break;
    case 3: // SOLID
      digitalWrite(BUZZER, HIGH);
      break;
  }
}

//================= MOTOR FUNCTIONS =================//

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(LED_UP, LOW);
  digitalWrite(LED_DOWN, LOW);
}

void moveTowardsTarget() {
  if (heightValue == targetHeight) {
    stopMotor();
    return;
  }

  bool canMoveUp = (heightValue < targetHeight && heightValue < maxHeight);
  bool canMoveDown = (heightValue > targetHeight && heightValue > minHeight);

  if (canMoveUp) {
    digitalWrite(LED_UP, HIGH); digitalWrite(LED_DOWN, LOW);
    if (millis() - lastMotorStepTime > (unsigned long)motorStepInterval) { 
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      heightValue++;
      lastMotorStepTime = millis();
    }
  } 
  else if (canMoveDown) {
    digitalWrite(LED_UP, LOW); digitalWrite(LED_DOWN, HIGH);
    if (millis() - lastMotorStepTime > (unsigned long)motorStepInterval) { 
      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      heightValue--;
      lastMotorStepTime = millis();
    }
  } else {
    stopMotor();
    targetHeight = heightValue; 
  }
}

//================= ATMOSPHERE LED LOGIC (20s DELAY) =================//

void updateAtmosphereLED() {
  unsigned long now = millis();
  bool isMoving = (heightValue != targetHeight);

  if (isMoving) {
    lastMoveTime = now; // Constantly reset timer while moving
    currentLedStatus = "ACTIVE";
    if (now - lastBlinkToggle > 100) {
      blinkState = !blinkState;
      analogWrite(FADE_LED, blinkState ? 1023 : 0); 
      lastBlinkToggle = now;
    }
  } 
  else if (now - lastMoveTime < 20000) { // Stay solid for 20 seconds
    currentLedStatus = "ACTIVE";
    analogWrite(FADE_LED, 1023); 
  } 
  else {
    currentLedStatus = "DIMING STARTED"; 
    // Dimming starts after the 20s mark
    float phase = (now - lastMoveTime - 20000) / 1500.0;
    int brightness = (int)(511.5 * (1.0 + sin(PI * phase - (PI / 2.0))));
    analogWrite(FADE_LED, (brightness < 0) ? 0 : brightness);
  }

  if (currentLedStatus != lastLedStatus && Firebase.ready()) {
    Firebase.RTDB.setString(&fbdo, "/tableControl/ledStatus", currentLedStatus);
    lastLedStatus = currentLedStatus;
  }
}

//================= FIREBASE SYNC =================//

void readFirebase() {
  static unsigned long lastRead = 0;
  if (!Firebase.ready() || needFirebaseSync || (millis() - lastRead < 200)) return;
  lastRead = millis();

  if (Firebase.RTDB.getInt(&fbdo, "/tableControl/targetHeight")) {
    int fbTarget = fbdo.intData();
    fbTarget = constrain(fbTarget, minHeight, maxHeight);
    if (fbTarget != lastFirebaseTarget) {
      targetHeight = fbTarget;
      lastFirebaseTarget = fbTarget;
    }
  }
  
  if (Firebase.RTDB.getBool(&fbdo, "/tableControl/isMuted")) {
    isMuted = fbdo.boolData();
  }

  if (Firebase.RTDB.getInt(&fbdo, "/tableControl/buzzerPattern")) {
    buzzerPattern = fbdo.intData();
  }
}

void updateHeightFirebase() {
  static unsigned long lastHeightUpdate = 0;
  if (Firebase.ready() && (millis() - lastHeightUpdate > 800)) { 
    Firebase.RTDB.setInt(&fbdo, "/tableControl/height", heightValue);
    lastHeightUpdate = millis();
  }
}

void syncToFirebase() {
  if (needFirebaseSync && (millis() - lastSyncTime > 300)) {
    if (Firebase.ready()) {
      if (Firebase.RTDB.setInt(&fbdo, "/tableControl/targetHeight", targetHeight)) {
          lastFirebaseTarget = targetHeight;
          needFirebaseSync = false;
      }
    }
  }
}

//================= BUTTONS =================//

void handleButtons() {
  bool pressed = false;
  if (digitalRead(BTN_UP) == LOW) { 
    if (targetHeight < maxHeight) targetHeight++; 
    pressed = true; 
  }
  else if (digitalRead(BTN_DOWN) == LOW) { 
    if (targetHeight > minHeight) targetHeight--; 
    pressed = true; 
  }
  
  if (digitalRead(BTN_STOP) == LOW) {
    targetHeight = heightValue;
    stopMotor();
    pressed = true;
  }

  if (pressed) {
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
  pinMode(LED_UP, OUTPUT); pinMode(LED_DOWN, OUTPUT);
  pinMode(FADE_LED, OUTPUT); pinMode(BUZZER, OUTPUT); 

  analogWriteRange(1023); 
  analogWriteFreq(1000); 

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(100); }

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  handleButtons();
  moveTowardsTarget();
  runBuzzerEngine();      
  updateAtmosphereLED(); 
  updateHeightFirebase(); 
  readFirebase();
  syncToFirebase();
  yield(); 
}