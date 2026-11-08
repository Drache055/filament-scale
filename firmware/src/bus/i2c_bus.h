#pragma once
#include <Arduino.h>

// Gemeinsamer I²C-Bus für Touch und PN532, per Mutex geschützt.
void i2cBusBegin();
bool i2cBusLock(uint32_t timeoutMs = 50);
void i2cBusUnlock();
