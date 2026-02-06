/*
  LedDisplay -- controller library for Avago HCMS-297x displays
  Modified for ESP-IDF with ESPHome support
  
  Copyright (c) 2009 Tom Igoe. Some right reserved.
  Revisions by Mark Liebman, 27 Jan 2010
*/

#include "LedDisplay.h"
#include "Font5x7.h"

// Helper functions for ESP-IDF (not needed for ESPHome)
#ifndef ESPHOME

static void pinMode(uint8_t pin, uint8_t mode) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = (gpio_mode_t)mode;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
}

static void digitalWrite(uint8_t pin, uint8_t val) {
    gpio_set_level((gpio_num_t)pin, val);
}

static void delay(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

static void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val) {
    for (uint8_t i = 0; i < 8; i++) {
        if (bitOrder == LSBFIRST) {
            digitalWrite(dataPin, !!(val & (1 << i)));
        } else {
            digitalWrite(dataPin, !!(val & (1 << (7 - i))));
        }
        digitalWrite(clockPin, HIGH);
        digitalWrite(clockPin, LOW);
    }
}

#else
// ESPHome - use ESPHome's GPIO functions
static void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val) {
    auto data_gpio = new GPIOPin();
    data_gpio->set_pin(dataPin);
    data_gpio->setup();
    
    auto clock_gpio = new GPIOPin();
    clock_gpio->set_pin(clockPin);
    clock_gpio->setup();
    
    for (uint8_t i = 0; i < 8; i++) {
        if (bitOrder == LSBFIRST) {
            data_gpio->digital_write(!!(val & (1 << i)));
        } else {
            data_gpio->digital_write(!!(val & (1 << (7 - i))));
        }
        clock_gpio->digital_write(true);
        clock_gpio->digital_write(false);
    }
    
    delete data_gpio;
    delete clock_gpio;
}

static void pinMode(uint8_t pin, uint8_t mode) {
    auto gpio = new GPIOPin();
    gpio->set_pin(pin);
    gpio->setup();
    gpio->pin_mode(mode == OUTPUT ? gpio::FLAG_OUTPUT : gpio::FLAG_INPUT);
    delete gpio;
}

static void digitalWrite(uint8_t pin, uint8_t val) {
    auto gpio = new GPIOPin();
    gpio->set_pin(pin);
    gpio->setup();
    gpio->digital_write(val);
    delete gpio;
}

static void delay(uint32_t ms) {
    delayMicroseconds(ms * 1000);
}
#endif

// Constructor
LedDisplay::LedDisplay(uint8_t _dataPin,
                       uint8_t _registerSelect,
                       uint8_t _clockPin,
                       uint8_t _chipEnable,
                       uint8_t _resetPin,
                       uint8_t _displayLength)
{
    this->dataPin = _dataPin;
    this->registerSelect = _registerSelect;
    this->clockPin = _clockPin;
    this->chipEnable = _chipEnable;
    this->resetPin = _resetPin;
    this->displayLength = _displayLength;
    this->cursorPos = 0;

    if (_displayLength > LEDDISPLAY_MAXCHARS) {
        this->displayLength = LEDDISPLAY_MAXCHARS;
    }

    for (unsigned int i = 0; i < sizeof(stringBuffer); i++) {
        stringBuffer[i] = ' ';
    }
    stringBuffer[sizeof(stringBuffer) - 1] = '\0';
    
    this->setString(stringBuffer);
}

void LedDisplay::begin() {
    pinMode(dataPin, OUTPUT);
    pinMode(registerSelect, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(chipEnable, OUTPUT);
    pinMode(resetPin, OUTPUT);

    digitalWrite(resetPin, LOW);
    delay(10);
    digitalWrite(resetPin, HIGH);

    loadDotRegister();
    loadAllControlRegisters(0b01111111);
}

void LedDisplay::clear() {
    this->setString(stringBuffer);
    for (int displayPos = 0; displayPos < displayLength; displayPos++) {
        writeCharacter(' ', displayPos);
    }
    loadDotRegister();
}

void LedDisplay::home() {
    this->cursorPos = 0;
}

void LedDisplay::setCursor(int whichPosition) {
    this->cursorPos = whichPosition;
}

int LedDisplay::getCursor() {
    return this->cursorPos;
}

size_t LedDisplay::write(uint8_t b) {
    if (cursorPos >= 0 && cursorPos < displayLength) {
        writeCharacter(b, cursorPos);
        if (this->displayString == stringBuffer && cursorPos < LEDDISPLAY_MAXCHARS) {
            stringBuffer[cursorPos] = b;
        }
        cursorPos++;
        loadDotRegister();
    }
    return 1;
}

void LedDisplay::scroll(int direction) {
    cursorPos += direction;
    int stringEnd = strlen(displayString);

    for (int displayPos = 0; displayPos < displayLength; displayPos++) {
        int whichCharacter = displayPos - cursorPos;
        char charToShow;
        if ((whichCharacter >= 0) && (whichCharacter < stringEnd)) {
            charToShow = displayString[whichCharacter];
        } else {
            charToShow = ' ';
        }
        writeCharacter(charToShow, displayPos);
    }
    loadDotRegister();
}

void LedDisplay::setString(const char *_displayString) {
    this->displayString = _displayString;
}

const char *LedDisplay::getString() {
    return displayString;
}

int LedDisplay::stringLength() {
    return strlen(displayString);
}

void LedDisplay::setBrightness(uint8_t bright) {
    if (bright > 15) {
        bright = 15;
    }
    loadAllControlRegisters(0b01110000 + bright);
}

void LedDisplay::writeCharacter(char whatCharacter, uint8_t whatPosition) {
    uint8_t thisPosition = whatPosition * 5;

    for (int i = 0; i < 5; i++) {
        dotRegister[thisPosition + i] = Font5x7[((whatCharacter - 0x20) * 5) + i];
    }
}

void LedDisplay::loadControlRegister(uint8_t dataByte) {
    digitalWrite(registerSelect, HIGH);
    digitalWrite(chipEnable, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, dataByte);
    digitalWrite(chipEnable, HIGH);
}

void LedDisplay::loadAllControlRegisters(uint8_t dataByte) {
    int chip_count = displayLength / 4;

    for (int i = 0; i < chip_count; i++) {
        loadControlRegister(0b10000001);
    }

    loadControlRegister(dataByte);
    loadControlRegister(0b10000000);
}

void LedDisplay::loadDotRegister() {
    int maxData = displayLength * 5;

    digitalWrite(registerSelect, LOW);
    digitalWrite(chipEnable, LOW);
    for (int i = 0; i < maxData; i++) {
        shiftOut(dataPin, clockPin, MSBFIRST, dotRegister[i]);
    }
    digitalWrite(chipEnable, HIGH);
}

int LedDisplay::version(void) {
    return 4;
}
