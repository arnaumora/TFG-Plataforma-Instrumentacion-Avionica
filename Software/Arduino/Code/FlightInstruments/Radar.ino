// Radar.ino - driver del HLK-LD2410C (radar 24 GHz FMCW) en UART2.

#include <MyLD2410.h>

HardwareSerial RadarSerial(2);  // UART2
MyLD2410 radar(RadarSerial);

// Suavizado EMA de la distancia
static float smoothedDist_cm = 0.0f;
static bool  hasSmoothedDist = false;

bool initRadar() {
  RadarSerial.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);

  Serial.print("RADAR: UART2 en RX=GPIO");
  Serial.print(RADAR_RX_PIN);
  Serial.print(", TX=GPIO");
  Serial.print(RADAR_TX_PIN);
  Serial.print(", Baud=");
  Serial.println(RADAR_BAUD);

  if (!radar.begin()) {
    Serial.println("RADAR: AVISO -- handshake LD2410C fallo");
    Serial.println("  Se reintentara en check() desde el loop principal");
    return false;
  }

  Serial.println("RADAR: LD2410C OK");

  // Firmware
  if (radar.requestFirmware()) {
    Serial.print("  Firmware: ");
    Serial.println(radar.getFirmware());
  }

  // Configuracion
  // Las setters requieren el radar en modo configuracion. enhancedMode()
  // y resetParameters() lo gestionan ellos solos, pero
  // setGateParameters() necesita configMode(true) explicito.
  if (!radar.configMode(true)) {
    Serial.println("  AVISO: no se pudo entrar en modo config");
  }

  // Resolucion del gate
  // Modo fino (20 cm/gate, 8 gates, max 1.8 m). La libreria usa
  // true = 0.20 m/gate, false = 0.75 m/gate (segun datasheet
  // LD2410C v1.05 sec.2.2). Para esta aplicacion el rango util
  // es el aterrizaje, donde 1.8 m es suficiente y la resolucion
  // fina es prioritaria sobre el alcance.
  if (radar.requestResolution()) {
    if (radar.getResolution() != RADAR_GATE_WIDTH_CM) {
      Serial.println("  Resolucion: cambiando a 20 cm/gate");
      radar.setResolution(true);   // true = 20 cm, false = 75 cm
      radar.requestReboot();
      delay(1500);                 // espera al reboot del radar
      radar.begin();
      radar.configMode(true);
    } else {
      Serial.print("  Resolucion: ");
      Serial.print(radar.getResolution());
      Serial.println(" cm/gate (max 1.8 m)");
    }
  }

  // Thresholds por gate ajustadas a downward-facing.
  // Gates cercanos: thresholds ALTAS para no saturar con clutter del
  // chasis y vibracion. Gates lejanos: thresholds BAJAS para detectar
  // el suelo sin perderlo a mas distancia.
  radar.setGateParameters(0, RADAR_SENS_GATE_0, RADAR_SENS_GATE_0);
  radar.setGateParameters(1, RADAR_SENS_GATE_1, RADAR_SENS_GATE_1);
  radar.setGateParameters(2, RADAR_SENS_GATE_2, RADAR_SENS_GATE_2);
  radar.setGateParameters(3, RADAR_SENS_GATE_3, RADAR_SENS_GATE_3);
  for (uint8_t g = 4; g <= 8; g++) {
    radar.setGateParameters(g, RADAR_SENS_GATE_FAR, RADAR_SENS_GATE_FAR);
  }
  Serial.println("  Thresholds por gate: configuradas");

  radar.configMode(false);

  // Modo enhanced
  if (radar.enhancedMode(true)) {
    Serial.println("  Modo enhanced: ON (per-gate signals disponibles)");
  } else {
    Serial.println("  Modo enhanced: OFF (solo basic mode)");
  }

  return true;
}

// Procesa los bytes que hayan llegado por UART. Llamar cada iteracion
// del loop para no perder paquetes
void feedRadar() {
  radar.check();
}

