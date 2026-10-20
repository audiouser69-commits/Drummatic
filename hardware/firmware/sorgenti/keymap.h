// generato da gen_keymap (circuito Drummatic): non modificare a mano
#pragma once
#define NUM_KEYS 46
#define NUM_ENC 9
#define NUM_LEDS 46
static const int8_t MATRIX[7][8] = {
  {0, 1, 2, 3, 4, 5, 6, 7},
  {8, 9, 10, 11, 12, 13, 14, 72},
  {15, 16, 17, 18, 19, 20, 21, 22},
  {23, 24, 25, 26, 27, 28, 29, -1},
  {30, 31, 32, 33, 34, 35, 36, 37},
  {38, 39, 40, 41, 42, 43, 44, 45},
  {64, 65, 66, 67, 68, 69, 70, 71},
};
static const uint8_t LED_TO_KEY[NUM_LEDS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45};
static const char *KEY_NAMES[NUM_KEYS] = {"VOICE", "SMPL", "TONE", "MOD", "SEQ", "LANE", "MIX", "PERF", "FUNC", "MAP", "LOCK", "DADO", "COMMIT", "KIT", "COPY", "VOICE1", "VOICE2", "VOICE3", "VOICE4", "VOICE5", "VOICE6", "VOICE7", "VOICE8", "REC", "PLAY", "STOP", "LEFT", "UP", "DOWN", "RIGHT", "TRIG1", "TRIG2", "TRIG3", "TRIG4", "TRIG5", "TRIG6", "TRIG7", "TRIG8", "TRIG9", "TRIG10", "TRIG11", "TRIG12", "TRIG13", "TRIG14", "TRIG15", "TRIG16"};
static const uint8_t ENC_BIT_A[NUM_ENC] = {7, 5, 3, 1, 15, 13, 11, 9, 23};
static const uint8_t ENC_BIT_B[NUM_ENC] = {6, 4, 2, 0, 14, 12, 10, 8, 22};
