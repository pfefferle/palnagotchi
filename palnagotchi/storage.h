#pragma once

#include "Arduino.h"

#define STORAGE_LOG_RING_SIZE 32
#define STORAGE_MAX_PEERS     64

typedef struct {
  String     name;
  String     face;
  String     identity;
  String     type;       // "wifi" or "ble"
  String     ble_addr;   // BLE address for GATT connections
  signed int rssi;
  bool       gone;
  bool       full_data;  // true after GATT read for BLE peers
} storage_peer;

void     initStorage();
bool     isSdAvailable();

// Unified peer access
storage_peer* storageGetPeers();
uint8_t    storageGetPeerCount();
uint16_t   storageGetTotalPeers();
String     storageGetLastFriendName();
signed int storageGetClosestRssi();

// Peer mutations (called by pwngrid/pwnbeacon)
void     storageAddPeer(const char* name, const char* face,
                        const char* identity, const char* type,
                        signed int rssi, const char* ble_addr = "");
// Persistence
void     storageSavePeers();
void     storageLoadPeers();

// Chat log (SD card if available, always in ring buffer)
void     storageLogPeer(const char* name, const char* face,
                        const char* phrase, const char* source);
void     storageLogMessage(const char* from, const char* message);

// Ring buffer access (for web UI)
uint8_t  storageGetLogCount();
String   storageGetLogEntry(uint8_t index);
