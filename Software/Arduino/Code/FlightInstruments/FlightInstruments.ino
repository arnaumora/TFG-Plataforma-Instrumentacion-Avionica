// FlightInstruments.ino - Sketch principal (transmisor)
//
// TFG: Diseno e implementacion de una plataforma experimental de
//       instrumentacion avionica.

#include <Wire.h>
#include "Config.h"
#include "SensorData.h"

// Estructuras de datos
RawIMU     rawIMU;
FlightData flightData;
BaroData   baroData;
GPSData    gpsData;
RadarData  radarData;
ToFData    tofData;

// Timers para scheduling cooperativo. Cada subsistema corre a su
// frecuencia natural sin bloquear a los demas. El loop principal
// solo dispara la tarea que toca segun el reloj.
unsigned long lastSensorRead    = 0;
unsigned long lastBaroRead      = 0;
unsigned long lastSerialPrint   = 0;
unsigned long lastTelemetrySend = 0;

bool radioAvailable = false;

// Contadores de transmision, definidos en Telemetry.ino
extern uint32_t txCount;
extern uint32_t txFailCount;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("  Flight Instruments");
  Serial.println("  ESP32-S3 + MPU-9250 + BMP280 + VL53L1X");
  Serial.println("  + NEO-6M + LD2410C + NRF24L01+PA+LNA");
  Serial.println("  Filtro: Mahony MARG (9 ejes)");

  neopixelWrite(PIN_LED_RGB, 30, 0, 0);  // rojo = booting

  // XSHUT del ToF en alto para que el sensor arranque
  pinMode(TOF_SHUT_PIN, OUTPUT);
  digitalWrite(TOF_SHUT_PIN, HIGH);
  delay(10);

  // Interrupciones (no usadas, solo se dejan los pines configurados)
  pinMode(TOF_IRQ_PIN, INPUT);
  pinMode(RADAR_IRQ_PIN, INPUT);
  pinMode(MPU_IRQ_PIN, INPUT);

  // Bus I2C compartido
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_CLOCK);
  Serial.println("I2C OK (400 kHz)");

  // IMU. Es el unico sensor critico: si falla se bloquea el arranque
  if (!initIMU()) {
    Serial.println("FATAL: fallo al inicializar el IMU");
    while (true) {  // parpadeo rojo perpetuo
      neopixelWrite(PIN_LED_RGB, 30, 0, 0); delay(300);
      neopixelWrite(PIN_LED_RGB, 0, 0, 0);  delay(300);
    }
  }

  // Calibracion del giroscopio
  calibrateGyro();

  initFusion(flightData);

  // Semilla del cuaternion con una muestra accel+mag para que el
  // filtro Mahony arranque mas cerca de la orientacion real
  delay(20);
  readIMU(rawIMU);
  seedQuaternionFromIMU(rawIMU);

  // Sensores no criticos: si alguno falla el sistema sigue, solo
  // queda marcado con ok=false y la palabra ARINC correspondiente
  // se transmite con SSM=NO_DATA
  baroData.ok = initBarometer();
  if (!baroData.ok) Serial.println("AVISO: barometro offline, sin altitud barometrica");

  tofData.ok = initToF();
  if (!tofData.ok) Serial.println("AVISO: ToF offline, sin distancia al suelo");

  initGPS();  // el flag ok se activa en el primer readGPS() valido
  radarData.ok = initRadar();

  radioAvailable = initTelemetry();
  if (!radioAvailable) Serial.println("AVISO: radio offline, sigue sin telemetria");

#ifdef CALIBRATE_MAG
  runMagCalOperational();   // solo la LLAMADA; captura y luego halt
#endif

  neopixelWrite(PIN_LED_RGB, 0, 30, 0);  // verde = listo
  Serial.println("\nSistema listo. Streaming...\n");
}

