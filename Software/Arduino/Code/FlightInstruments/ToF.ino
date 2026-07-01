// ToF.ino - driver del VL53L1X (TOF400C)

#include <VL53L1X.h>

VL53L1X tofSensor;

bool initToF() {
  tofSensor.setTimeout(500);

  if (!tofSensor.init()) {
    Serial.println("TOF ERROR: VL53L1X no encontrado");
    Serial.println("  Revisar: VCC=3.3V, SDA=GPIO8, SCL=GPIO9, XSHUT en alto");
    return false;
  }

  tofSensor.setDistanceMode(VL53L1X::Long);       // hasta 4 m
  tofSensor.setMeasurementTimingBudget(TOF_TIMING_BUDGET_US);
  tofSensor.startContinuous(TOF_CONTINUOUS_MS);

  Serial.println("TOF: VL53L1X OK");
  return true;
}

// Lectura no bloqueante. Si no hay muestra nueva, no se toca data.
void readToF(ToFData &data) {
  if (!data.ok) return;

  if (!tofSensor.dataReady()) return;

  uint16_t dist = tofSensor.read(false);  // false = no bloquear

  if (tofSensor.timeoutOccurred()) {
    data.valid = false;
    return;
  }

  data.distance_mm = dist;
  data.valid = (dist >= TOF_DEAD_ZONE_MM);
}
