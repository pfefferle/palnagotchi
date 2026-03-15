#pragma once

#include "Arduino.h"
#include "M5Unified.h"

#define MOOD_BROKEN 19

#define THEME_DEFAULT 0
#define THEME_PORKCHOP 1

#define COLOR_DEFAULT GREEN
#define COLOR_PORKCHOP 0xFC18  // Pink

void initMood();
void setMood(uint8_t mood, String face = "", String phrase = "",
             bool broken = false);
void setThemeMood(uint8_t theme);
uint8_t getCurrentMoodId();
String getCurrentMoodFace();
String getCurrentMoodPhrase();
bool isCurrentMoodBroken();
uint8_t getCurrentTheme();
uint16_t getCurrentThemeColor();
