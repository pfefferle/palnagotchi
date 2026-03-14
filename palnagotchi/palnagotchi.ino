#include "M5Unified.h"

#ifdef ARDUINO_M5STACK_CARDPUTER
  #include "M5Cardputer.h"
#endif

#ifdef ARDUINO_M5STACK_DIAL
  #include "M5Dial.h"
#endif

#ifdef ARDUINO_M5STACK_DINMETER
  #include "M5DinMeter.h"
#endif

#include "EEPROM.h"
#include "esp_system.h"
#include "ui.h"

#define STATE_INIT 0
#define STATE_WAKE 1
#define STATE_HALT 255

uint8_t state = STATE_INIT;
char session_id[18] = "";

void initM5() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.begin();

  #ifdef ARDUINO_M5STACK_CARDPUTER
    M5Cardputer.begin(cfg, true);
  #endif

  #ifdef ARDUINO_M5STACK_DIAL
    M5Dial.begin(cfg, true, false);
  #endif

  #ifdef ARDUINO_M5STACK_DINMETER
    DinMeter.begin(cfg, true);
  #endif
}

// TODO: store SID in EEPROM
void setRandomSessionId() {
  randomSeed(esp_random());
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

  for (int i = 0; i < 17; i++) {
    session_id[i] = charset[random(0, sizeof(charset) - 1)];
  }

  session_id[17] = '\0';
}

void setup() {
  EEPROM.begin(256);
  Serial.begin(9600);
  setRandomSessionId();
  initM5();
  initMood();
  initPwngrid();
  initUi();
}

uint8_t current_channel = 1;
uint32_t last_mood_switch = 10001;

void wakeUp() {
  for (uint8_t i = 0; i < 3; i++) {
    setMood(i);
    updateUi();
    delay(1250);
  }
}

void advertise(uint8_t channel) {
  uint32_t elapsed = millis() - last_mood_switch;
  if (elapsed > 50000) {
    setMood(random(2, 21));
    last_mood_switch = millis();
  }

  esp_err_t result = pwngridAdvertise(channel, session_id, getCurrentMoodFace());

  if (result == ESP_ERR_WIFI_IF) {
    setMood(MOOD_BROKEN, "", "Error: invalid interface", true);
    state = STATE_HALT;
  } else if (result == ESP_ERR_INVALID_ARG) {
    setMood(MOOD_BROKEN, "", "Error: invalid argument", true);
    state = STATE_HALT;
  } else if (result != ESP_OK) {
    setMood(MOOD_BROKEN, "", "Error: unknown", true);
    state = STATE_HALT;
  }
}

void loop() {
  M5.update();
  
  #ifdef ARDUINO_M5STACK_CARDPUTER
    M5Cardputer.update();
  #endif

  if (state == STATE_HALT) {
    return;
  }

  if (state == STATE_INIT) {
    wakeUp();
    state = STATE_WAKE;
  }

  if (state == STATE_WAKE) {
    checkPwngridGoneFriends();
    advertise(current_channel++);
    if (current_channel == 15) {
      current_channel = 1;
    }
  }

  updateUi(true);
}
