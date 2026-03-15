#include "storage.h"
#include "EEPROM.h"
#include <SD.h>
#include <SPI.h>

static bool sd_available = false;
static uint16_t total_peers = 0;

// Ring buffer for recent log entries
static String log_ring[STORAGE_LOG_RING_SIZE];
static uint8_t log_head = 0;
static uint8_t log_count = 0;

static void addToRing(const String& entry) {
  log_ring[log_head] = entry;
  log_head = (log_head + 1) % STORAGE_LOG_RING_SIZE;
  if (log_count < STORAGE_LOG_RING_SIZE) {
    log_count++;
  }
}

static String formatUptime() {
  unsigned long secs = millis() / 1000;
  int h = secs / 3600;
  int m = (secs % 3600) / 60;
  int s = secs % 60;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
  return String(buf);
}

static void writeToSd(const String& line) {
  if (!sd_available) return;

  File f = SD.open("/palnagotchi/chat.log", FILE_APPEND);
  if (f) {
    f.println(line);
    f.close();
  }
}

void initStorage() {
  // EEPROM
  EEPROM.begin(256);
  EEPROM.get(0, total_peers);

  // SD card auto-detection
  #if defined(ARDUINO_M5STACK_CARDPUTER)
    // Cardputer + Cardputer Adv — built-in SD slot
    SPI.begin(40, 39, 14, 12);
    sd_available = SD.begin(12);
  #elif defined(ARDUINO_M5STACK_ATOM)
    // Atomic TF Card Reader (ESP32-PICO)
    sd_available = SD.begin(33);
  #elif defined(ARDUINO_M5STACK_ATOMS3)
    // Atomic TF Card Reader (ESP32-S3) — needs HW verification
    SPI.begin(17, 8, 21, 15);
    sd_available = SD.begin(15);
  #endif

  if (sd_available) {
    if (!SD.exists("/palnagotchi")) {
      SD.mkdir("/palnagotchi");
    }
  }
}

bool isSdAvailable() {
  return sd_available;
}

uint16_t storageGetTotalPeers() {
  return total_peers;
}

void storageIncrementTotalPeers() {
  total_peers++;
  EEPROM.put(0, total_peers);
  EEPROM.commit();
}

void storageLogPeer(const char* name, const char* face,
                    const char* phrase, const char* source) {
  String entry = "[" + formatUptime() + "] " + source + " " + name;
  if (face && strlen(face) > 0) {
    entry += " - " + String(face);
  }
  if (phrase && strlen(phrase) > 0) {
    entry += " - \"" + String(phrase) + "\"";
  }

  addToRing(entry);
  writeToSd(entry);
}

void storageLogMessage(const char* from, const char* message) {
  String entry = "[" + formatUptime() + "] MSG " + from + ": " + message;

  addToRing(entry);
  writeToSd(entry);
}

uint8_t storageGetLogCount() {
  return log_count;
}

String storageGetLogEntry(uint8_t index) {
  if (index >= log_count) return "";

  // Ring buffer: oldest entry is at (head - count), newest at (head - 1)
  uint8_t pos = (log_head - log_count + index + STORAGE_LOG_RING_SIZE) % STORAGE_LOG_RING_SIZE;
  return log_ring[pos];
}
