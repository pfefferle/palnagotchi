#include "mood.h"

// ASCII equivalent
String palnagotchi_default_moods[] = {
    "(v__v)",  // 0 - sleeping
    "(=__=)",  // 1 - awakening
    "(O__O)",  // 2 - awake
    "( O_O)",  // 3 - observing (neutral) right
    "(O_O )",  // 4 - observig (neutral) left
    "( 0_0)",  // 5 - observing (happy) right
    "(0_0 )",  // 6 - observing (happy) left
    "(+__+)",  // 7 - intense
    "(-@_@)",  // 8 - cool
    "(0__0)",  // 9 - happy
    "(^__^)",  // 10 - grateful
    "(a__a)",  // 11 - excited
    "(+__+)",  // 12 - smart
    "(*__*)",  // 13 - friendly
    "(@__@)",  // 14 - motivated
    "(>__<)",  // 15 - demotivated
    "(-__-)",  // 16 - bored
    "(T_T )",  // 17 - sad
    "(;__;)",  // 18 - lonely
    "(X__X)",  // 19 - broken
    "(#__#)",  // 20 - debugging,
    "8====D",  // 21 - ultra random easter egg
};

// ASCII mood faces
String minigotchi_moods[] = {
    "(v_v)",  // 0 - sleeping
    "(=_=)",  // 1 - awakening
    "(O_O)",  // 2 - awake
    "(o_O)",  // 3 - observing (neutral) right
    "(O_o)",  // 4 - observing (neutral) left
    "(o_0)",  // 5 - observing (happy) right
    "(0_o)",  // 6 - observing (happy) left
    "(+_+)",  // 7 - intense
    "(@_@)",  // 8 - cool
    "(0_0)",  // 9 - happy
    "(^_^)",  // 10 - grateful
    "(a_a)",  // 11 - excited
    "(+_+)",  // 12 - smart
    "(*_*)",  // 13 - friendly
    "(@_@)",  // 14 - motivated
    "(>_<)",  // 15 - demotivated
    "(-_-)",  // 16 - bored
    "(T_T)",  // 17 - sad
    "(;_;)",  // 18 - lonely
    "(X_X)",  // 19 - broken
    "(#_#)",  // 20 - debugging
    "8=D"     // 21 - ultra random easter egg
};

String palnagotchi_moods_default_desc[] = {
    "Zzzz...",                               // 0 - sleeping
    "...",                                   // 1 - awakening
    "Let's MAKE FRIENDS!",                   // 2 - awake
    "WANTED: FRIENDS",                       // 3 - observing (neutral) right
    "WANTED: FRIENDS",                       // 4 - observig (neutral) left
    "Can we have even more friends?",        // 5 - observing (happy) right
    "Can we have even more friends?",        // 6 - observing (happy) left
    "YEAH! So many pwnagotchis!",            // 7 - intense
    "The coolest pal in the neighbourhood",  // 8 - cool
    "Can we have even more friends?",        // 9 - happy
    "I LOVE PWNAGOTCHIS!",                   // 10 - grateful
    "That's how I like it.",                 // 11 - excited
    "3.1415926535897932384626433832795",     // 12 - smart
    "HEY YOU! LETS BE FRIENDS!",             // 13 - friendly
    "IT RUNS! SUCK MA BALLZ!",               // 14 - motivated
    "Why my life sucks? WHY",                // 15 - demotivated
    "Seriously, let's go for a walk...",     // 16 - bored
    "Get your hands off me...",              // 17 - sad
    "Where are all the Pwnagotchis?",        // 18 - lonely
    "It works on my end.",                   // 19 - broken
    "Wtf? I didn't even touch it...",        // 20 - debugging,
    "What?",                                 // 21 - ultra random easter egg
};

