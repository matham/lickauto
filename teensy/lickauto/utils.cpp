#include "Arduino.h"
#include "utils.h"


static uint8_t v1 = 0;
static uint8_t v2 = 0;
static uint8_t v3 = 0;
static uint8_t v = 1;


uint8_t get_next_code_val() {
  // from https://github.com/edrosten/8bit_rng
  uint8_t temp = v1 ^ (v1 << 4);
  v1 = v2;
  v2 = v3;
  v3 = v;

  v = v3 ^ temp ^ (v3 >> 1) ^ (temp << 1);

  return v;
}
