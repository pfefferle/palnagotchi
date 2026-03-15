#pragma once

#include "Arduino.h"
#include "ArduinoJson.h"
#include "M5Unified.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"

void initPwngrid();
esp_err_t pwngridAdvertise(uint8_t channel, char session_id[18], String face);
bool pwngridTick(uint8_t &channel, char *session_id, String face);
