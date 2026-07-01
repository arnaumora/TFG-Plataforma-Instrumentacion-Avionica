// Telemetry.ino - transmisor NRF24L01+PA+LNA con payload ARINC 429 y
// planificador de tasas por etiqueta.

#include <SPI.h>
#include <RF24.h>
#include <ARINC429.h>

RF24 radio(NRF_CE, NRF_CSN);
const byte addrTX[6] = NRF_ADDR_TX;

uint32_t txCount = 0;
uint32_t txFailCount = 0;
static uint8_t txSeqNumber = 0;

// Slots del planificador. Los slots 0-2 (pitch, roll, heading) son
// fijos y van en cada paquete (20 Hz). Los slots 3-7 multiplexan los
// parametros listados aqui segun su periodo nominal
enum SchedSlot {
  SCH_PRES_ALT = 0,
  SCH_VSPEED,
  SCH_RADAR_DIST,
  SCH_RADIO_ALT,
  SCH_GPS_ALT,
  SCH_LATITUDE,
  SCH_LONGITUDE,
  SCH_STATUS,
  SCH_COUNT
};

// Periodo nominal en ms entre dos transmisiones del mismo parametro
static const uint16_t schedPeriodMs[SCH_COUNT] = {
  100,   // PRES_ALT  (10 Hz)
  100,   // VSPEED    (10 Hz)
  100,   // RADAR     (10 Hz)
  100,   // RADIO_ALT (10 Hz)
  200,   // GPS_ALT    (5 Hz)
  200,   // LATITUDE   (5 Hz)
  200,   // LONGITUDE  (5 Hz)
  200,   // STATUS     (5 Hz)
};

static unsigned long schedLastSent[SCH_COUNT] = {0};

bool initTelemetry() {
  SPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI, -1);
  if (!radio.begin(&SPI)) {
    Serial.println("RADIO ERROR: NRF24L01 no detectado");
    return false;
  }

  radio.setPALevel(NRF_PA_LEVEL);
  radio.setDataRate(NRF_DATA_RATE);
  radio.setChannel(NRF_CHANNEL);
  radio.setRetries(NRF_RETRY_DELAY, NRF_RETRY_COUNT);
  radio.setPayloadSize(sizeof(ARINC429Packet));

  radio.openWritingPipe(addrTX);
  radio.stopListening();

  Serial.println("RADIO: NRF24L01+PA+LNA OK (payload ARINC 429)");
  Serial.print("  Canal: ");   Serial.print(radio.getChannel());
  Serial.print("  Payload: "); Serial.print(sizeof(ARINC429Packet));
  Serial.print("B (8 palabras x 32 bits)  Rate: ");
  Serial.print(TELEMETRY_RATE_HZ);
  Serial.println(" Hz");
  Serial.println("  Planificador:");
  Serial.println("    pitch/roll/hdg=20Hz  baro/radar/tof=10Hz");
  Serial.println("    gps/status=5Hz");

  return true;
}

// Construye el campo discrete del status word
static uint32_t buildStatusDiscrete(GPSData &gps, BaroData &baro, RadarData &radarData, ToFData &tofData, uint8_t seq) {
  uint32_t d = 0;
  uint8_t sats = (gps.satellites > 15) ? 15 : gps.satellites;
  d |= (sats & A429_STATUS_SATS_MASK);
  if (gps.fix)             d |= A429_STATUS_FIX;
  if (baro.ok)             d |= A429_STATUS_BARO_OK;
  if (radarData.ok)        d |= A429_STATUS_RADAR_OK;
  if (tofData.ok)          d |= A429_STATUS_TOF_OK;

  d |= ((uint32_t)(radarData.proximityZone & 0x3) << A429_STATUS_RADAR_ZONE_SH);
  if (radarData.movingTargetAlert)    d |= A429_STATUS_RADAR_ALERT;
  if (!radarData.platformStationary)  d |= A429_STATUS_PLAT_MOVING;

  d |= ((uint32_t)(seq & 0x3F) << A429_STATUS_SEQ_SH);
  return d;
}

