const int fsrPin = A0;
const int threshold = 300;

const int calibrationTapsRequired = 12;
unsigned long tapDurations[calibrationTapsRequired];
int tapIndex = 0;

bool isTapping = false;
unsigned long tapStartTime = 0;
unsigned long lastTapEnd = 0;
unsigned long dotAvg;

bool calibrated = false;

String morseBuffer = "";
String fullMorseSentence = "";

void setup() {
  Serial.begin(115200);
  Serial.println("CALIBRATION MODE: Apply pressure in Morse pattern 'TEST OK'.");
  Serial.println("Expected: - . ... -   --- -.-");
}

void loop() {
  int val = analogRead(fsrPin);
  unsigned long currentTime = millis();

  if (!calibrated) {
    if (!isTapping && val > threshold) {
      isTapping = true;
      tapStartTime = currentTime;
    }

    if (isTapping && val <= threshold) {
      unsigned long duration = currentTime - tapStartTime;
      if (duration >= 30) {
        tapDurations[tapIndex++] = duration;
        Serial.printf("Tap %d: %lu ms\n", tapIndex, duration);
      } else {
        Serial.printf("Ignored noise tap: %lu ms\n", duration);
      }
      isTapping = false;
    }

    if (tapIndex >= calibrationTapsRequired) {
      calibrate();
      calibrated = true;
      Serial.println("Calibration complete. Start pressing in Morse input:");
    }
    return;
  }

  if (!isTapping && val > threshold) {
    isTapping = true;
    tapStartTime = currentTime;

    if (lastTapEnd != 0) {
      unsigned long gap = currentTime - lastTapEnd;

      if (gap > dotAvg * 6) {
        printMorse(morseBuffer);
        fullMorseSentence += morseBuffer + " / ";
        morseBuffer = "";
      } else if (gap > dotAvg * 3) {
        printMorse(morseBuffer);
        fullMorseSentence += morseBuffer + " ";
        morseBuffer = "";
      }
    }
  }

  if (isTapping && val <= threshold) {
    unsigned long duration = currentTime - tapStartTime;
    lastTapEnd = currentTime;

    if (duration < 30) {
      Serial.printf("Ignored noise tap: %lu ms\n", duration);
      isTapping = false;
      return;
    }

    if (duration < dotAvg * 1.25) {
      morseBuffer += ".";
    } else {
      morseBuffer += "-";
    }

    Serial.printf("Tap Detected: %lu ms → %c\n", duration, morseBuffer[morseBuffer.length() - 1]);
    isTapping = false;
  }

  if (calibrated && lastTapEnd != 0 && (millis() - lastTapEnd > 5000)) {
    if (morseBuffer.length() > 0) {
      printMorse(morseBuffer);
      fullMorseSentence += morseBuffer;
      morseBuffer = "";
    }

    if (fullMorseSentence.length() > 0) {
      Serial.println("=== FULL MORSE SENTENCE ===");
      Serial.println(fullMorseSentence);
      Serial.println("===========================");
      fullMorseSentence = "";
      lastTapEnd = 0;
    }
  }
}

void calibrate() {
  for (int i = 0; i < calibrationTapsRequired - 1; i++) {
    for (int j = i + 1; j < calibrationTapsRequired; j++) {
      if (tapDurations[i] > tapDurations[j]) {
        unsigned long temp = tapDurations[i];
        tapDurations[i] = tapDurations[j];
        tapDurations[j] = temp;
      }
    }
  }

  unsigned long dotSum = 0;
  for (int i = 0; i < 6; i++) {
    dotSum += tapDurations[i];
  }

  dotAvg = dotSum / 6;

  Serial.printf("Calibration Results:\nAverage Dot Duration: %lu ms\nThreshold: Dot < %.2f ms\n", dotAvg, dotAvg * 1.25);
}

void printMorse(String code) {
  if (code.length() == 0) return;
  Serial.printf("Morse: %s\n", code.c_str());
}
