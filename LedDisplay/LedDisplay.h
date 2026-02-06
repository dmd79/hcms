/*
  LedDisplay -- controller library for Avago HCMS-297x displays
  Modified for ESP-IDF with ESPHome support
  
  Copyright (c) 2009 Tom Igoe. Some right reserved.
  Revisions by Mark Liebman, 27 Jan 2010
*/

#ifndef LedDisplay_h
#define LedDisplay_h

// ESPHome compatibility
#ifdef ESPHOME
  #include "esphome/core/hal.h"
  #include "esphome/core/component.h"
  using namespace esphome;
  
  // ESPHome already provides these functions
  #define pinMode(pin, mode) do { } while(0)
  #define digitalWrite(pin, val) do { \
    auto p = new GPIOPin(); \
    p->setup(); \
    p->digital_write(val); \
  } while(0)
  
#else
  // ESP-IDF puro
  #include <stdint.h>
  #include <stddef.h>
  #include <string.h>
  #include "driver/gpio.h"
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"

  #define HIGH 1
  #define LOW 0
  #define OUTPUT GPIO_MODE_OUTPUT
  #define INPUT GPIO_MODE_INPUT
  #define MSBFIRST 1
  #define LSBFIRST 0
#endif

#define LEDDISPLAY_MAXCHARS 32

class LedDisplay
{
  private:
    uint8_t dataPin;
    uint8_t registerSelect;
    uint8_t clockPin;
    uint8_t chipEnable;
    uint8_t resetPin;
    uint8_t displayLength;
    int cursorPos;
    
    char stringBuffer[LEDDISPLAY_MAXCHARS + 1];
    const char *displayString;
    uint8_t dotRegister[LEDDISPLAY_MAXCHARS * 5];
    
    void writeCharacter(char whatCharacter, uint8_t whatPosition);
    void loadControlRegister(uint8_t dataByte);
    void loadAllControlRegisters(uint8_t dataByte);
    void loadDotRegister();
    
  public:
    LedDisplay(uint8_t _dataPin,
               uint8_t _registerSelect,
               uint8_t _clockPin,
               uint8_t _chipEnable,
               uint8_t _resetPin,
               uint8_t _displayLength);
    
    void begin();
    void clear();
    void home();
    void setCursor(int whichPosition);
    int getCursor();
    size_t write(uint8_t b);
    void setString(const char *_displayString);
    const char *getString();
    int stringLength();
    void scroll(int direction);
    void setBrightness(uint8_t bright);
    int version(void);
};

#endif
