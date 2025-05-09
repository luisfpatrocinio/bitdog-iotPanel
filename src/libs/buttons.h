#ifndef BUTTONS_H
#define BUTTONS_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"

// Buttons
#define BTA 5
#define BTB 6

void initButtons();
void setButtonCallback(void (*handleButtonGpioEvent)(uint gpio, uint32_t events));
bool isButtonPressed(uint gpio);

#endif