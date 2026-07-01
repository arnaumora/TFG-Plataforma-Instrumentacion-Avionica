// IMU.ino - lectura cruda del MPU-9250 (accel + gyro) y del AK8963 (mag).
// La fusion a Euler/quaternion se hace en SensorFusion.ino.


// Ajuste de sensibilidad de fabrica (ASA) del magnetometro
float magCalX = 1.0f, magCalY = 1.0f, magCalZ = 1.0f;

// Helpers I2C
void writeRegister(byte addr, byte reg, byte val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

int16_t readWord(byte addr, byte reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (byte)2);
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  return (msb << 8) | lsb;
}

// Inicializacion
bool initIMU() {
  // Sale del modo sleep
  writeRegister(MPU9250_ADDR, 0x6B, 0x00);
  delay(100);

  // Comprueba WHO_AM_I - debe ser 0x71
  Wire.beginTransmission(MPU9250_ADDR);
  Wire.write(0x75);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU9250_ADDR, (byte)1);
  byte whoAmI = Wire.read();

  if (whoAmI != 0x71) {
    Serial.print("IMU ERROR: WHO_AM_I = 0x");
    Serial.println(whoAmI, HEX);
    return false;
  }

  writeRegister(MPU9250_ADDR, 0x1C, 0x08);  // Accel +/-4g
  writeRegister(MPU9250_ADDR, 0x1B, 0x08);  // Gyro +/-500deg/s
  writeRegister(MPU9250_ADDR, 0x1A, 0x03);  // DLPF ~42 Hz
  writeRegister(MPU9250_ADDR, 0x37, 0x02);  // bypass I2C para poder hablar con el AK8963
  delay(100);

  // Comprueba la ID del magnetometro (WIA = 0x48)
  Wire.beginTransmission(AK8963_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(false);
  Wire.requestFrom(AK8963_ADDR, (byte)1);
  byte magId = Wire.read();

  if (magId != 0x48) {
    Serial.print("MAG ERROR: WIA = 0x");
    Serial.println(magId, HEX);
    return false;
  }

  // Lee el ASA desde la Fuse ROM del AK8963 para corregir la sensibilidad
  // de cada eje. Formula del datasheet MS1356-E-02 sec.8.3.11.
  writeRegister(AK8963_ADDR, 0x0A, 0x00);   // power down
  delay(10);
  writeRegister(AK8963_ADDR, 0x0A, 0x0F);   // modo Fuse ROM
  delay(10);

  Wire.beginTransmission(AK8963_ADDR);
  Wire.write(0x10);                          // registro ASAX
  Wire.endTransmission(false);
  Wire.requestFrom(AK8963_ADDR, (byte)3);

  uint8_t asaX = Wire.read();
  uint8_t asaY = Wire.read();
  uint8_t asaZ = Wire.read();

  magCalX = ((float)(asaX - 128) / 256.0f) + 1.0f;
  magCalY = ((float)(asaY - 128) / 256.0f) + 1.0f;
  magCalZ = ((float)(asaZ - 128) / 256.0f) + 1.0f;

  Serial.print("MAG ASA: X="); Serial.print(asaX);
  Serial.print("  Y="); Serial.print(asaY);
  Serial.print("  Z="); Serial.println(asaZ);
  Serial.print("MAG Cal: X="); Serial.print(magCalX, 4);
  Serial.print("  Y="); Serial.print(magCalY, 4);
  Serial.print("  Z="); Serial.println(magCalZ, 4);

  writeRegister(AK8963_ADDR, 0x0A, 0x00);   // power down otra vez para reconfigurar
  delay(10);
  writeRegister(AK8963_ADDR, 0x0A, 0x16);   // 16-bit, continuo a 100 Hz
  delay(100);

  Serial.println("IMU: MPU-9250 + AK8963 OK");

  return true;
}

// Lectura de los 9 ejes
void readIMU(RawIMU &imu) {
  // Burst de 14 bytes desde 0x3B: accel (6) + temp (2) + gyro (6)
  Wire.beginTransmission(MPU9250_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU9250_ADDR, (byte)14);

  // MPU-9250 es big-endian (MSB primero, LSB despues)
  uint8_t msb, lsb;
  msb = Wire.read(); lsb = Wire.read();
  int16_t axRaw = (msb << 8) | lsb;
  msb = Wire.read(); lsb = Wire.read();
  int16_t ayRaw = (msb << 8) | lsb;
  msb = Wire.read(); lsb = Wire.read();
  int16_t azRaw = (msb << 8) | lsb;

  // La temperatura queda entre accel y gyro. No se usa pero hay que
  // leerla igual porque va en el mismo paquete
  msb = Wire.read(); lsb = Wire.read();
  int16_t tempRaw = (msb << 8) | lsb;
  (void)tempRaw;

  msb = Wire.read(); lsb = Wire.read();
  int16_t gxRaw = (msb << 8) | lsb;
  msb = Wire.read(); lsb = Wire.read();
  int16_t gyRaw = (msb << 8) | lsb;
  msb = Wire.read(); lsb = Wire.read();
  int16_t gzRaw = (msb << 8) | lsb;

  imu.ax = axRaw / ACCEL_SCALE;
  imu.ay = ayRaw / ACCEL_SCALE;
  imu.az = azRaw / ACCEL_SCALE;

  imu.gx = (gxRaw / GYRO_SCALE) - gyroBiasX;
  imu.gy = (gyRaw / GYRO_SCALE) - gyroBiasY;
  imu.gz = (gzRaw / GYRO_SCALE) - gyroBiasZ;

  // AK8963: 7 bytes desde 0x03 (6 de datos + ST2). El ST2 hay que leerlo
  // siempre para que el mag prepare la siguiente muestra
  Wire.beginTransmission(AK8963_ADDR);
  Wire.write(0x03);
  Wire.endTransmission(false);
  Wire.requestFrom(AK8963_ADDR, (byte)7);

  // El AK8963 es little-endian (LSB primero, MSB despues)
  uint8_t l, h;
  l = Wire.read(); h = Wire.read();
  int16_t mxRaw = l | (h << 8);
  l = Wire.read(); h = Wire.read();
  int16_t myRaw = l | (h << 8);
  l = Wire.read(); h = Wire.read();
  int16_t mzRaw = l | (h << 8);
  uint8_t st2 = Wire.read();  // ST2, obligatorio leerlo
  (void)st2;

// Aplicacion correcciones
  float mxUT = mxRaw * MAG_SCALE * magCalX;
  float myUT = myRaw * MAG_SCALE * magCalY;
  float mzUT = mzRaw * MAG_SCALE * magCalZ;

  imu.mx = (mxUT - MAG_OFFSET_X) * MAG_SCALE_X;
  imu.my = (myUT - MAG_OFFSET_Y) * MAG_SCALE_Y;
  imu.mz = (mzUT - MAG_OFFSET_Z) * MAG_SCALE_Z;
}
