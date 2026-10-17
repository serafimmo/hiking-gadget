#pragma once
// The FIRST include of every hal/*.cpp (device only; lint checks it).
#include <Arduino.h>                             // defines ESP_ARDUINO_VERSION_MAJOR
#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "Hiking Gadget v2 requires arduino-esp32 core 3.x (developed on 3.3.12)"
#endif
#if !ARDUINO_USB_CDC_ON_BOOT || !ARDUINO_USB_MODE  // -D flags from boards.txt menus [V-src platform.txt 79]
#error "Arduino IDE: Tools > USB CDC On Boot = Enabled, USB Mode = Hardware CDC and JTAG (see README)"
#endif
