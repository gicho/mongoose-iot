/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#include <ets_sys.h>
#include <math.h>
#include <stdlib.h>

#include <osapi.h>
#include <gpio.h>
#include <os_type.h>
#include <user_interface.h>
#include <mem.h>

#include "fw/platforms/esp8266/user/util.h"
#include "fw/platforms/esp8266/user/esp_gpio.h"
#include "common/platforms/esp8266/esp_missing_includes.h"

void set_gpio(int g, int v) {
#define GPIO_SET(pin) gpio_output_set(1 << pin, 0, 1 << pin, 0);
#define GPIO_CLR(pin) gpio_output_set(0, 1 << pin, 1 << pin, 0);
  if (v) {
    GPIO_SET(g);
  } else {
    GPIO_CLR(g);
  }
}

int read_gpio_pin(int g) {
  gpio_output_set(0, 0, 0, 1 << g);
  return (gpio_input_get() & (1 << g)) != 0;
}

int await_change(int gpio, int *max_cycles) {
  int v1, v2, n;
  v1 = read_gpio_pin(gpio);
  for (n = *max_cycles; (*max_cycles)-- > 0;) {
    v2 = read_gpio_pin(gpio);
    if (v2 != v1) {
      return (n - *max_cycles);
    }
  }
  return 0;
}
