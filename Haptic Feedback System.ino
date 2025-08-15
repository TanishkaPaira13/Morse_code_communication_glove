#include <SPI.h>
#include <LoRa.h>

#define FSR_PIN A0
#define MOTOR_PIN 5
#define LORA_SS 10
#define LORA_RST 9
#define LORA_DIO0 2

#define THRESHOLD 400
#define DOT_DURATION 200
#define DASH_DURATION 600
#define GAP_DURATION 800
#define ACK_TIMEOUT 5000
#define MODE 0

String morseBuffer = "";
unsigned long lastTapTime = 0;
bool inMessage = false;

void setup() {
  Serial.begin(9600);
  pinMode(MOTOR_PIN, OUTPUT);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  Serial.println("LoRa init OK");
}

void loop() {
  if (MODE == 0) {
    transmitterLoop();
  } else {
    receiverLoop();
  }
}

void transmitterLoop() {
  int fsrValue = analogRead(FSR_PIN);

  if (fsrValue > THRESHOLD) {
    unsigned long pressStart = millis();
    while (analogRead(FSR_PIN) > THRESHOLD);
    unsigned long pressDuration = millis() - pressStart;

    if (pressDuration < DOT_DURATION) morseBuffer += ".";
    else morseBuffer += "-";

    lastTapTime = millis();
    inMessage = true;
  }

  if (inMessage && millis() - lastTapTime > GAP_DURATION) {
    sendMessage(morseBuffer);
    morseBuffer = "";
    inMessage = false;
  }
}

void sendMessage(String msg) {
  Serial.println("Sending: " + msg);
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();

  unsigned long ackStart = millis();
  bool ackReceived = false;

  while (millis() - ackStart < ACK_TIMEOUT) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String incoming = "";
      while (LoRa.available()) {
        incoming += (char)LoRa.read();
      }
      if (incoming == "ACK") {
        Serial.println("ACK received");
        vibrate(1, 50);
        ackReceived = true;
        break;
      }
    }
  }

  if (!ackReceived) {
    Serial.println("No ACK");
    vibrate(3, 40);
  }
}

void receiverLoop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    Serial.println("Received: " + incoming);

    while (true) {
      vibrate(1, 50);
      delay(200);

      if (analogRead(FSR_PIN) > THRESHOLD) {
        while (analogRead(FSR_PIN) > THRESHOLD);
        break;
      }
    }

    LoRa.beginPacket();
    LoRa.print("ACK");
    LoRa.endPacket();
  }
}

void vibrate(int times, int duration) {
  for (int i = 0; i < times; i++) {
    digitalWrite(MOTOR_PIN, HIGH);
    delay(duration);
    digitalWrite(MOTOR_PIN, LOW);
    delay(100);
  }
}
