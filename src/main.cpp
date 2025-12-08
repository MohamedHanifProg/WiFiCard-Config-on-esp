#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN  4      // Your wiring
#define SS_PIN   5      // Your wiring

MFRC522 mfrc522(SS_PIN, RST_PIN);

// Default factory key for MIFARE cards (FF FF FF FF FF FF)
MFRC522::MIFARE_Key key;

// We will use these blocks to store the config (avoid trailer blocks!)
// Use only blocks in sectors with default keys (32+), skipping trailer blocks
const byte dataBlocks[] = {32, 33, 34, 36, 37, 38, 40, 41};

const byte NUM_BLOCKS = sizeof(dataBlocks) / sizeof(dataBlocks[0]);
const size_t CONFIG_MAX_LEN = NUM_BLOCKS * 16; // 128 bytes total

// Buffer for config string
char config[CONFIG_MAX_LEN];

// ---------- Helper: authenticate a block ----------
bool authBlock(byte blockAddr) {
  MFRC522::StatusCode status;
  status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A,
      blockAddr,
      &key,
      &(mfrc522.uid)
  );
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed for block "));
    Serial.print(blockAddr);
    Serial.print(F(": "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}

// ---------- Helper: read config from card into `config` buffer ----------
bool readConfigFromCard() {
  size_t offset = 0;
  config[0] = '\0';

  for (byte i = 0; i < NUM_BLOCKS; i++) {
    byte blockAddr = dataBlocks[i];

    if (!authBlock(blockAddr)) {
      return false;
    }

    byte buffer[18];     // 16 data bytes + 2 CRC
    byte size = sizeof(buffer);

    MFRC522::StatusCode status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Read() failed for block "));
      Serial.print(blockAddr);
      Serial.print(F(": "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
    }

    // Copy 16 data bytes into config buffer
    for (byte j = 0; j < 16; j++) {
      if (offset < CONFIG_MAX_LEN - 1) { // keep space for '\0'
        config[offset] = buffer[j];
        if (buffer[j] == '\0') {
          config[offset] = '\0';
          return true; // we reached end of stored string
        }
        offset++;
      }
    }
  }

  // Ensure string termination
  config[offset] = '\0';
  return true;
}

// ---------- Helper: write config string from `config` buffer to card ----------
bool writeConfigToCard() {
  size_t len = strlen(config) + 1; // include '\0'
  if (len > CONFIG_MAX_LEN) {
    Serial.println(F("Config too long to write to card!"));
    return false;
  }

  size_t offset = 0;

  for (byte i = 0; i < NUM_BLOCKS; i++) {
    byte blockAddr = dataBlocks[i];

    if (!authBlock(blockAddr)) {
      return false;
    }

    byte buffer[16];
    for (byte j = 0; j < 16; j++) {
      if (offset < len) {
        buffer[j] = config[offset++];
      } else {
        buffer[j] = 0x00; // pad rest with zeros
      }
    }

    MFRC522::StatusCode status = mfrc522.MIFARE_Write(blockAddr, buffer, 16);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("MIFARE_Write() failed for block "));
      Serial.print(blockAddr);
      Serial.print(F(": "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return false;
    }
  }

  return true;
}

// ---------- Helper: initialize default config if none exists ----------
void initDefaultConfig() {
  // counter = 10; ensure long string (>96 bytes)
  snprintf(
    config,
    CONFIG_MAX_LEN,
    "CFG:{\"name\":\"Device1\",\"counter\":%02d,\"mode\":\"demo\",\"padding\":\"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\"}",
    10
  );
}

// ---------- Helper: decrement counter field in config string ----------
void updateCounterInConfig() {
  // Look for "counter":
  char *p = strstr(config, "\"counter\":");
  if (!p) {
    Serial.println(F("No \"counter\" field found, keeping config as is."));
    return;
  }

  p += strlen("\"counter\":"); // move to the number
  while (*p == ' ') p++;       // skip spaces just in case

  int value = atoi(p);         // read current value
  value--;                     // decrement
  if (value < 0) {
    value = 99;                // wrap to 99 if negative
  }

  // We will always store with two digits: 00–99
  char numBuf[4];
  snprintf(numBuf, sizeof(numBuf), "%02d", value);

  // Overwrite the first two characters of the old number
  p[0] = numBuf[0];
  p[1] = numBuf[1];
  // (If there were more digits before, they'll remain, but we never set >99)
}

// ---------- Arduino setup ----------
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial
  }

  SPI.begin();
  mfrc522.PCD_Init();

  // Initialize default key (FF FF FF FF FF FF)
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.println(F("RFID CONFIG demo"));
  mfrc522.PCD_DumpVersionToSerial();
  Serial.println(F("Place a card on the reader..."));
}

// ---------- Arduino loop ----------
void loop() {
  // Wait for a new card
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println(F("\n--- New card detected ---"));

  // Read existing config
  if (!readConfigFromCard()) {
    Serial.println(F("Failed to read config from card."));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }

  if (strncmp(config, "CFG:", 4) != 0) {
    Serial.println(F("No valid config on card. Initializing default config..."));
    initDefaultConfig();
  } else {
    Serial.println(F("Existing config on card:"));
    Serial.println(config);

    // Update counter
    updateCounterInConfig();
  }

  Serial.println(F("New config to write:"));
  Serial.println(config);

  // Write back to card
  if (!writeConfigToCard()) {
    Serial.println(F("Failed to write new config to card!"));
  } else {
    Serial.println(F("Config written successfully."));
  }

  // Halt the PICC and stop encryption
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  Serial.println(F("Remove card and place again for another update.\n"));
}
