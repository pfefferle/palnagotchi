#include "pwnbeacon.h"
#include "config.h"
#include "storage.h"
#include "mood.h"
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <mbedtls/sha256.h>

char pwnbeacon_name[PWNBEACON_ADV_MAX_NAME_LEN + 1] = {0};
char pwnbeacon_identity[129] = {0};
char pwnbeacon_face[64] = "(O__O)";
uint16_t pwnbeacon_pwnd_run = 0;
uint16_t pwnbeacon_pwnd_tot = 0;
uint8_t pwnbeacon_fingerprint[PWNBEACON_FINGERPRINT_LEN] = {0};

NimBLEServer *ble_server = nullptr;
NimBLECharacteristic *char_identity = nullptr;
NimBLECharacteristic *char_face = nullptr;
NimBLECharacteristic *char_name = nullptr;
NimBLECharacteristic *char_signal = nullptr;
NimBLECharacteristic *char_message = nullptr;
NimBLEAdvertising *ble_advertising = nullptr;
bool ble_scanning = false;

void computeFingerprint(const char *identity, uint8_t *out) {
  uint8_t hash[32];
  mbedtls_sha256((const unsigned char *)identity, strlen(identity), hash, 0);
  memcpy(out, hash, PWNBEACON_FINGERPRINT_LEN);
}

String buildIdentityJson() {
  JsonDocument doc;

  doc["pal"]          = true;
  doc["name"]         = PALNAGOTCHI_NAME;
  doc["face"]         = pwnbeacon_face;
  doc["epoch"]        = 1;
  doc["grid_version"] = "2.0.0-ble";
  doc["identity"]     = pwnbeacon_identity;
  doc["pwnd_run"]     = pwnbeacon_pwnd_run;
  doc["pwnd_tot"]     = pwnbeacon_pwnd_tot;
  doc["session_id"]   = NimBLEDevice::getAddress().toString().c_str();
  doc["timestamp"]    = (int)(millis() / 1000);
  doc["uptime"]       = (int)(millis() / 1000);
  doc["version"]      = "1.8.4";

  String json;
  serializeJson(doc, json);
  return json;
}

void buildAdvPayload(uint8_t *buf, size_t *len) {
  pwnbeacon_adv adv;
  memset(&adv, 0, sizeof(adv));

  adv.version = PWNBEACON_PROTOCOL_VERSION;
  adv.flags = PWNBEACON_FLAG_ADVERTISE | PWNBEACON_FLAG_CONNECTABLE;
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

void pwnbeaconAddPeer(const uint8_t *data, size_t len, int8_t rssi,
                      const char *ble_name, const char *addr,
                      uint8_t addr_type) {
  if (len < 10) return;

  pwnbeacon_adv adv;
  memset(&adv, 0, sizeof(adv));
  size_t copy_len = len < sizeof(adv) ? len : sizeof(adv);
  memcpy(&adv, data, copy_len);

  if (adv.version != PWNBEACON_PROTOCOL_VERSION) return;
  if (memcmp(adv.fingerprint, pwnbeacon_fingerprint, PWNBEACON_FINGERPRINT_LEN) == 0) return;

  // Build fingerprint hex string as identity
  char fp_hex[PWNBEACON_FINGERPRINT_LEN * 2 + 1];
  for (int i = 0; i < PWNBEACON_FINGERPRINT_LEN; i++) {
    snprintf(fp_hex + i * 2, 3, "%02x", adv.fingerprint[i]);
  }

  // Resolve name
  uint8_t name_len = adv.name_len;
  if (name_len > PWNBEACON_ADV_MAX_NAME_LEN) name_len = PWNBEACON_ADV_MAX_NAME_LEN;

  String name;
  if (name_len > 0) {
    name = String(adv.name).substring(0, name_len);
  } else if (ble_name && strlen(ble_name) > 0) {
    name = String(ble_name);
  } else {
    name = "BLE peer";
  }

  storageAddPeer(name.c_str(), "", fp_hex, "ble", rssi, addr, addr_type);
}

// --- NimBLE Callbacks ---

class PwnBeaconScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *device) override {
    int svcDataCount = device->getServiceDataCount();
    for (int i = 0; i < svcDataCount; i++) {
      if (device->getServiceDataUUID(i).equals(NimBLEUUID(PWNBEACON_SERVICE_UUID))) {
        std::string svcData = device->getServiceData(i);
        std::string devName = device->getName();
        std::string addr = device->getAddress().toString();
        uint8_t addrType = device->getAddress().getType();
        pwnbeaconAddPeer((const uint8_t *)svcData.data(), svcData.length(),
                         device->getRSSI(), devName.c_str(), addr.c_str(),
                         addrType);
        break;
      }
    }
  }

  void onScanEnd(const NimBLEScanResults &results, int reason) override {
    ble_scanning = false;
  }
};

