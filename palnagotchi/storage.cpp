#include "storage.h"
#include "EEPROM.h"
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

static bool sd_available = false;
static uint16_t total_peers_eeprom = 0;

// Unified peer list
static storage_peer peers[STORAGE_MAX_PEERS];
static uint8_t peer_count = 0;
static String last_friend_name = "";

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
  EEPROM.begin(256);
  EEPROM.get(0, total_peers_eeprom);

  #if defined(ARDUINO_M5STACK_CARDPUTER)
    SPI.begin(40, 39, 14, 12);
    sd_available = SD.begin(12);
  #elif defined(ARDUINO_M5STACK_ATOM)
    sd_available = SD.begin(33);
  #elif defined(ARDUINO_M5STACK_ATOMS3)
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

// --- Unified peer access ---

storage_peer* storageGetPeers() {
  return peers;
}

uint8_t storageGetPeerCount() {
  return peer_count;
}

uint16_t storageGetTotalPeers() {
  if (sd_available) {
    return peer_count;
  }
  return total_peers_eeprom;
}

String storageGetLastFriendName() {
  return last_friend_name;
}

void storageSetLastFriendName(const String& name) {
  last_friend_name = name;
}

signed int storageGetClosestRssi() {
  signed int closest = -1000;
  for (uint8_t i = 0; i < peer_count; i++) {
    if (!peers[i].gone && peers[i].rssi > closest) {
      closest = peers[i].rssi;
    }
  }
  return closest;
}

// --- Peer mutations ---

static void incrementEepromCounter() {
  if (sd_available) return;
  total_peers_eeprom++;
  EEPROM.put(0, total_peers_eeprom);
  EEPROM.commit();
}

static int findPeer(const char* identity) {
  for (uint8_t i = 0; i < peer_count; i++) {
    if (peers[i].identity == identity) {
      return i;
    }
  }
  return -1;
}

void storageAddPeer(const char* name, const char* face,
                    const char* identity, const char* type,
                    signed int rssi, const char* ble_addr,
                    uint8_t ble_addr_type) {
  int idx = findPeer(identity);
  if (idx >= 0) {
    peers[idx].rssi = rssi;
    bool was_gone = peers[idx].gone;
    peers[idx].gone = false;
    if (was_gone) {
      peers[idx].full_data = false;
    }
    if (name && strlen(name) > 0 && String(name) != "BLE peer") {
      peers[idx].name = name;
    }
    if (face && strlen(face) > 0) {
      peers[idx].face = face;
    }
    if (ble_addr && strlen(ble_addr) > 0) {
      peers[idx].ble_addr = ble_addr;
      peers[idx].ble_addr_type = ble_addr_type;
    }
    if (strcmp(type, "ble") == 0) {
      peers[idx].type = type;
    }
    return;
  }

  if (peer_count >= STORAGE_MAX_PEERS) return;

  peers[peer_count].name      = name;
  peers[peer_count].face      = face ? face : "";
  peers[peer_count].identity  = identity;
  peers[peer_count].type      = type;
  peers[peer_count].rssi      = rssi;
  peers[peer_count].gone      = false;
  peers[peer_count].full_data = false;
  peers[peer_count].ble_addr  = ble_addr ? ble_addr : "";
  peers[peer_count].ble_addr_type = ble_addr_type;
  last_friend_name = name;
  peer_count++;

  incrementEepromCounter();
  storageLogPeer(name, face, "", type);
  storageSavePeers();
}

// --- Persistence ---

void storageSavePeers() {
  if (!sd_available) return;

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  for (uint8_t i = 0; i < peer_count; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"]     = peers[i].name;
    obj["face"]     = peers[i].face;
    obj["identity"] = peers[i].identity;
    obj["type"]     = peers[i].type;
    obj["rssi"]     = peers[i].rssi;
    if (peers[i].ble_addr.length() > 0) {
      obj["ble_addr"] = peers[i].ble_addr;
      obj["ble_addr_type"] = peers[i].ble_addr_type;
    }
    if (peers[i].full_data) {
      obj["full_data"] = true;
    }
  }

  SD.remove("/palnagotchi/peers.json");
  File f = SD.open("/palnagotchi/peers.json", FILE_WRITE);
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void storageLoadPeers() {
  if (!sd_available) return;
  if (!SD.exists("/palnagotchi/peers.json")) return;

  File f = SD.open("/palnagotchi/peers.json", FILE_READ);
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject obj : arr) {
    if (peer_count >= STORAGE_MAX_PEERS) break;

    peers[peer_count].name      = obj["name"] | "";
    peers[peer_count].face      = obj["face"] | "";
    peers[peer_count].identity  = obj["identity"] | "";
    peers[peer_count].type      = obj["type"] | "";
    peers[peer_count].ble_addr  = obj["ble_addr"] | "";
    peers[peer_count].ble_addr_type = obj["ble_addr_type"] | 0;
    peers[peer_count].rssi      = obj["rssi"] | -100;
    peers[peer_count].gone      = true;
    peers[peer_count].full_data = obj["full_data"] | false;
    peer_count++;
  }
}

// --- Chat log ---

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

  uint8_t pos = (log_head - log_count + index + STORAGE_LOG_RING_SIZE) % STORAGE_LOG_RING_SIZE;
  return log_ring[pos];
}