void loop() {
  unsigned long nowUs = micros();
  unsigned long nowMs = millis();

  // Alimentamos buffers de UART cada iteracion (no bloqueante).
  // Si se descuida aunque sea un ciclo, la UART se satura y se pierden frames.
  feedGPS();
  feedRadar();

  // IMU + fusion a 100 Hz
  if (nowUs - lastSensorRead >= SENSOR_INTERVAL_US) {
    lastSensorRead = nowUs;
    readIMU(rawIMU);
    updateFusion(rawIMU, flightData);
  }

  // Barometro a 10 Hz
  if (baroData.ok && (nowMs - lastBaroRead >= BARO_INTERVAL_MS)) {
    lastBaroRead = nowMs;
    readBarometer(baroData);
  }

  // ToF (no bloqueante, solo actualiza si hay muestra nueva)
  readToF(tofData);

  // GPS
  readGPS(gpsData);

  // Radar (recibe la velocidad GPS para filtrar detecciones estaticas o en movimiento)
  float platformSpeed = gpsData.fix ? gpsData.speed_kmh : 0.0f;
  readRadar(radarData, platformSpeed);

  // Telemetria a 20 Hz
  if (radioAvailable && (nowMs - lastTelemetrySend >= TELEMETRY_INTERVAL_MS)) {
    lastTelemetrySend = nowMs;
    sendTelemetry(flightData, baroData, gpsData, radarData, tofData);
  }


  // Debug por serial a 10 Hz. Solo se compila si DEBUG_SERIAL esta
  // definido (ver Config.h) ya que el transmisor normalmente no esta
  // conectado por USB, asi se ahorra computacion.
#ifdef DEBUG_SERIAL
  if (nowMs - lastSerialPrint >= SERIAL_PRINT_INTERVAL) {
    lastSerialPrint = nowMs;

    // Magnitud del campo magnetico para diagnostico
    float magMag = sqrtf(rawIMU.mx * rawIMU.mx
                       + rawIMU.my * rawIMU.my
                       + rawIMU.mz * rawIMU.mz);

    float lossPct = (txCount > 0) ? (txFailCount * 100.0f / txCount) : 0.0f;
    Serial.printf(
      "P:%.1f R:%.1f H:%.1f |Mag:%.1fuT| Alt:%.1f VS:%.1f | Sat:%d %s"
      " | Rdr:%ucm ToF:%umm | TX:%lu L:%.1f%%\n",
      flightData.pitch, flightData.roll, flightData.heading, magMag,
      baroData.altitude_m, baroData.vspeed_mps,
      gpsData.satellites, gpsData.fix ? "Fix" : "NoFix",
      radarData.bestDist_cm, tofData.distance_mm,
      txCount, lossPct
    );
  }
#endif
}



//  Calibracion del magnetometro 
// Calibracion hard/soft iron con el sistema completo en operacion.

#ifdef CALIBRATE_MAG
extern float magCalX, magCalY, magCalZ;   // ASA de fabrica, definidos en IMU.ino

