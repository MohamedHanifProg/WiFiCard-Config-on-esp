# WiFiCard-Config-on-esp (RFID Config Storage Demo)

This project demonstrates how to store and update a device configuration string on a **MIFARE Classic RFID card** using an **ESP microcontroller** and the **MFRC522 RFID reader**.  
On every card scan, the sketch reads a configuration payload from specific data blocks, updates a counter field, and writes the updated config back to the card.

---

## What This Homework Does

When you place an RFID card on the reader:

1. **Reads** a config string stored across multiple RFID memory blocks.
2. If no valid config exists (doesn’t start with `CFG:`), it **creates a default config**.
3. If a valid config exists, it **decrements** the JSON field `"counter"` (wraps from `00` to `99`).
4. **Writes** the updated config back to the same blocks.

The config is stored as a **null-terminated C string** split across multiple 16-byte MIFARE blocks (up to **128 bytes** total in this sketch).

---

## Hardware Requirements

- ESP32 / ESP8266 / Arduino (any board supported by the MFRC522 library)
- MFRC522 RFID reader module
- MIFARE Classic RFID card/tag (commonly 1K)
- Jumper wires

---

## Libraries

- `SPI.h`
- `MFRC522.h` (Arduino MFRC522 library)

Install via Arduino IDE:
**Tools → Manage Libraries → search “MFRC522” → install**

---

## Wiring

The sketch uses:
```cpp
#define RST_PIN  4
#define SS_PIN   5
