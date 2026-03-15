#pragma once

#include "Arduino.h"

// --- PwnBeacon Service UUIDs ---
#define PWNBEACON_SERVICE_UUID        "b34c0000-0000-0000-1337-000000000001"
#define PWNBEACON_IDENTITY_CHAR_UUID  "b34c0000-0000-0000-1337-000000000002"
#define PWNBEACON_FACE_CHAR_UUID      "b34c0000-0000-0000-1337-000000000003"
#define PWNBEACON_NAME_CHAR_UUID      "b34c0000-0000-0000-1337-000000000004"
#define PWNBEACON_SIGNAL_CHAR_UUID    "b34c0000-0000-0000-1337-000000000005"
#define PWNBEACON_MESSAGE_CHAR_UUID   "b34c0000-0000-0000-1337-000000000006"

// --- Protocol constants ---
#define PWNBEACON_PROTOCOL_VERSION  0x01
#define PWNBEACON_ADV_MAX_NAME_LEN  8
#define PWNBEACON_FINGERPRINT_LEN   6

// --- Advertisement flags ---
#define PWNBEACON_FLAG_ADVERTISE    0x01
#define PWNBEACON_FLAG_CONNECTABLE  0x02

// --- Advertisement packet (compact binary, max 21 bytes) ---
typedef struct __attribute__((packed)) {
  uint8_t  version;
  uint8_t  flags;
  uint16_t pwnd_run;
  uint16_t pwnd_tot;
  uint8_t  fingerprint[PWNBEACON_FINGERPRINT_LEN];
  uint8_t  name_len;
  char     name[PWNBEACON_ADV_MAX_NAME_LEN];
} pwnbeacon_adv;

void initPwnbeacon();
esp_err_t pwnbeaconAdvertise(String face);
void pwnbeaconScan(uint16_t duration_ms);
bool isPwnbeaconScanning();
bool pwnbeaconTick(String face);