// Construye la palabra ARINC 429 para un parametro multiplexado
static uint32_t buildScheduledWord(uint8_t schSlot, FlightData &flight, BaroData &baro, GPSData &gpsData, RadarData &radarData, ToFData &tofData, uint8_t seq) {
  switch (schSlot) {
    case SCH_PRES_ALT: {
      uint8_t ssm = baro.ok ? A429_SSM_BNR_NORMAL : A429_SSM_BNR_NO_DATA;
      float altFt = baro.ok ? baro.altitude_m * 3.28084f : 0.0f;
      return ARINC429::buildBNR(A429_LABEL_PRES_ALT, 0, altFt, A429_LSB_PRES_ALT_FT, ssm);
    }
    case SCH_VSPEED: {
      uint8_t ssm = baro.ok ? A429_SSM_BNR_NORMAL : A429_SSM_BNR_NO_DATA;
      float vsFpm = baro.ok ? baro.vspeed_mps * 196.85f : 0.0f;
      return ARINC429::buildBNR(A429_LABEL_VERT_SPEED, 0, vsFpm, A429_LSB_VSPEED_FPM, ssm);
    }
    case SCH_RADAR_DIST: {
      uint8_t ssm = (radarData.ok && radarData.bestDist_cm > 0) ? A429_SSM_BNR_NORMAL : A429_SSM_BNR_NO_DATA;
      float dist_m = radarData.bestDist_cm / 100.0f;
      return ARINC429::buildBNR(A429_LABEL_RADAR_DIST, 0, dist_m, A429_LSB_RADAR_DIST_M, ssm);
    }
    case SCH_RADIO_ALT: {
      uint8_t ssm = (tofData.ok && tofData.valid) ? A429_SSM_BNR_NORMAL : A429_SSM_BNR_NO_DATA;
      float ft = tofData.distance_mm * 0.00328084f;  // mm -> ft
      return ARINC429::buildBNR(A429_LABEL_RADIO_ALT, 0, ft, A429_LSB_RADIO_ALT_FT, ssm);
    }
    case SCH_GPS_ALT: {
      uint8_t ssm = gpsData.fix ? A429_SSM_BNR_NORMAL : A429_SSM_BNR_NO_DATA;
      float ft = gpsData.fix ? gpsData.altitude_m * 3.28084f : 0.0f;
      return ARINC429::buildBNR(A429_LABEL_GPS_ALT, 0, ft, A429_LSB_GPS_ALT_FT, ssm);
    }
    case SCH_LATITUDE: {
      uint8_t ssm = gpsData.fix ? A429_SSM_BNR_NORMAL : A429_SSM_BNR_NO_DATA;
      // Se transmite el desplazamiento respecto a la referencia local con
      // LSB fino para tener resolucion metrica (ver SensorData.h)
      float latOff = gpsData.fix ? (float)(gpsData.lat - GPS_LAT_REF_DEG) : 0.0f;
      return ARINC429::buildBNR(A429_LABEL_LATITUDE, 0, latOff, A429_LSB_LATLON_DEG, ssm);
    }
    case SCH_LONGITUDE: {
      uint8_t ssm = gpsData.fix ? A429_SSM_BNR_NORMAL : A429_SSM_BNR_NO_DATA;
      float lonOff = gpsData.fix ? (float)(gpsData.lon - GPS_LON_REF_DEG) : 0.0f;
      return ARINC429::buildBNR(A429_LABEL_LONGITUDE, 0, lonOff, A429_LSB_LATLON_DEG, ssm);
    }
    case SCH_STATUS: {
      uint32_t bits = buildStatusDiscrete(gpsData, baro, radarData, tofData, seq);
      return ARINC429::buildDiscrete(A429_LABEL_STATUS, 0, bits, A429_SSM_BNR_NORMAL);
    }
    default:
      return A429_EMPTY_WORD;
  }
}

// Empaqueta el estado actual y lo transmite.
bool sendTelemetry(FlightData &flight, BaroData &baro, GPSData &gpsData, RadarData &radarData, ToFData &tofData) {
  ARINC429Packet pkt;
  unsigned long now = millis();

  // Slots fijos 0-2: actitud a 20 Hz (cada paquete los lleva)
  // Convertimos heading a rango [-180, +180] para que la codificacion
  // BNR signed sea simetrica.
  float hdg = flight.heading;
  if (hdg > 180.0f) hdg -= 360.0f;
  pkt.words[0] = ARINC429::buildBNR(A429_LABEL_PITCH, 0, flight.pitch, A429_LSB_ANGLE_DEG, A429_SSM_BNR_NORMAL);
  pkt.words[1] = ARINC429::buildBNR(A429_LABEL_ROLL, 0, flight.roll, A429_LSB_ANGLE_DEG, A429_SSM_BNR_NORMAL);
  pkt.words[2] = ARINC429::buildBNR(A429_LABEL_HDG_MAG, 0, hdg, A429_LSB_ANGLE_DEG, A429_SSM_BNR_NORMAL);

  // Slots multiplexados 3-7: planificador tipo "earliest deadline first".
  // Para cada parametro se calcula cuanto tarde llega respecto a su
  // periodo nominal, en cada slot se elige el mas atrasado de entre
  // los que ya tocaba enviar
  long lateness[SCH_COUNT];
  bool ready[SCH_COUNT];

  for (uint8_t i = 0; i < SCH_COUNT; i++) {
    long elapsed = (long)(now - schedLastSent[i]);
    lateness[i] = elapsed - schedPeriodMs[i];
    ready[i] = (lateness[i] >= 0);
  }

  for (uint8_t slot = 3; slot < 8; slot++) {
    int8_t bestIdx = -1;
    long bestLate = -1;
    for (uint8_t i = 0; i < SCH_COUNT; i++) {
      if (ready[i] && lateness[i] > bestLate) {
        bestLate = lateness[i];
        bestIdx = (int8_t)i;
      }
    }

    if (bestIdx < 0) {
      // SI ningun parametro tocaba todavia se rellena con palabra vacia
      // (label 0, SSM=NO_DATA) que el receptor ignora
      pkt.words[slot] = A429_EMPTY_WORD;
    } else {
      // El status word se construye con el txSeqNumber actual
      // El incremento se hace justo despues, solo cuando el slot elegido es el
      // status, asi el receptor detecta status words perdidos por el salto de
      // secuencia. El orden build-antes-de-incremento es lo que mantiene la
      // numeracion: el paquete lleva N, el siguiente status llevara N+1.
      pkt.words[slot] = buildScheduledWord((uint8_t)bestIdx, flight, baro, gpsData, radarData, tofData, txSeqNumber);
      schedLastSent[bestIdx] = now;
      ready[bestIdx] = false;
      if (bestIdx == SCH_STATUS) txSeqNumber++;
    }
  }

  bool success = radio.write(&pkt, sizeof(pkt));
  txCount++;
  if (!success) txFailCount++;
  return success;
}