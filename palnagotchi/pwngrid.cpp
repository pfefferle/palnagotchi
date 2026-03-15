#include "pwngrid.h"
#include "config.h"
#include "storage.h"

// Had to remove Radiotap headers, since its automatically added
// Also had to remove the last 4 bytes (frame check sequence)
const uint8_t pwngrid_beacon_raw[] = {
    0x80, 0x00,                          // FC
    0x00, 0x00,                          // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // DA (broadcast)
    0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,  // SA
    0xa1, 0x00, 0x64, 0xe6, 0x0b, 0x8b,  // BSSID
    0x40, 0x43,  // Sequence number/fragment number/seq-ctl
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Timestamp
    0x64, 0x00,                                      // Beacon interval
    0x11, 0x04,                                      // Capability info
    // 0xde (AC = 222) + 1 byte payload len + payload (AC Header)
    // For each 255 bytes of the payload, a new AC header should be set
};

const int raw_beacon_len = sizeof(pwngrid_beacon_raw);

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len,
                            bool en_sys_seq);

void pwnSnifferCallback(void *buf, wifi_promiscuous_pkt_type_t type);

const wifi_promiscuous_filter_t filter = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};

esp_err_t pwngridAdvertise(uint8_t channel, char session_id[18], String face) {
  JsonDocument pal_json;
  String pal_json_str = "";

  pal_json["pal"] = true;  // Also detect other Palnagotchis
  pal_json["name"] = PALNAGOTCHI_NAME;
  pal_json["face"] = face;
  pal_json["epoch"] = 1;
  pal_json["grid_version"] = "1.10.3";
  pal_json["identity"] = PALNAGOTCHI_IDENTITY;
  pal_json["pwnd_run"] = 0;
  pal_json["pwnd_tot"] = 0;
  pal_json["session_id"] = session_id;
  pal_json["timestamp"] = 0;
  pal_json["uptime"] = 0;
  pal_json["version"] = "1.8.4";
  pal_json["policy"]["advertise"] = true;
  pal_json["policy"]["bond_encounters_factor"] = 20000;
  pal_json["policy"]["bored_num_epochs"] = 0;
  pal_json["policy"]["sad_num_epochs"] = 0;
  pal_json["policy"]["excited_num_epochs"] = 9999;

  serializeJson(pal_json, pal_json_str);
  uint16_t pal_json_len = measureJson(pal_json);
  uint8_t header_len = 2 + ((uint8_t)(pal_json_len / 255) * 2);
  uint8_t pwngrid_beacon_frame[raw_beacon_len + pal_json_len + header_len];
  memcpy(pwngrid_beacon_frame, pwngrid_beacon_raw, raw_beacon_len);

  // Iterate through json string and copy it to beacon frame
  int frame_byte = raw_beacon_len;
  for (int i = 0; i < pal_json_len; i++) {
    // Write AC and len tags before every 255 bytes
    if (i == 0 || i % 255 == 0) {
      pwngrid_beacon_frame[frame_byte++] = 0xde;  // AC = 222
      uint8_t payload_len = 255;
      if (pal_json_len - i < 255) {
        payload_len = pal_json_len - i;
      }

      pwngrid_beacon_frame[frame_byte++] = payload_len;
    }

    // Append json byte to frame
    // If current byte is not ascii, add ? instead
    uint8_t next_byte = (uint8_t)'?';
    if (isAscii(pal_json_str[i])) {
      next_byte = (uint8_t)pal_json_str[i];
    }

    pwngrid_beacon_frame[frame_byte++] = next_byte;
  }

  esp_err_t result = esp_wifi_80211_tx(WIFI_IF_AP, pwngrid_beacon_frame,
                                       sizeof(pwngrid_beacon_frame), false);
  return result;
}

void pwngridAddPeer(JsonDocument &json, signed int rssi) {
  storageAddPeer(json["name"].as<String>().c_str(),
                 json["face"].as<String>().c_str(),
                 json["identity"].as<String>().c_str(),
                 "wifi", rssi);
}

void getMAC(char *addr, uint8_t *data, uint16_t offset) {
  snprintf(addr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", data[offset + 0],
           data[offset + 1], data[offset + 2], data[offset + 3],
           data[offset + 4], data[offset + 5]);
}

// Detect pwnagotchi adapted from Marauder
// https://github.com/justcallmekoko/ESP32Marauder/wiki/detect-pwnagotchi
// https://github.com/justcallmekoko/ESP32Marauder/blob/master/esp32_marauder/WiFiScan.cpp#L2255
void pwnSnifferCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *snifferPacket = (wifi_promiscuous_pkt_t *)buf;

  if (type != WIFI_PKT_MGMT) return;

  // Only process beacon frames
  if (snifferPacket->payload[0] != 0x80) return;

  int len = snifferPacket->rx_ctrl.sig_len - 4;

  char addr[] = "00:00:00:00:00:00";
  getMAC(addr, snifferPacket->payload, 10);

  String src(addr);
  if (src != "de:ad:be:ef:de:ad") return;

  // Extract JSON from vendor-specific IEs
  String essid = "";
  for (int i = 38; i < len; i++) {
    if (isAscii(snifferPacket->payload[i])) {
      essid.concat((char)snifferPacket->payload[i]);
    }
  }

  JsonDocument sniffed_json;
  DeserializationError result = deserializeJson(sniffed_json, essid);

  if (result == DeserializationError::Ok) {
    pwngridAddPeer(sniffed_json, snifferPacket->rx_ctrl.rssi);
  }
}

void initPwngrid() {
  esp_log_level_set("wifi", ESP_LOG_NONE);

  wifi_init_config_t WIFI_INIT_CONFIG = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&WIFI_INIT_CONFIG);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_AP);
}

static bool wifi_started = false;

void startPwngridSniffing() {
  if (!wifi_started) {
    esp_wifi_start();
    wifi_started = true;
  }
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(&pwnSnifferCallback);
  esp_wifi_set_promiscuous(true);
}

void stopPwngridSniffing() {
  esp_wifi_set_promiscuous(false);
}

static uint32_t wifi_phase_start = 0;

// Dwell on each channel for 2 seconds, cycle through 1-11, then hand off to BLE
static uint32_t channel_start_time = 0;
static const uint16_t DWELL_TIME_MS = 2000;
static const uint8_t MAX_CHANNEL = 11;

bool pwngridTick(uint8_t &channel, char *session_id, String face) {
  if (wifi_phase_start == 0) {
    startPwngridSniffing();
    wifi_phase_start = millis();
    channel_start_time = 0;
  }

  // Set channel and send beacon once at start of each dwell
  if (channel_start_time == 0) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    delay(10);
    pwngridAdvertise(channel, session_id, face);
    channel_start_time = millis();
  }

  // After dwell time, move to next channel
  if (millis() - channel_start_time > DWELL_TIME_MS) {
    channel++;
    if (channel > MAX_CHANNEL) channel = 1;
    channel_start_time = 0;
  }

  // End phase after cycling through all channels (~22 seconds)
  if (millis() - wifi_phase_start > (uint32_t)DWELL_TIME_MS * MAX_CHANNEL) {
    stopPwngridSniffing();
    wifi_phase_start = 0;
    return true;
  }

  return false;
}
