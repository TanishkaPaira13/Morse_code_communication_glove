#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>           // LoRa by Sandeep Mistry
#include <AESLib.h>         // AESLib by Matej Sychra
#include <SHA256.h>         // Crypto by Rhys

// --- Pin configuration (change if needed) ---
#if defined(ESP8266)
  #define LORA_SS   D8 //chip select or slave select (active low)
  #define LORA_RST  D1 //reset pin
  #define LORA_DIO0 D2 //event signal pin (active high)

#else
  #error "Board not supported"
#endif

const long LORA_FREQ = 433E6;  // LoRa frequency = 433MHz

// Keys (can be changed later)
const uint8_t AES_KEY[16] = {
  0x3A,0x5C,0x01,0x9F,0x22,0x44,0xAB,0xCD,
  0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88
};

const uint8_t HMAC_KEY[16] = {
  0xAA,0xBB,0xCC,0xDD,0x01,0x02,0x03,0x04,
  0x05,0x06,0x07,0x08,0x09,0x10,0x11,0x12
};

// Buffers
char inputBuffer[128];    // change to uint8_t if the input is binary
volatile bool inputReady = false; //volatile because it can change unexpectedly due to interrupts

char outputBuffer[128];   // Decrypted output holder
volatile bool outputReady = false;

uint8_t packetBuffer[512];
int packetLen = 0;

// Crypto objects
AESLib aesLib;
SHA256 sha256;

// IV buffer for AES
uint8_t iv[16];

// generate random IV 
void genIV(uint8_t* out, int len) { //generates initialization vector
  for (int i = 0; i < len; i++) {
    out[i] = random(0, 256);
  }
}

// --- AES Encrypt ---
int aes_encrypt(const uint8_t* plain, int plainLen, uint8_t* outCipher, const uint8_t* key, const uint8_t* iv_in) {
  byte pbuf[256];
  byte ivbuf[16];
  for (int i = 0; i < plainLen; i++) pbuf[i] = plain[i];
  for (int i = 0; i < 16; i++) ivbuf[i] = iv_in[i];
  
  return aesLib.encrypt(pbuf, plainLen, outCipher, key, 128, ivbuf);
}

// --- AES Decrypt ---
int aes_decrypt(const uint8_t* cipher, int cipherLen, uint8_t* outPlain, const uint8_t* key, const uint8_t* iv_in) {
  byte cbuf[256];
  byte ivbuf[16];
  for (int i = 0; i < cipherLen; i++) cbuf[i] = cipher[i];
  for (int i = 0; i < 16; i++) ivbuf[i] = iv_in[i];
  
  return aesLib.decrypt(cbuf, cipherLen, outPlain, key, 128, ivbuf);
}

// --- Compute HMAC-SHA256 ---
void compute_hmac_sha256(const uint8_t* data, int dataLen, const uint8_t* key, int keyLen, uint8_t out[32]) {
  sha256.resetHMAC(key, keyLen);
  sha256.update(data, dataLen);
  sha256.finalizeHMAC(key, keyLen, out, 32);
}

// --- Packet format ---
// [16B IV][2B cipherLen][ciphertext][32B HMAC]

int buildEncryptedPacket(const uint8_t* plain, int plainLen, uint8_t* outPacket) {
  genIV(iv, 16);
  int cipherLen = aes_encrypt(plain, plainLen, outPacket + 18, AES_KEY, iv);
  memcpy(outPacket, iv, 16);
  outPacket[16] = (cipherLen >> 8) & 0xFF;
  outPacket[17] = cipherLen & 0xFF;

  // Calculate HMAC on IV + cipherLen + ciphertext
  compute_hmac_sha256(outPacket, 18 + cipherLen, HMAC_KEY, sizeof(HMAC_KEY), outPacket + 18 + cipherLen);

  return 18 + cipherLen + 32;
}

bool processReceivedPacket(const uint8_t* pkt, int pktLen, char* outPlainStr, int maxLen) {
  if (pktLen < 50) return false; // minimal packet length check

  uint8_t recvIV[16];
  memcpy(recvIV, pkt, 16);

  int cipherLen = (pkt[16] << 8) | pkt[17];
  if (cipherLen <= 0 || cipherLen > pktLen - 50) return false;

  const uint8_t* cipher = pkt + 18;
  const uint8_t* recvHmac = pkt + 18 + cipherLen;

  // Verify HMAC
  uint8_t calcHmac[32];
  compute_hmac_sha256(pkt, 18 + cipherLen, HMAC_KEY, sizeof(HMAC_KEY), calcHmac);
  for (int i = 0; i < 32; i++) {
    if (calcHmac[i] != recvHmac[i]) return false;
  }

  // Decrypt
  uint8_t plain[256];
  int plainLen = aes_decrypt(cipher, cipherLen, plain, AES_KEY, recvIV);
  if (plainLen <= 0) return false;

  int copyLen = (plainLen < maxLen - 1) ? plainLen : maxLen - 1;
  memcpy(outPlainStr, plain, copyLen);
  outPlainStr[copyLen] = 0;
  return true;
}

// --- LoRa functions ---
void loraInit() {
  SPI.begin();
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa init failed! Check wiring/frequency.");
    while (1) delay(1000);
  }
  Serial.print("LoRa started at ");
  Serial.println(LORA_FREQ);
}

void loraSend(const uint8_t* data, int len) {
  LoRa.beginPacket();
  LoRa.write(data, len);
  LoRa.endPacket();
}

int loraReceive(uint8_t* buf, int maxLen, unsigned long timeoutMs = 500) {
  unsigned long start = millis();
  LoRa.receive();
  while (millis() - start < timeoutMs) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      int idx = 0;
      while (LoRa.available() && idx < maxLen) {
        buf[idx++] = LoRa.read();
      }
      LoRa.idle();
      return idx;
    }
  }
  LoRa.idle();
  return 0;
}

// === Teammate API ===
// Called when teammate has Morse input ready
void onMorseInputReady(const char* morseStr) {
  noInterrupts();
  strncpy(inputBuffer, morseStr, sizeof(inputBuffer) - 1);
  inputBuffer[sizeof(inputBuffer) - 1] = 0;
  inputReady = true;
  interrupts();
}

// Teammate calls this to get decrypted Morse output for ERM vibration
bool getDecryptedOutput(char* buf, int maxLen) {
  if (!outputReady) return false;
  strncpy(buf, outputBuffer, maxLen - 1);
  buf[maxLen - 1] = 0;
  outputReady = false;
  return true;
}

// --- Main program ---
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  loraInit();
  Serial.println("Ready");
}

void loop() {
  // 1) Send encrypted message if input ready
  if (inputReady) {
    noInterrupts();
    inputReady = false;
    interrupts();

    int plainLen = strlen(inputBuffer);
    packetLen = buildEncryptedPacket((uint8_t*)inputBuffer, plainLen, packetBuffer);

    Serial.print("Sending encrypted: ");
    Serial.println(inputBuffer);
    loraSend(packetBuffer, packetLen);
  }

  // 2) Receive & decrypt message
  uint8_t recvBuf[512];
  int rcvLen = loraReceive(recvBuf, sizeof(recvBuf), 1000);
  if (rcvLen > 0) {
    if (processReceivedPacket(recvBuf, rcvLen, outputBuffer, sizeof(outputBuffer))) {
      outputReady = true;
      Serial.print("Received & decrypted: ");
      Serial.println(outputBuffer);
    } else {
      Serial.println("Packet verification failed");
    }
  }

  delay(20);
}
