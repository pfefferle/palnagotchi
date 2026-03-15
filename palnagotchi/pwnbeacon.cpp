#include "pwnbeacon.h"
#include <NimBLEDevice.h>
#include <mbedtls/sha256.h>

uint8_t pwnbeacon_friends_run = 0;
pwnbeacon_peer pwnbeacon_peers[PWNBEACON_MAX_PEERS];
String pwnbeacon_last_friend_name = "";

uint8_t getPwnbeaconRunTotalPeers() { return pwnbeacon_friends_run; }
String getPwnbeaconLastFriendName() { return pwnbeacon_last_friend_name; }
pwnbeacon_peer *getPwnbeaconPeers() { return pwnbeacon_peers; }

char pwnbeacon_name[PWNBEACON_ADV_MAX_NAME_LEN + 1] = {0};
char pwnbeacon_identity[129] = {0};
uint16_t pwnbeacon_pwnd_run = 0;
uint16_t pwnbeacon_pwnd_tot = 0;
uint8_t pwnbeacon_fingerprint[PWNBEACON_FINGERPRINT_LEN] = {0};

NimBLEAdvertising *ble_advertising = nullptr;
bool ble_scanning = false;

void computeFingerprint(const char *identity, uint8_t *out) {
  uint8_t hash[32];
  mbedtls_sha256((const unsigned char *)identity, strlen(identity), hash, 0);
  memcpy(out, hash, PWNBEACON_FINGERPRINT_LEN);
}

void buildAdvPayload(uint8_t *buf, size_t *len) {
  pwnbeacon_adv adv;
  memset(&adv, 0, sizeof(adv));

  adv.version = PWNBEACON_PROTOCOL_VERSION;
  adv.flags = PWNBEACON_FLAG_ADVERTISE;
  adv.pwnd_run = pwnbeacon_pwnd_run;
  adv.pwnd_tot = pwnbeacon_pwnd_tot;
  memcpy(adv.fingerprint, pwnbeacon_fingerprint, PWNBEACON_FINGERPRINT_LEN);

  size_t name_len = strlen(pwnbeacon_name);
  if (name_len > PWNBEACON_ADV_MAX_NAME_LEN) {
    name_len = PWNBEACON_ADV_MAX_NAME_LEN;
  }
  adv.name_len = (uint8_t)name_len;
  memcpy(adv.name, pwnbeacon_name, name_len);

  *len = offsetof(pwnbeacon_adv, name) + name_len;
  memcpy(buf, &adv, *len);
}

int findPeerByFingerprint(const uint8_t *fp) {
  for (uint8_t i = 0; i < pwnbeacon_friends_run; i++) {
    if (memcmp(pwnbeacon_peers[i].fingerprint, fp, PWNBEACON_FINGERPRINT_LEN) == 0) {
      return i;
    }
  }
  return -1;
}

void pwnbeaconAddPeer(const uint8_t *data, size_t len, int8_t rssi,
                      const char *ble_name = nullptr) {
  // Need at least version + flags + pwnd counts + 4 bytes of fingerprint
  if (len < 10) {
    return;
  }

  pwnbeacon_adv adv;
  memset(&adv, 0, sizeof(adv));
  size_t copy_len = len < sizeof(adv) ? len : sizeof(adv);
  memcpy(&adv, data, copy_len);

  if (adv.version != PWNBEACON_PROTOCOL_VERSION) {
    return;
  }

  if (memcmp(adv.fingerprint, pwnbeacon_fingerprint, PWNBEACON_FINGERPRINT_LEN) == 0) {
    return;
  }

  int idx = findPeerByFingerprint(adv.fingerprint);

  if (idx >= 0) {
    pwnbeacon_peers[idx].rssi = rssi;
    pwnbeacon_peers[idx].last_seen = millis();
    pwnbeacon_peers[idx].gone = false;
    pwnbeacon_peers[idx].pwnd_run = adv.pwnd_run;
    pwnbeacon_peers[idx].pwnd_tot = adv.pwnd_tot;
    return;
  }

  if (pwnbeacon_friends_run >= PWNBEACON_MAX_PEERS) {
    return;
  }

  uint8_t name_len = adv.name_len;
  if (name_len > PWNBEACON_ADV_MAX_NAME_LEN) {
    name_len = PWNBEACON_ADV_MAX_NAME_LEN;
  }

  memset(&pwnbeacon_peers[pwnbeacon_friends_run], 0, sizeof(pwnbeacon_peer));
  if (name_len > 0) {
    pwnbeacon_peers[pwnbeacon_friends_run].name = String(adv.name).substring(0, name_len);
  } else if (ble_name && strlen(ble_name) > 0) {
    pwnbeacon_peers[pwnbeacon_friends_run].name = String(ble_name);
  } else {
    pwnbeacon_peers[pwnbeacon_friends_run].name = "BLE peer";
  }
  pwnbeacon_peers[pwnbeacon_friends_run].pwnd_run = adv.pwnd_run;
  pwnbeacon_peers[pwnbeacon_friends_run].pwnd_tot = adv.pwnd_tot;
  pwnbeacon_peers[pwnbeacon_friends_run].rssi = rssi;
  pwnbeacon_peers[pwnbeacon_friends_run].last_seen = millis();
  pwnbeacon_peers[pwnbeacon_friends_run].gone = false;
  pwnbeacon_peers[pwnbeacon_friends_run].full_data = false;
  memcpy(pwnbeacon_peers[pwnbeacon_friends_run].fingerprint, adv.fingerprint,
         PWNBEACON_FINGERPRINT_LEN);

  pwnbeacon_last_friend_name = pwnbeacon_peers[pwnbeacon_friends_run].name;
  pwnbeacon_friends_run++;
}

class PwnBeaconScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
    // Check service UUID in advertisement
    if (advertisedDevice->haveServiceUUID() &&
        advertisedDevice->isAdvertisingService(NimBLEUUID(PWNBEACON_SERVICE_UUID))) {
    }

    // Check service data UUID
    int svcDataCount = advertisedDevice->getServiceDataCount();
    for (int i = 0; i < svcDataCount; i++) {
      if (advertisedDevice->getServiceDataUUID(i).equals(NimBLEUUID(PWNBEACON_SERVICE_UUID))) {
        std::string svcData = advertisedDevice->getServiceData(i);
        std::string devName = advertisedDevice->getName();
        pwnbeaconAddPeer((const uint8_t *)svcData.data(), svcData.length(),
                         advertisedDevice->getRSSI(), devName.c_str());
        break;
      }
    }
  }

  void onScanEnd(const NimBLEScanResults &results, int reason) override {
    ble_scanning = false;
  }
};

static PwnBeaconScanCallbacks scanCallbacks;

void initPwnbeacon() {
  strncpy(pwnbeacon_name, "Palnagot", PWNBEACON_ADV_MAX_NAME_LEN);
  pwnbeacon_name[PWNBEACON_ADV_MAX_NAME_LEN] = '\0';
  strncpy(pwnbeacon_identity,
          "32e9f315e92d974342c93d0fd952a914bfb4e6838953536ea6f63d54db6b9610",
          sizeof(pwnbeacon_identity) - 1);
  computeFingerprint(pwnbeacon_identity, pwnbeacon_fingerprint);

  NimBLEDevice::init("Palnagotchi");

  ble_advertising = NimBLEDevice::getAdvertising();
  ble_advertising->addServiceUUID(NimBLEUUID(PWNBEACON_SERVICE_UUID));
  ble_advertising->setMinInterval(0x20);
  ble_advertising->setMaxInterval(0x40);
}

esp_err_t pwnbeaconAdvertise(String face) {
  uint8_t adv_payload[sizeof(pwnbeacon_adv)];
  size_t adv_payload_len = 0;
  buildAdvPayload(adv_payload, &adv_payload_len);

  // BLE scan response: 31 bytes max, 128-bit UUID takes 18 bytes overhead,
  // leaving 13 for data. Use 10 to stay safely within limits.
  if (adv_payload_len > 10) {
    adv_payload_len = 10;
  }

  // Main advertisement: flags + service UUID
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.setCompleteServices(NimBLEUUID(PWNBEACON_SERVICE_UUID));

  // Scan response: service data with payload
  NimBLEAdvertisementData scanResp;
  scanResp.setServiceData(NimBLEUUID(PWNBEACON_SERVICE_UUID),
                          std::string((char *)adv_payload, adv_payload_len));

  ble_advertising->stop();
  ble_advertising->setAdvertisementData(advData);
  ble_advertising->setScanResponseData(scanResp);
  ble_advertising->start();

  return ESP_OK;
}

static uint32_t scan_start_time = 0;
static uint16_t scan_expected_duration = 0;

bool isPwnbeaconScanning() {
  // Safety timeout: if scan hasn't ended after expected duration + 1s, force reset
  if (ble_scanning && scan_start_time > 0 &&
      (millis() - scan_start_time > scan_expected_duration + 1000)) {
    NimBLEDevice::getScan()->stop();
    ble_scanning = false;
  }
  return ble_scanning;
}

void pwnbeaconScan(uint16_t duration_ms) {
  if (ble_scanning) {
    return;
  }

  // Keep advertising while scanning so other devices can see us
  NimBLEScan *scanner = NimBLEDevice::getScan();
  scanner->setScanCallbacks(&scanCallbacks, true);
  scanner->setActiveScan(true);
  scanner->setInterval(45);
  scanner->setWindow(15);
  scanner->clearResults();

  scan_start_time = millis();
  scan_expected_duration = duration_ms;
  ble_scanning = true;

  if (!scanner->start(duration_ms)) {
    ble_scanning = false;
  }
}

const int ble_away_threshold = 120000;

void checkPwnbeaconGonePeers() {
  for (uint8_t i = 0; i < pwnbeacon_friends_run; i++) {
    int away_secs = pwnbeacon_peers[i].last_seen - millis();
    if (away_secs > ble_away_threshold) {
      pwnbeacon_peers[i].gone = true;
      return;
    }
  }
}

signed int getPwnbeaconClosestRssi() {
  signed int closest = -1000;

  for (uint8_t i = 0; i < pwnbeacon_friends_run; i++) {
    if (pwnbeacon_peers[i].gone == false && pwnbeacon_peers[i].rssi > closest) {
      closest = pwnbeacon_peers[i].rssi;
    }
  }

  return closest;
}

static bool ble_phase_active = false;

bool pwnbeaconTick(String face) {
  if (!ble_phase_active) {
    ble_phase_active = true;
    pwnbeaconAdvertise(face);
    pwnbeaconScan(3000);
    return false;
  }

  checkPwnbeaconGonePeers();

  // Still scanning
  if (isPwnbeaconScanning()) {
    return false;
  }

  // Scan done — stop advertising so WiFi can use the radio
  ble_advertising->stop();
  ble_phase_active = false;
  return true;
}
