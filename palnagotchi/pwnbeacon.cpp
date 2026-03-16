#include "pwnbeacon.h"
#include "config.h"
#include "storage.h"
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
  doc["name"]         = pwnbeacon_name;
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
                      const char *ble_name, const char *addr) {
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

  storageAddPeer(name.c_str(), "", fp_hex, "ble", rssi, addr);
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
        pwnbeaconAddPeer((const uint8_t *)svcData.data(), svcData.length(),
                         device->getRSSI(), devName.c_str(), addr.c_str());
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
  strncpy(pwnbeacon_name, "Palnagot", PWNBEACON_ADV_MAX_NAME_LEN);
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
  char_name->setValue(pwnbeacon_name);

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

  NimBLEClient *client = NimBLEDevice::createClient();
  client->setConnectTimeout(2);

  NimBLEAddress addr(std::string(peers[target].ble_addr.c_str()), 0);
  if (!client->connect(addr)) {
    NimBLEDevice::deleteClient(client);
    peers[target].full_data = true;
    return false;
  }

  bool got_data = false;

  NimBLERemoteService *svc = client->getService(
      NimBLEUUID(PWNBEACON_SERVICE_UUID));
  if (svc) {
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
  }

  client->disconnect();
  NimBLEDevice::deleteClient(client);

  peers[target].full_data = true;
  if (got_data) {
    storageLogPeer(peers[target].name.c_str(),
                   peers[target].face.c_str(), "", "BLE GATT");
    storageSavePeers();
  }

  return got_data;
}

// --- Tick ---

static bool ble_phase_active = false;
static bool ble_gatt_done = false;

bool pwnbeaconTick(String face) {
  if (!ble_phase_active) {
    ble_phase_active = true;
    ble_gatt_done = false;
    pwnbeaconAdvertise(face);
    pwnbeaconScan(3000);
    return false;
  }

  if (isPwnbeaconScanning()) {
    return false;
  }

  // After scan, attempt one GATT read before ending phase
  if (!ble_gatt_done) {
    ble_gatt_done = true;
    pwnbeaconGattRead();
  }

  // Done — stop advertising so WiFi can use the radio
  ble_advertising->stop();
  ble_phase_active = false;
  return true;
}
