#pragma once

#include "Arduino.h"

// --- PwnBeacon Service UUID ---
#define PWNBEACON_SERVICE_UUID        "b34c0000-0000-0000-1337-000000000001"

// --- Protocol constants ---
#define PWNBEACON_PROTOCOL_VERSION  0x01
#define PWNBEACON_ADV_MAX_NAME_LEN  8
#define PWNBEACON_FINGERPRINT_LEN   6
#define PWNBEACON_MAX_PEERS         32

// --- Advertisement flags ---
#define PWNBEACON_FLAG_ADVERTISE    0x01

// --- Advertisement packet (compact binary, max 21 bytes) ---
typedef struct __attribute__((packed)) {
  uint8_t  version;                                // Protocol version (0x01)
  uint8_t  flags;                                  // Bitfield flags
  uint16_t pwnd_run;                               // Pwned this session (LE)
  uint16_t pwnd_tot;                               // Pwned total (LE)
  uint8_t  fingerprint[PWNBEACON_FINGERPRINT_LEN]; // First 6 bytes of identity SHA-256
  uint8_t  name_len;                               // Name length (max 8)
  char     name[PWNBEACON_ADV_MAX_NAME_LEN];       // Name (UTF-8, truncated)
} pwnbeacon_adv;

// --- Peer data ---
typedef struct {
  String    name;
  uint16_t  pwnd_run;
  uint16_t  pwnd_tot;
  int8_t    rssi;
  uint32_t  last_seen;
  bool      gone;
  bool      full_data;
  uint8_t   fingerprint[PWNBEACON_FINGERPRINT_LEN];
} pwnbeacon_peer;

void initPwnbeacon();
esp_err_t pwnbeaconAdvertise(String face);
void pwnbeaconScan(uint16_t duration_ms);
bool isPwnbeaconScanning();
bool pwnbeaconTick(String face);
pwnbeacon_peer* getPwnbeaconPeers();
uint8_t getPwnbeaconRunTotalPeers();
String getPwnbeaconLastFriendName();
signed int getPwnbeaconClosestRssi();
void checkPwnbeaconGonePeers();