// Lee datos y calcula las senales derivadas
// platformSpeed_kmh: velocidad GPS actual, necesaria para decidir si
// se puede confiar en la clasificacion movil/estacionario.
void readRadar(RadarData &data, float platformSpeed_kmh) {
  feedRadar();

  // ok = el radar envia frames
  data.ok = (radar.getFrameCount() > 0);
  if (!data.ok) return;

  // Datos del algoritmo interno del radar
  data.presence = (radar.getStatus() != 0);

  if (radar.movingTargetDetected()) {
    data.movingDetected = true;
    data.movingDist_cm  = (uint16_t)radar.movingTargetDistance();
    data.movingEnergy   = radar.movingTargetSignal();
  } else {
    data.movingDetected = false;
    data.movingDist_cm  = 0;
    data.movingEnergy   = 0;
  }

  if (radar.stationaryTargetDetected()) {
    data.stationaryDetected = true;
    data.stationaryDist_cm  = (uint16_t)radar.stationaryTargetDistance();
    data.stationaryEnergy   = radar.stationaryTargetSignal();
  } else {
    data.stationaryDetected = false;
    data.stationaryDist_cm  = 0;
    data.stationaryEnergy   = 0;
  }

  // La clasificacion Doppler solo es fiable con la plataforma parada
  data.platformStationary = (platformSpeed_kmh < RADAR_HOVER_SPEED_KMH);

  // Mejor estimacion de distancia
  // Tomamos el target con mayor signal (el reflector dominante).
  // Cuando el drone esta parado, lo normal es que el suelo sea
  // stationary; cuando se mueve, el suelo aparece como moving por el
  // Doppler shift. Asi que mirar a ambos cubre los dos casos.
  uint16_t dist_cm = 0;
  uint8_t  energy  = 0;
  bool     valid   = false;

  if (data.stationaryDetected && data.movingDetected) {
    // Los dos a la vez: gana el de mas energia
    if (data.stationaryEnergy >= data.movingEnergy) {
      dist_cm = data.stationaryDist_cm;
      energy  = data.stationaryEnergy;
    } else {
      dist_cm = data.movingDist_cm;
      energy  = data.movingEnergy;
    }
  } else if (data.stationaryDetected) {
    dist_cm = data.stationaryDist_cm;
    energy  = data.stationaryEnergy;
  } else if (data.movingDetected) {
    dist_cm = data.movingDist_cm;
    energy  = data.movingEnergy;
  }

  // Filtro adicional sobre la energia: el algoritmo del chip ya rechaza
  // mucho ruido, pero a veces declara un target valido con signal bajo
  // (en torno a 20-30) que es solo eco residual. Con RADAR_NOISE_FLOOR
  // descartamos esos casos para que la salida sea estable.
  valid = (dist_cm > 0) && (energy >= RADAR_NOISE_FLOOR);

  if (valid) {
    // Suavizado EMA. Reduce el flicker frame-a-frame que aparece
    // cuando el chip alterna entre dos gates adyacentes
    if (!hasSmoothedDist) {
      smoothedDist_cm = (float)dist_cm;
      hasSmoothedDist = true;
    } else {
      smoothedDist_cm = smoothedDist_cm * (1.0f - RADAR_DIST_ALPHA)
                      + (float)dist_cm  * RADAR_DIST_ALPHA;
    }
    data.bestDist_cm = (uint16_t)(smoothedDist_cm + 0.5f);
    data.maxEnergy   = energy;
  } else {
    // Sin target valido: se resetea el filtro para que el siguiente
    // eco arranque limpio
    data.bestDist_cm = 0;
    data.maxEnergy   = 0;
    hasSmoothedDist  = false;
  }

  // Zona de proximidad en base a la distancia con histeresis implicita
  // por el suavizado EMA
  data.proximityZone = RADAR_ZONE_NONE;
  if (data.bestDist_cm > 0 && data.maxEnergy >= RADAR_ALERT_ENERGY_MIN) {
    if (data.bestDist_cm <= 60)        data.proximityZone = RADAR_ZONE_DANGER;
    else if (data.bestDist_cm <= 120)  data.proximityZone = RADAR_ZONE_CAUTION;
    else                               data.proximityZone = RADAR_ZONE_CLEAR;
  }

  // Target movil en zona de aterrizaje. Solo se evalua con la
  // plataforma parada; en movimiento la clasificacion Doppler del
  // chip ve el suelo como "movil" y dispararia falsos positivos.
  data.movingTargetAlert = false;
  if (data.platformStationary && data.movingDetected) {
    if (data.movingDist_cm <= 120 && data.movingEnergy >= RADAR_ALERT_ENERGY_MIN) {
      data.movingTargetAlert = true;
    }
  }
}
