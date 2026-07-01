// Barometer.ino - driver del BMP280 (presion + temperatura + altitud MSL)

#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;

// Estado para calcular la velocidad vertical por diferencias finitas
static float prevAltitude = 0.0f;
static unsigned long prevBaroTime = 0;

bool initBarometer() {
  if (!bmp.begin(BMP280_ADDR)) {
    Serial.println("BARO ERROR: BMP280 no encontrado en 0x76");
    Serial.println("  Revisar: VCC=3.3V, SDA=GPIO8, SCL=GPIO9, SDO=GND");
    return false;
  }

  // Configuracion para 10 Hz de lectura (BARO_INTERVAL_MS)
  // Parametros segun BMP280 datasheet Bosch BST-BMP280-DS001-19 sec.3.5.
  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,
    Adafruit_BMP280::SAMPLING_X16,
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_63
  );

  // Base para el calculo de velocidad vertical
  prevAltitude = bmp.readAltitude(SEA_LEVEL_HPA);
  prevBaroTime = millis();

  Serial.println("BARO: BMP280 OK");
  Serial.print("  Altitud inicial: ");
  Serial.print(prevAltitude, 1);
  Serial.println(" m MSL");

  return true;
}

void readBarometer(BaroData &baro) {
  baro.pressure_hPa = bmp.readPressure() / 100.0f;  // Pa -> hPa
  baro.temperature_C = bmp.readTemperature();
  baro.altitude_m = bmp.readAltitude(SEA_LEVEL_HPA);

  // Velocidad vertical por diferencia finita, suavizada con EMA.
  // El IIR interno del BMP280 reduce el ruido de presion pero queda
  // la discretizacion del LSB (~0.16 Pa). Sin el EMA el VSI se ve
  // saltar en pasos discretos al derivar.
  unsigned long now = millis();
  float dt = (now - prevBaroTime) / 1000.0f;

  if (dt > 0.05f && dt < 2.0f) {  // protege contra div/0 y dt anomalos
    float rawVS = (baro.altitude_m - prevAltitude) / dt;
    baro.vspeed_mps = baro.vspeed_mps * (1.0f - VSPEED_ALPHA)
                    + rawVS * VSPEED_ALPHA;
  }

  prevAltitude = baro.altitude_m;
  prevBaroTime = now;
}
