#pragma once
#include "core/sensor_core.h"

// Treiber-Schnittstelle für den Wägesensor. Ein neuer Sensor braucht nur eine weitere Klasse (sensor_*.cpp),
// einen Eintrag in SensorType (core/sensor_core.h) und einen Fall in sensorCreate.
class LoadSensor {
 public:
  virtual ~LoadSensor() = default;
  virtual bool begin() = 0;           // Initialisierung; false = Sensor nicht gefunden (wird später erneut versucht)
  virtual bool ready() = 0;           // neuer Messwert liegt bereit
  virtual bool read(long &raw) = 0;   // Rohwert (vorzeichenbehaftet, 24 Bit); false = Lesefehler
};

LoadSensor *sensorCreate(const SensorConfig &cfg);
