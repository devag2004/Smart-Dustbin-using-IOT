/* Smart Dustbin - ESP32 (Blynk IoT version)
   Components:
   - HC-SR04 (3.3V)
   - IR module (5V)
   - Servo (5V)
*/

#define BLYNK_TEMPLATE_ID "TMPL3LczedOmS"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "Oz6xyNZgYSY22A5YqfNyNpxVDqThhEeW"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>

// WiFi credentials
char ssid[] = "Marla Singer";
char pass[] = "emmastone";

// pins
const int TRIG_PIN = 33;
const int ECHO_PIN = 32;
const int IR_PIN   = 34;
const int SERVO_PIN= 13;


int BIN_EMPTY_DIST_CM = 15;
int BIN_MIN_DIST_CM   = 2;

const int FULL_THRESHOLD_PERCENT = 90;
const int NOTIFY_COOLDOWN_MS = 1000L * 60 * 30;


const int SERVO_OPEN_ANGLE = 130;
const int SERVO_CLOSED_ANGLE = 0;
const unsigned long LID_OPEN_MS = 5000;

Servo lidServo;
BlynkTimer timer;
volatile unsigned long lastNotifyTime = 0;
bool alerted = false;

const int NUM_SAMPLES = 3;


int measureDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return BIN_EMPTY_DIST_CM;
  int distance = (int)((duration * 0.0343) / 2.0);
  if (distance < 0) distance = 0;
  return distance;
}

int readDistanceAvg() {
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += measureDistanceCM();
    delay(60);
  }
  return (int)(sum / NUM_SAMPLES);
}

int distanceToLevelPct(int distance) {
  if (distance >= BIN_EMPTY_DIST_CM) return 0;
  if (distance <= BIN_MIN_DIST_CM) return 100;
  int pct = map(distance, BIN_EMPTY_DIST_CM, BIN_MIN_DIST_CM, 0, 100);
  return constrain(pct, 0, 100);
}


void ultrasonicTask() {
  int distance = readDistanceAvg();
  int level = distanceToLevelPct(distance);

  Blynk.virtualWrite(V0, distance);
  Blynk.virtualWrite(V1, level);
  Serial.printf("[ULTR] dist=%d cm level=%d%%\n", distance, level);

  unsigned long now = millis();
  if (level >= FULL_THRESHOLD_PERCENT) {
    if (!alerted || (now - lastNotifyTime) > NOTIFY_COOLDOWN_MS) {
      Blynk.logEvent("bin_full", "Smart Dustbin: bin near full (" + String(level) + "%)");
      Serial.println("[ULTR] Notification sent.");
      alerted = true;
      lastNotifyTime = now;
    }
  } else if (alerted && level < (FULL_THRESHOLD_PERCENT - 5)) {
    alerted = false;
    Serial.println("[ULTR] Level dropped below threshold, alert reset.");
  }
}


void SMESensor() {
  int ir = digitalRead(IR_PIN);
  if (ir == LOW) {
    Serial.println("[IR] Detected hand - Opening lid.");
    lidServo.write(SERVO_OPEN_ANGLE);
    Blynk.virtualWrite(V2, SERVO_OPEN_ANGLE);

    unsigned long start = millis();
    while (millis() - start < LID_OPEN_MS) {
      ultrasonicTask();
      delay(120);
    }

    lidServo.write(SERVO_CLOSED_ANGLE);
    Blynk.virtualWrite(V2, SERVO_CLOSED_ANGLE);
    Serial.println("[IR] Lid closed.");
    delay(400);
  } else {
    ultrasonicTask();
    delay(200);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  lidServo.setPeriodHertz(50);
  lidServo.attach(SERVO_PIN);
  lidServo.write(SERVO_CLOSED_ANGLE);

  Serial.println("Connecting to WiFi...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Blynk.virtualWrite(V0, 123);
  Blynk.virtualWrite(V1, 45);
  Blynk.virtualWrite(V2, 90);

  Serial.println("Connected!");

  timer.setInterval(1000L, SMESensor);
}

void loop() {
  Blynk.run();
  timer.run();
}
