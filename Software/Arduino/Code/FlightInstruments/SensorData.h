// SensorData.h - estructuras de datos compartidas entre los archivos.

#ifndef SENSORDATA_H
#define SENSORDATA_H

// IMU sin procesar (MPU-9250 + AK8963)
struct RawIMU {
  float ax, ay, az;   // acelerometro (g)
  float gx, gy, gz;   // giroscopio (deg/s), ya con bias restado
  float mx, my, mz;   // magnetometro (uT), ya con ASA aplicado
};

// AHRS ya fusionado
struct FlightData {
  float pitch;        // -90 a +90deg (nariz arriba positivo)
  float roll;         // -180 a +180deg (ala derecha abajo positivo)
  float heading;      // 0 a 360deg (norte magnetico = 0)
  float q0, q1, q2, q3;  // cuaternion
  unsigned long lastUpdate;
};

// Barometro (BMP280)
struct BaroData {
  float pressure_hPa;
  float temperature_C;
  float altitude_m;      // altitud barometrica MSL
  float vspeed_mps;      // velocidad vertical filtrada con EMA
  bool  ok;
};

// GPS (NEO-6M)
struct GPSData {
  double lat;
  double lon;
  float  altitude_m;
  float  speed_kmh;
  float  course_deg;
  uint8_t satellites;
  bool   fix;            // posicion valida
  bool   ok;             // UART recibiendo datos
};

// Radar (HLK-LD2410C)
struct RadarData {
  bool     presence;
  bool     movingDetected;
  uint16_t movingDist_cm;
  uint8_t  movingEnergy;
  bool     stationaryDetected;
  uint16_t stationaryDist_cm;
  uint8_t  stationaryEnergy;
  bool     ok;
  bool     platformStationary;
  uint16_t bestDist_cm;
  uint8_t  maxEnergy;
  uint8_t  proximityZone;
  bool     movingTargetAlert;
};

// Time-of-Flight (VL53L1X / TOF400C)
struct ToFData {
  uint16_t distance_mm;
  bool     valid;
  bool     ok;
};

// Paquete de telemetria ARINC 429 sobre NRF24L01
//
// El paquete ocupa 32 bytes (8 palabras x 32 bits = payload maximo del
// NRF24L01) y se envia a 20 Hz.
struct ARINC429Packet {
  uint32_t words[8];
};

// Slot vacio: una palabra con label 0 y SSM = NO_DATA, asi el receptor
// la ignora limpiamente. Lo usa el planificador cuando no hay nada que
// mandar en un slot
#define A429_EMPTY_WORD          0x20000000UL  // SSM=01 (no data), todo cero

// Layout del campo discrete de la palabra de status (19 bits utiles)
#define A429_STATUS_SATS_MASK       0x0000F
#define A429_STATUS_FIX             0x00010
#define A429_STATUS_IMU_OK          0x00020
#define A429_STATUS_BARO_OK         0x00040
#define A429_STATUS_RADAR_OK        0x00080
#define A429_STATUS_TOF_OK          0x00100
#define A429_STATUS_RADAR_ZONE_SH   9
#define A429_STATUS_RADAR_ZONE_MASK 0x00600
#define A429_STATUS_RADAR_ALERT     0x00800
#define A429_STATUS_PLAT_MOVING     0x01000
#define A429_STATUS_SEQ_SH          13
#define A429_STATUS_SEQ_MASK        0x7E000

// Codificacion de la posicion GNSS en ARINC 429 BNR
// Valor de referencia para mejorar la medida
#define GPS_LAT_REF_DEG       41.0                  // referencia de latitud
#define GPS_LON_REF_DEG        1.0                  // referencia de longitud
#define A429_LSB_LATLON_DEG   (1.0f / 262144.0f)    // ~3.81e-6 deg/LSB (~0.42 m)

#endif