void runMagCalOperational() {
  Serial.println("\nCAL MAG: Esperando fix GPS con el sistema en operacion...");
  unsigned long tStart = millis(), tWarmTx = 0, tWarmPrint = 0;

  // Bucle que no termina hasta que el GPS obtenga un Fix válido
  while (!gpsData.fix) {
    // Reproduce el loop normal para mantener el sistema vivo
    feedGPS(); feedRadar();
    readIMU(rawIMU);
    updateFusion(rawIMU, flightData);
    readBarometer(baroData);
    readToF(tofData);
    readGPS(gpsData);
    readRadar(radarData, gpsData.fix ? gpsData.speed_kmh : 0.0f);
    
    // Comprobamos radioAvailable antes de transmitir
    if (radioAvailable && (millis() - tWarmTx >= TELEMETRY_INTERVAL_MS)) {
      tWarmTx = millis();
      sendTelemetry(flightData, baroData, gpsData, radarData, tofData);
    }

    // Imprimir el estado cada segundo para ver el progreso
    if (millis() - tWarmPrint > 1000) {
      tWarmPrint = millis();
      Serial.print("WARMUP: Esperando GPS... Tiempo transcurrido: ");
      Serial.print((millis() - tStart) / 1000UL);
      Serial.print("s  GPS: NoFix");
      Serial.print("  Sat: ");
      Serial.println(gpsData.satellites);
    }
  }

  Serial.println("\nGPS Fix conseguido. Dando 5 segundos de estabilizacion final...");
  delay(5000);

  Serial.println("\nCAL MAG: rotar LENTO en los 3 ejes durante 30 s");
  for (int i = 5; i > 0; i--) { Serial.print(i); Serial.print("... "); delay(1000); }
  Serial.println("\n>>> ROTAR AHORA <<<\n");

  float minX=1e6f, minY=1e6f, minZ=1e6f;
  float maxX=-1e6f, maxY=-1e6f, maxZ=-1e6f;
  unsigned long t0 = millis(), tTx = 0, tPrint = 0;
  uint32_t n = 0;

  while (millis() - t0 < 30000UL) {
    // Reproduce el loop normal: radio TX a 20 Hz + todos los sensores activos
    // Asi la calibracion se hace con los sensores en funcionamiento
    feedGPS(); feedRadar();
    readIMU(rawIMU);
    updateFusion(rawIMU, flightData);
    readBarometer(baroData);
    readToF(tofData);
    readGPS(gpsData);
    readRadar(radarData, gpsData.fix ? gpsData.speed_kmh : 0.0f);
    
    // Comprobamos radioAvailable antes de transmitir
    if (radioAvailable && (millis() - tTx >= TELEMETRY_INTERVAL_MS)) {
      tTx = millis();
      sendTelemetry(flightData, baroData, gpsData, radarData, tofData);
    }

    // Mag SOLO con ASA (sin offset/scale)
    Wire.beginTransmission(AK8963_ADDR); Wire.write(0x03); Wire.endTransmission(false);
    Wire.requestFrom(AK8963_ADDR, (byte)7);
    if (Wire.available() >= 7) {
      int16_t xr = Wire.read() | (Wire.read() << 8);
      int16_t yr = Wire.read() | (Wire.read() << 8);
      int16_t zr = Wire.read() | (Wire.read() << 8);
      (void)Wire.read();  // ST2, obligatorio leerlo
      float mx = xr * MAG_SCALE * magCalX;
      float my = yr * MAG_SCALE * magCalY;
      float mz = zr * MAG_SCALE * magCalZ;
      if (mx<minX) minX=mx;  if (mx>maxX) maxX=mx;
      if (my<minY) minY=my;  if (my>maxY) maxY=my;
      if (mz<minZ) minZ=mz;  if (mz>maxZ) maxZ=mz;
      n++;
    }

    if (millis() - tPrint > 1000) {
      tPrint = millis();
      Serial.print("CAL t-"); Serial.print((30000UL - (millis()-t0)) / 1000UL);
      Serial.print("s  N="); Serial.println(n);
    }
  }

  // Offset = centro del elipsoide. escala = correccion soft iron por eje
  float offX=(maxX+minX)*0.5f, offY=(maxY+minY)*0.5f, offZ=(maxZ+minZ)*0.5f;
  float rX=(maxX-minX)*0.5f, rY=(maxY-minY)*0.5f, rZ=(maxZ-minZ)*0.5f;
  float avg=(rX+rY+rZ)/3.0f;
  float scX=(rX>0.1f)?avg/rX:1.0f, scY=(rY>0.1f)?avg/rY:1.0f, scZ=(rZ>0.1f)?avg/rZ:1.0f;

  Serial.println("\nCALIBRACION COMPLETADA");
  Serial.print("Muestras: "); Serial.println(n);
  Serial.print("Magnitud media: "); Serial.print(avg,1);
  Serial.println(" uT  (esperado ~46 uT, WMM2025 Terrassa)");
  Serial.println("\nPEGAR EN Config.h (sustituir las 6 lineas existentes):\n");
  Serial.print("#define MAG_OFFSET_X    "); Serial.print(offX,4); Serial.println("f");
  Serial.print("#define MAG_OFFSET_Y    "); Serial.print(offY,4); Serial.println("f");
  Serial.print("#define MAG_OFFSET_Z    "); Serial.print(offZ,4); Serial.println("f");
  Serial.print("#define MAG_SCALE_X     "); Serial.print(scX,4);  Serial.println("f");
  Serial.print("#define MAG_SCALE_Y     "); Serial.print(scY,4);  Serial.println("f");
  Serial.print("#define MAG_SCALE_Z     "); Serial.print(scZ,4);  Serial.println("f");

  while (true) {  // halt: no seguir al loop normal. Reflashear sin CALIBRATE_MAG.
    neopixelWrite(PIN_LED_RGB, 0, 30, 0); delay(500);
    neopixelWrite(PIN_LED_RGB, 0, 0, 0);  delay(500);
  }
}
#endif