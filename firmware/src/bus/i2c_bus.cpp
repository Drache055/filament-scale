#include "i2c_bus.h"

#include <Wire.h>
#include <freertos/semphr.h>

#include "pins.h"

static SemaphoreHandle_t i2cMutex = nullptr;

void i2cBusBegin() {
  i2cMutex = xSemaphoreCreateMutex();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
}

bool i2cBusLock(uint32_t timeoutMs) {
  return xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void i2cBusUnlock() {
  xSemaphoreGive(i2cMutex);
}
