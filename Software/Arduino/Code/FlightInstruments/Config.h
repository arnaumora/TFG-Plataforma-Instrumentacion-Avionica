// Config.h - constantes de hardware y parametros de ajuste

#ifndef CONFIG_H
#define CONFIG_H

// DEBUG
// #define DEBUG_SERIAL

// MAG Calibration
// #define CALIBRATE_MAG 


// Definicion Pines

// I2C compartido (MPU-9250 + BMP280 + VL53L1X)
#define PIN_SDA        8
#define PIN_SCL        9

// LED RGB onboard
#define PIN_LED_RGB    48

// NRF24L01+PA+LNA (SPI)
#define NRF_SCK        39
#define NRF_MISO       40
#define NRF_MOSI       41
#define NRF_CE         2
#define NRF_CSN        42
#define NRF_IRQ        38

// GPS NEO-6M (UART1)
#define GPS_RX_PIN     21   // ESP32 RX <- GPS TX
#define GPS_TX_PIN     47   // ESP32 TX -> GPS RX
#define GPS_BAUD       9600

// HLK-LD2410C radar (UART2)
#define RADAR_RX_PIN   17
#define RADAR_TX_PIN   18
#define RADAR_BAUD     256000

// Interrupciones / control (no usadas en el codigo, pero asignadas)
#define TOF_SHUT_PIN   11   // XSHUT del TOF400C
#define TOF_IRQ_PIN    10   // INT del TOF400C
#define RADAR_IRQ_PIN  16
#define MPU_IRQ_PIN    4

// Direcciones I2C
#define MPU9250_ADDR   0x68   // AD0 a GND
#define AK8963_ADDR    0x0C   // magnetometro en el MPU-9250
#define BMP280_ADDR    0x76   // SDO a GND
#define VL53L1X_ADDR   0x29   // direccion por defecto del TOF400C
#define I2C_CLOCK      400000 // 400 kHz (Fast Mode)

// Rangos del MPU-9250
// Factores sacados del datasheet PS-MPU-9250A-01 Rev 1.1
#define ACCEL_SCALE    8192.0  // LSB/g     a +/-4g
#define GYRO_SCALE     65.5    // LSB/(deg/s) a +/-500deg/s
#define MAG_SCALE      0.15    // uT/LSB    en 16 bits

// BMP280
#define SEA_LEVEL_HPA      1013.25f   // presion de referencia ISA
#define VSPEED_ALPHA       0.2f      // suavizado EMA de la velocidad vertical

// VL53L1X / TOF400C
#define TOF_TIMING_BUDGET_US   50000   // 50 ms modo Long permite hasta 4 m
#define TOF_CONTINUOUS_MS      50      // 20 Hz
#define TOF_DEAD_ZONE_MM       40      // por debajo de aqui la medida no es fiable

// HLK-LD2410C

// Resolucion del gate.
#define RADAR_GATE_WIDTH_CM    20

// Thresholds por gate (downward-facing)
#define RADAR_SENS_GATE_0      80   // 0.00 - 0.20 m 
#define RADAR_SENS_GATE_1      70    // 0.20 - 0.40 m
#define RADAR_SENS_GATE_2      60    // 0.40 - 0.60 m
#define RADAR_SENS_GATE_3      50    // 0.60 - 0.80 m
#define RADAR_SENS_GATE_FAR    35    // 0.80 - 1.80 m  (gates 4-8)

// Peak-finder (modo enhanced)
#define RADAR_NOISE_FLOOR      40

// Suavizado EMA de la distancia derivada por gate
#define RADAR_DIST_ALPHA       0.40f

#define RADAR_HOVER_SPEED_KMH  2.0f  // por debajo de esta velocidad se considera hover
#define RADAR_ALERT_ENERGY_MIN 30    // energia minima para disparar alerta

// Zona de proximidad en funcion de la distancia
#define RADAR_ZONE_NONE        0
#define RADAR_ZONE_DANGER      1     // 0.00 - 0.60 m  (gates 0-2)
#define RADAR_ZONE_CAUTION     2     // 0.60 - 1.20 m  (gates 3-5)
#define RADAR_ZONE_CLEAR       3     // 1.20 - 1.80 m  (gates 6-8)

// Tasas de muestreo
#define SENSOR_RATE_HZ       100      // IMU + fusion
#define SENSOR_INTERVAL_US   (1000000 / SENSOR_RATE_HZ)

#define BARO_RATE_HZ         10
#define BARO_INTERVAL_MS     (1000 / BARO_RATE_HZ)

#define RADAR_PRINT_HZ       2
#define RADAR_PRINT_INTERVAL (1000 / RADAR_PRINT_HZ)

#define SERIAL_PRINT_HZ      10
#define SERIAL_PRINT_INTERVAL (1000 / SERIAL_PRINT_HZ)

// Calibracion del giroscopio
#define CALIBRATION_SAMPLES              500
#define CALIBRATION_VARIANCE_THRESHOLD   2.0

// Mahony
#define MAHONY_KP              3.0f
#define MAHONY_KI              1.0f


// CALIBRACION DEL MAGNETOMETRO (Hard iron / Soft iron)

// Hard iron offsets (uT) 
#define MAG_OFFSET_X    10.6264f
#define MAG_OFFSET_Y    -2.6479f
#define MAG_OFFSET_Z    -30.1492f

// Soft iron scales (adimensional)
#define MAG_SCALE_X     1.0157f
#define MAG_SCALE_Y     0.9977f
#define MAG_SCALE_Z     0.9884f

// CORRECCIONES DE HEADING
//
// Declinacion magnetica
// Diferencia entre el norte magnetico (lo que mide el sensor) y el
// norte geografico verdadero. Para obtener el valor real visitar la web
// (https://www.ncei.noaa.gov/products/world-magnetic-model)

#define MAG_DECLINATION_DEG    1.95f

// Offset entre el "forward" del IMU y el "forward" de la PCB
// Si la IMU esta rotada respecto a la placa, el heading
//  tendra un offset constante respecto al norte real.
// Tambien se puede usar para corregir errores constantes 
// que no provienen de la orientacion de la PCB

#define HEADING_BODY_OFFSET_DEG    -5.0f

// Calibracion actitud 
#define PITCH_MOUNT_OFFSET_DEG    0.6f
#define ROLL_MOUNT_OFFSET_DEG     0.2f


// NRF24L01
#define NRF_CHANNEL            108
#define NRF_DATA_RATE          RF24_250KBPS
#define NRF_PA_LEVEL           RF24_PA_HIGH
#define NRF_RETRY_DELAY        5
#define NRF_RETRY_COUNT        15

#define NRF_ADDR_TX            "SENSR"
#define NRF_ADDR_RX            "CNTRL"

#define TELEMETRY_RATE_HZ      20
#define TELEMETRY_INTERVAL_MS  (1000 / TELEMETRY_RATE_HZ)

#endif