// Mood descriptions
String minigotchi_moods_desc[] = {
    "Zzzz...",              // 0 - sleeping
    "...",                  // 1 - awakening
    "Let's MAKE FRIENDS!",  // 2 - awake
    "Anybody here?",        // 3 - observing (neutral) right
    "WANTED: FRIENDS",      // 4 - observing (neutral) left
    "Looking around...",    // 5 - observing (happy) right
    "Hello, friend?",       // 6 - observing (happy) left
    "YEAH! pwnagotchis!",   // 7 - intense
    "Coolest pal around",   // 8 - cool
    "More friends!!!",      // 9 - happy
    "<3 PWNAGOTCHIS!",      // 10 - grateful
    "I like it.",           // 11 - excited
    "3.14159",              // 12 - smart
    "HEY YOU!",             // 13 - friendly
    "IT RUNS!",             // 14 - motivated
    "WHY???",               // 15 - demotivated
    "Seriously?",           // 16 - bored
    "hands off!!!",         // 17 - sad
    "pwnagotchis? where?",  // 18 - lonely
    "Works on my end.",     // 19 - broken
    "Wtf?",                 // 20 - debugging
    "What?"                 // 21 - ultra random easter egg
};

// Porkchop faces (from M5PORKCHOP piglet avatar)
String porkchop_faces[] = {
    "(o 00)",   // 0 - neutral
    "(^ 00)",   // 1 - happy
    "(@ 00)",   // 2 - excited
    "(= 00)",   // 3 - hunting
    "(- 00)",   // 4 - sleepy
    "(T 00)",   // 5 - sad
    "(# 00)",   // 6 - angry
};

String crackling_moods[] = {
    "(o00)",    // 0 - neutral
    "(^00)",    // 1 - happy
    "(@00)",    // 2 - excited
    "(=00)",    // 3 - hunting
    "(-00)",    // 4 - sleepy
    "(T00)",    // 5 - sad
    "(#00)",    // 6 - angry
};

String porkchop_phrases[] = {
    "OINK OINK!",
    "Truffle time!",
    "SQUEEEAL!",
    "Sniffing packets...",
    "Snort... Zzz...",
    "Need mud...",
    "OINK! OINK! OINK!",
};

const int porkchop_count = sizeof(porkchop_faces) / sizeof(porkchop_faces[0]);

String* porkchop_moods = porkchop_faces;

String* palnagotchi_moods = palnagotchi_default_moods;
String* palnagotchi_moods_desc = palnagotchi_moods_default_desc;

uint8_t current_mood = 0;
String current_phrase = "";
String current_face = "";
bool current_broken = false;
uint8_t current_theme = THEME_DEFAULT;

uint8_t getCurrentMoodId() { return current_mood; }
String getCurrentMoodFace() { return current_face; }
String getCurrentMoodPhrase() { return current_phrase; }
bool isCurrentMoodBroken() { return current_broken; }
uint8_t getCurrentTheme() { return current_theme; }

uint16_t getCurrentThemeColor() {
  switch (current_theme) {
    case THEME_PORKCHOP: return COLOR_PORKCHOP;
    default:             return COLOR_DEFAULT;
  }
}

void initMood() {
  if (M5.Display.width() <= 128) {
    palnagotchi_moods = minigotchi_moods;
    palnagotchi_moods_desc = minigotchi_moods_desc;
    porkchop_moods = crackling_moods;
  }
}

void setThemeMood(uint8_t theme) {
  current_theme = theme;
  current_broken = false;

  switch (theme) {
    case THEME_PORKCHOP: {
      int idx = random(0, porkchop_count);
      current_face = porkchop_moods[idx];
      current_phrase = porkchop_phrases[idx];
      break;
    }
    default:
      current_theme = THEME_DEFAULT;
      setMood(random(2, 21));
      break;
  }
}

void setMood(uint8_t mood, String face, String phrase, bool broken) {
  current_mood = mood;
  current_broken = broken;
  current_theme = THEME_DEFAULT;

  if (face != "") {
    current_face = face;
  } else {
    current_face = palnagotchi_moods[current_mood];
  }

  if (phrase != "") {
    current_phrase = phrase;
  } else {
    current_phrase = palnagotchi_moods_desc[current_mood];
  }
}
