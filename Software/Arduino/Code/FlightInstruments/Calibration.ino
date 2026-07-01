// Calibration.ino - calibracion del bias del giroscopio al arranque.

// Bias del giroscopio
float gyroBiasX = 0, gyroBiasY = 0, gyroBiasZ = 0;

void calibrateGyro() {
  Serial.println("CAL: Calibrando gyro NO MOVER EL SENSOR...");
  neopixelWrite(PIN_LED_RGB, 30, 15, 0);  // ambar mientras se calibra

  float sumX = 0, sumY = 0, sumZ = 0;
  float sumSqX = 0, sumSqY = 0, sumSqZ = 0;

  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    float gx = readWord(MPU9250_ADDR, 0x43) / GYRO_SCALE;
    float gy = readWord(MPU9250_ADDR, 0x45) / GYRO_SCALE;
    float gz = readWord(MPU9250_ADDR, 0x47) / GYRO_SCALE;

    sumX += gx;  sumY += gy;  sumZ += gz;
    sumSqX += gx * gx;
    sumSqY += gy * gy;
    sumSqZ += gz * gz;

    delay(10);
  }

  float meanX = sumX / CALIBRATION_SAMPLES;
  float meanY = sumY / CALIBRATION_SAMPLES;
  float meanZ = sumZ / CALIBRATION_SAMPLES;

  // Varianza: Var = E[X^2] - E[X]^2
  float varX = (sumSqX / CALIBRATION_SAMPLES) - (meanX * meanX);
  float varY = (sumSqY / CALIBRATION_SAMPLES) - (meanY * meanY);
  float varZ = (sumSqZ / CALIBRATION_SAMPLES) - (meanZ * meanZ);
  float maxVariance = max(varX, max(varY, varZ));

  Serial.print("CAL: Varianza X="); Serial.print(varX, 4);
  Serial.print("  Y="); Serial.print(varY, 4);
  Serial.print("  Z="); Serial.print(varZ, 4);
  Serial.print("  (umbral="); Serial.print(CALIBRATION_VARIANCE_THRESHOLD, 1);
  Serial.println(")");

  if (maxVariance < CALIBRATION_VARIANCE_THRESHOLD) {
    // Sensor en reposo durante toda la captura: la media es un bias fiable
    gyroBiasX = meanX;
    gyroBiasY = meanY;
    gyroBiasZ = meanZ;

    Serial.print("CAL: [LIVE] Bias X="); Serial.print(gyroBiasX, 4);
    Serial.print("  Y="); Serial.print(gyroBiasY, 4);
    Serial.print("  Z="); Serial.println(gyroBiasZ, 4);
  } else {
    // Movimiento detectado durante la calibracion. Como fallback se usan
    // valores tipicos obtenidos previamente en prueba controlada, y el 
    // filtro mahony convergera al valor real mas rapido.
    gyroBiasX = -0.2359;
    gyroBiasY =  0.3017;
    gyroBiasZ = -0.5030;
    Serial.println("CAL: AVISO: movimiento detectado, usando bias por defecto");
  }

  Serial.println("CAL: Calibracion del gyro completada.");
}
