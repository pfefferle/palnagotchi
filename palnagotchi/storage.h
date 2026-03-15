#pragma once

#include "Arduino.h"

#define STORAGE_LOG_RING_SIZE 32

void     initStorage();
bool     isSdAvailable();

// Peer counter (EEPROM)
uint16_t storageGetTotalPeers();
void     storageIncrementTotalPeers();

// Chat log (SD card if available, always in ring buffer)
void     storageLogPeer(const char* name, const char* face,
                        const char* phrase, const char* source);
void     storageLogMessage(const char* from, const char* message);

// Ring buffer access (for web UI)
uint8_t  storageGetLogCount();
String   storageGetLogEntry(uint8_t index);