static PwnBeaconScanCallbacks scanCallbacks;

class PwnBeaconServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override {
    ble_advertising->start();
  }
};

class SignalCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override {
    // Signal received — ping/poke
  }
};

class MessageCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override {
    std::string raw = characteristic->getValue();
    if (raw.length() > 0) {
      storageLogMessage("BLE peer", raw.c_str());
    }
  }
};

// --- Public API ---

void initPwnbeacon() {
  // Shorten name for advertisement: strip vowels first, then truncate if needed
  const char *full = PALNAGOTCHI_NAME;
  size_t full_len = strlen(full);

  if (full_len <= PWNBEACON_ADV_MAX_NAME_LEN) {
    strncpy(pwnbeacon_name, full, PWNBEACON_ADV_MAX_NAME_LEN);
  } else {
    // Try removing vowels (keep first character regardless)
    char stripped[128];
    size_t j = 0;
    for (size_t i = 0; i < full_len && j < sizeof(stripped) - 1; i++) {
      if (i == 0 || !strchr("aeiouAEIOU", full[i])) {
        stripped[j++] = full[i];
      }
    }
    stripped[j] = '\0';

    // Use stripped version if it fits, otherwise truncate original
    if (j <= PWNBEACON_ADV_MAX_NAME_LEN) {
      strncpy(pwnbeacon_name, stripped, PWNBEACON_ADV_MAX_NAME_LEN);
    } else {
      strncpy(pwnbeacon_name, full, PWNBEACON_ADV_MAX_NAME_LEN);
    }
  }
  pwnbeacon_name[PWNBEACON_ADV_MAX_NAME_LEN] = '\0';
  strncpy(pwnbeacon_identity, PALNAGOTCHI_IDENTITY,
          sizeof(pwnbeacon_identity) - 1);
  computeFingerprint(pwnbeacon_identity, pwnbeacon_fingerprint);

  NimBLEDevice::init(PALNAGOTCHI_NAME);

  // GATT server
  ble_server = NimBLEDevice::createServer();
  ble_server->setCallbacks(new PwnBeaconServerCallbacks());

  NimBLEService *service = ble_server->createService(PWNBEACON_SERVICE_UUID);

  // Identity — full JSON payload (read)
  char_identity = service->createCharacteristic(
      PWNBEACON_IDENTITY_CHAR_UUID, NIMBLE_PROPERTY::READ);
  char_identity->setValue(buildIdentityJson());

  // Face — current mood (read + notify)
  char_face = service->createCharacteristic(
      PWNBEACON_FACE_CHAR_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  char_face->setValue(pwnbeacon_face);

  // Name (read)
  char_name = service->createCharacteristic(
      PWNBEACON_NAME_CHAR_UUID, NIMBLE_PROPERTY::READ);
  char_name->setValue(PALNAGOTCHI_NAME);

  // Signal — write-only ping/poke
  char_signal = service->createCharacteristic(
      PWNBEACON_SIGNAL_CHAR_UUID, NIMBLE_PROPERTY::WRITE);
  char_signal->setCallbacks(new SignalCallbacks());

  // Message — read/write/notify for text messages
  char_message = service->createCharacteristic(
      PWNBEACON_MESSAGE_CHAR_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  char_message->setCallbacks(new MessageCallbacks());

  service->start();

  // Advertising
  ble_advertising = NimBLEDevice::getAdvertising();
  ble_advertising->addServiceUUID(NimBLEUUID(PWNBEACON_SERVICE_UUID));
  ble_advertising->setMinInterval(0x20);
  ble_advertising->setMaxInterval(0x40);
}

esp_err_t pwnbeaconAdvertise(String face) {
  // Update face in GATT characteristics
  strncpy(pwnbeacon_face, face.c_str(), sizeof(pwnbeacon_face) - 1);
  pwnbeacon_face[sizeof(pwnbeacon_face) - 1] = '\0';

  if (char_face) {
    char_face->setValue(pwnbeacon_face);
    char_face->notify();
  }
  if (char_identity) {
    char_identity->setValue(buildIdentityJson());
  }
  if (char_message) {
    char_message->setValue(getCurrentMoodPhrase().c_str());
  }

  // Build compact advertisement payload
  uint8_t adv_payload[sizeof(pwnbeacon_adv)];
  size_t adv_payload_len = 0;
  buildAdvPayload(adv_payload, &adv_payload_len);

  // BLE scan response: 31 bytes max, 128-bit UUID takes 18 bytes overhead,
  // leaving ~13 for data. Clamp to 10 for safety.
  if (adv_payload_len > 10) {
    adv_payload_len = 10;
  }

  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.setCompleteServices(NimBLEUUID(PWNBEACON_SERVICE_UUID));

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
  if (ble_scanning && scan_start_time > 0 &&
      (millis() - scan_start_time > scan_expected_duration + 1000)) {
    NimBLEDevice::getScan()->stop();
    ble_scanning = false;
  }
  return ble_scanning;
}

void pwnbeaconScan(uint16_t duration_ms) {
  if (ble_scanning) return;

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

// --- GATT Client ---

bool pwnbeaconGattRead() {
  storage_peer *peers = storageGetPeers();
  uint8_t count = storageGetPeerCount();

  // Find first BLE peer needing GATT data
  int target = -1;
  for (uint8_t i = 0; i < count; i++) {
    if (peers[i].type == "ble" && !peers[i].full_data &&
        !peers[i].gone && peers[i].ble_addr.length() > 0) {
      target = i;
      break;
    }
  }

  if (target < 0) return false;

  Serial.printf("[ble] gatt: connecting to %s (type=%d)\n", peers[target].ble_addr.c_str(), peers[target].ble_addr_type);

  // Ensure scan is fully stopped before connecting
  NimBLEDevice::getScan()->stop();
  delay(50);

  NimBLEClient *client = NimBLEDevice::createClient();
  client->setConnectTimeout(10000);

  NimBLEAddress addr(std::string(peers[target].ble_addr.c_str()), peers[target].ble_addr_type);
  if (!client->connect(addr)) {
    Serial.printf("[ble] gatt: connection failed to %s (rc=%d)\n",
                  peers[target].ble_addr.c_str(), client->getLastError());
    NimBLEDevice::deleteClient(client);
    return false;
  }

  Serial.printf("[ble] gatt: connected to %s\n", peers[target].ble_addr.c_str());

  bool got_data = false;

  NimBLERemoteService *svc = client->getService(
      NimBLEUUID(PWNBEACON_SERVICE_UUID));
  if (svc) {
    Serial.println("[ble] gatt: found service");

    // Read full identity JSON
    NimBLERemoteCharacteristic *id_char = svc->getCharacteristic(
        NimBLEUUID(PWNBEACON_IDENTITY_CHAR_UUID));
    if (id_char) {
      std::string val = id_char->readValue();
      if (val.length() > 0) {
        JsonDocument doc;
        if (deserializeJson(doc, val.c_str()) == DeserializationError::Ok) {
          peers[target].identity = doc["identity"] | peers[target].identity;
          peers[target].name     = doc["name"] | peers[target].name;
          peers[target].face     = doc["face"] | peers[target].face;
          got_data = true;
        }
      }
    }

    // Read face (may be more current than identity JSON)
    NimBLERemoteCharacteristic *face_char = svc->getCharacteristic(
        NimBLEUUID(PWNBEACON_FACE_CHAR_UUID));
    if (face_char) {
      std::string val = face_char->readValue();
      if (val.length() > 0) {
        peers[target].face = val.c_str();
        got_data = true;
      }
    }

    // Read full name
    NimBLERemoteCharacteristic *name_char = svc->getCharacteristic(
        NimBLEUUID(PWNBEACON_NAME_CHAR_UUID));
    if (name_char) {
      std::string val = name_char->readValue();
      if (val.length() > 0) {
        peers[target].name = val.c_str();
        got_data = true;
      }
    }

    // Read message
    NimBLERemoteCharacteristic *msg_char = svc->getCharacteristic(
        NimBLEUUID(PWNBEACON_MESSAGE_CHAR_UUID));
    if (msg_char) {
      std::string val = msg_char->readValue();
      if (val.length() > 0) {
        storageLogMessage(peers[target].name.c_str(), val.c_str());
      }
    }
  } else {
    Serial.println("[ble] gatt: service not found");
  }

  client->disconnect();
  NimBLEDevice::deleteClient(client);

  peers[target].full_data = true;
  if (got_data) {
    Serial.printf("[ble] gatt: got data for %s\n", peers[target].name.c_str());
    storageLogPeer(peers[target].name.c_str(),
                   peers[target].face.c_str(), "", "BLE GATT");
    storageSavePeers();
  } else {
    Serial.println("[ble] gatt: no data received");
  }

  return got_data;
}

// --- Tick ---

static bool ble_phase_active = false;
static uint32_t ble_phase_start = 0;
static uint8_t ble_scan_count = 0;
static bool ble_is_connector = false;
static const uint8_t BLE_MAX_SCANS = 3;
static const uint32_t BLE_PHASE_MAX_MS = 20000;
// Beacon-only mode: just advertise for this long so peers can connect to us
static const uint32_t BLE_BEACON_DURATION_MS = 12000;

bool pwnbeaconTick(String face) {
  if (!ble_phase_active) {
    ble_phase_active = true;
    ble_phase_start = millis();
    ble_scan_count = 0;
    pwnbeaconAdvertise(face);

    // Randomly choose role: connector (scan + GATT client) or beacon (just advertise)
    ble_is_connector = (random(0, 2) == 0);
    Serial.printf("[ble] phase start, role=%s\n", ble_is_connector ? "connector" : "beacon");

    if (ble_is_connector) {
      pwnbeaconScan(3000 + random(0, 3000));
    }
    return false;
  }

  // Beacon mode: just wait while advertising, let peers connect to us
  if (!ble_is_connector) {
    if (millis() - ble_phase_start > BLE_BEACON_DURATION_MS) {
      ble_advertising->stop();
      ble_phase_active = false;
      return true;
    }
    return false;
  }

  // Connector mode: scan, then GATT connect
  if (isPwnbeaconScanning()) {
    return false;
  }

  pwnbeaconGattRead();

  ble_scan_count++;

  // Check if we should end the BLE phase
  if (ble_scan_count >= BLE_MAX_SCANS ||
      millis() - ble_phase_start > BLE_PHASE_MAX_MS) {
    ble_advertising->stop();
    ble_phase_active = false;
    return true;
  }

  // Start another scan cycle
  pwnbeaconScan(3000 + random(0, 3000));
  return false;
}
