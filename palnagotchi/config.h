#pragma once

#define PALNAGOTCHI_NAME     "Palnagotchi"

// Derived at boot from BLE MAC, shared across modules
extern char palnagotchi_identity[65];

#define PEER_AWAY_THRESHOLD_MS 120000
