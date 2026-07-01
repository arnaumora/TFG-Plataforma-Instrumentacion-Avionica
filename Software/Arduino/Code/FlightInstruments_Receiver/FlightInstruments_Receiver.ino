// FlightInstruments_Receiver.ino - Receptor de telemetria y puente WiFi

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <ARINC429.h>
#include "SensorData.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// SPI del NRF24L01 sobre el ESP32-C3 Super Mini
#define NRF_SCK    4
#define NRF_MISO   5
#define NRF_MOSI   6
#define CE_PIN     3
#define CSN_PIN    7

// Radio. El canal y data rate tienen que coincidir con el transmisor
#define NRF_CHANNEL    108
#define NRF_DATA_RATE  RF24_250KBPS
#define NRF_PA_LEVEL   RF24_PA_HIGH
const byte addressRX[6] = "SENSR";

// Punto de acceso WiFi del receptor, para uso en android 
// El movil con la apk se conecta a esta red y escucha el UDP broadcast
const char *ssid = "FlightData_Network";
const char *password = "12345678";
WiFiUDP udp;
const int udpPort = 12345;
IPAddress broadcastIP(192, 168, 4, 255);

#define PRINT_RATE_HZ      20
#define PRINT_INTERVAL_MS  (1000 / PRINT_RATE_HZ)

// Estado decodificado de la telemetria. Funciona como cache de
// hold-last-good: cada label refresca su campo cuando llega una palabra
struct DecodedState {
  float    pitch_deg;
  float    roll_deg;
  float    heading_deg;
  float    pres_alt_ft;
  float    vspeed_fpm;
  double   lat_deg;
  double   lon_deg;
  float    gps_alt_ft;
  float    radio_alt_ft;
  float    radar_dist_m;
  uint8_t  satellites;
  uint8_t  status_flags;     // byte de sensores OK que va en $TEL
  uint8_t  radar_status;     // byte empaquetado: zona + alerta + plataforma
  uint8_t  last_seq;
};

DecodedState st = {0};

// Bits del byte de status que va en el campo 10 del paquete $TEL.
#define STATUS_IMU_OK     0x01
#define STATUS_BARO_OK    0x02
#define STATUS_GPS_FIX    0x04
#define STATUS_RADIO_OK   0x08
#define STATUS_RADAR_OK   0x10
#define STATUS_TOF_OK     0x20

RF24 radio(CE_PIN, CSN_PIN);
ARINC429Packet rxPkt;

uint32_t rxCount = 0;
uint32_t lostCount = 0;
uint32_t statusRxCount = 0; // Se mantiene la inicialización aquí
int32_t lastSeqRx = -1;
unsigned long lastRxTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastStatsTime = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Flight Instruments Receiver & WiFi Bridge");

  // WiFi en modo AP (Access Point)
  WiFi.softAP(ssid, password);
  udp.begin(udpPort);
  Serial.print("WiFi AP OK. IP: ");
  Serial.println(WiFi.softAPIP());

  // NRF24L01: si falla aqui no hay nada que recibir, asi que se bloquea
  // el arranque
  SPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI, -1);
  if (!radio.begin(&SPI)) {
    Serial.println("Error NRF24L01");
    while (true) { delay(1000); }
  }

  radio.setPALevel(NRF_PA_LEVEL);
  radio.setDataRate(NRF_DATA_RATE);
  radio.setChannel(NRF_CHANNEL);
  radio.setPayloadSize(sizeof(ARINC429Packet));
  radio.openReadingPipe(1, addressRX);
  radio.startListening();

  Serial.println("Escuchando telemetria...");
}

// Decodifica una palabra ARINC 429 y actualiza el campo correspondiente
// de DecodedState
static void processWord(uint32_t word) {
  uint8_t label = (uint8_t)(word & 0xFF);
  uint8_t ssm   = (uint8_t)((word >> 29) & 0x03);

  // Palabras vacias (label 0) y palabras corruptas se descartan
  if (label == 0) return;
  if (!ARINC429::checkParity(word)) return;

  bool dataOk = (ssm == A429_SSM_BNR_NORMAL);
  uint8_t lblOut, ssmOut;
  bool valid;

  switch (label) {
    case A429_LABEL_PITCH:
      if (dataOk) st.pitch_deg = ARINC429::parseBNR(word, A429_LSB_ANGLE_DEG, lblOut, ssmOut, valid);
      break;
    case A429_LABEL_ROLL:
      if (dataOk) st.roll_deg = ARINC429::parseBNR(word, A429_LSB_ANGLE_DEG, lblOut, ssmOut, valid);
      break;
    case A429_LABEL_HDG_MAG:
      if (dataOk) {
        // Heading codificado como signed [-180, +180]; el display espera [0, 360]
        float h = ARINC429::parseBNR(word, A429_LSB_ANGLE_DEG, lblOut, ssmOut, valid);
        if (h < 0.0f) h += 360.0f;
        st.heading_deg = h;
      }
      break;
    case A429_LABEL_PRES_ALT:
      if (dataOk) st.pres_alt_ft = ARINC429::parseBNR(word, A429_LSB_PRES_ALT_FT, lblOut, ssmOut, valid);
      break;
    case A429_LABEL_VERT_SPEED:
      if (dataOk) st.vspeed_fpm = ARINC429::parseBNR(word, A429_LSB_VSPEED_FPM, lblOut, ssmOut, valid);
      break;
    case A429_LABEL_LATITUDE:
      if (dataOk) st.lat_deg = GPS_LAT_REF_DEG + ARINC429::parseBNR(word, A429_LSB_LATLON_DEG, lblOut, ssmOut, valid);
      break;
    case A429_LABEL_LONGITUDE:
      if (dataOk) st.lon_deg = GPS_LON_REF_DEG + ARINC429::parseBNR(word, A429_LSB_LATLON_DEG, lblOut, ssmOut, valid);
      break;
    case A429_LABEL_GPS_ALT:
      if (dataOk) st.gps_alt_ft = ARINC429::parseBNR(word, A429_LSB_GPS_ALT_FT, lblOut, ssmOut, valid);
      break;
    case A429_LABEL_RADIO_ALT:
      // Reset explicito a 0 si no hay datos nuevos
      if (dataOk) st.radio_alt_ft = ARINC429::parseBNR(word, A429_LSB_RADIO_ALT_FT, lblOut, ssmOut, valid);
      else        st.radio_alt_ft = 0;
      break;
    case A429_LABEL_RADAR_DIST:
      if (dataOk) st.radar_dist_m = ARINC429::parseBNR(word, A429_LSB_RADAR_DIST_M, lblOut, ssmOut, valid);
      else        st.radar_dist_m = 0;
      break;
    case A429_LABEL_STATUS: {
      // Discrete: 19 bits utiles con flags, contador de satelites,
      // zona del radar, alerta y numero de secuencia.
      uint32_t bits = ARINC429::parseDiscrete(word, lblOut, ssmOut, valid);
      if (!valid) break;
      
      statusRxCount++; // Incremento tras parseo válido
      
      st.satellites = (uint8_t)(bits & A429_STATUS_SATS_MASK);

      // Traduce los bits del status word ARINC al layout del byte de
      // status del $TEL. IMU y RADIO se ponen incondicionalmente
      uint8_t flags = STATUS_IMU_OK | STATUS_RADIO_OK;
      if (bits & A429_STATUS_FIX)      flags |= STATUS_GPS_FIX;
      if (bits & A429_STATUS_BARO_OK)  flags |= STATUS_BARO_OK;
      if (bits & A429_STATUS_RADAR_OK) flags |= STATUS_RADAR_OK;
      if (bits & A429_STATUS_TOF_OK)   flags |= STATUS_TOF_OK;
      st.status_flags = flags;

      // Empaqueta zona del radar (bits 0-1), alerta de target movil
      // (bit 2) y plataforma en movimiento (bit 3) en un solo byte
      // para el campo 14 del $TEL.
      uint8_t a = (uint8_t)((bits & A429_STATUS_RADAR_ZONE_MASK) >> A429_STATUS_RADAR_ZONE_SH);
      if (bits & A429_STATUS_RADAR_ALERT) a |= 0x04;
      if (bits & A429_STATUS_PLAT_MOVING) a |= 0x08;
      st.radar_status = a;

      // Deteccion de perdidas por gap en el numero de secuencia.
      uint8_t curSeq = (uint8_t)((bits >> A429_STATUS_SEQ_SH) & 0x3F);
      if (lastSeqRx >= 0) {
        uint8_t expected = (uint8_t)((lastSeqRx + 1) & 0x3F);
        if (curSeq != expected) {
          uint8_t gap = (uint8_t)((curSeq - expected) & 0x3F);
          if (gap < 32) lostCount += gap;
        }
      }
      lastSeqRx = curSeq;
      break;
    }
  }
}

void loop() {
  unsigned long now = millis();

  // Recepcion: cada paquete entra como 8 palabras ARINC y se procesan
  // todas, aunque alguna sea EMPTY_WORD
  if (radio.available()) {
    radio.read(&rxPkt, sizeof(rxPkt));
    rxCount++;
    lastRxTime = now;

    for (uint8_t i = 0; i < 8; i++) {
      processWord(rxPkt.words[i]);
    }
  }

  // Salida a 20 Hz: serializa el estado actual como sentencia CSV
  // "$TEL,..." que el display Processing parsea como entrada.
  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = now;
    if (rxCount > 0) {
      // Conversiones de unidades para el display
      float alt_m     = st.pres_alt_ft / 3.28084f;
      float vs_mps    = st.vspeed_fpm / 196.85f;
      float gps_alt_m = st.gps_alt_ft / 3.28084f;

      String telPacket = "$TEL," +
                         String(st.pitch_deg, 1) + "," +
                         String(st.roll_deg, 1) + "," +
                         String(st.heading_deg, 1) + "," +
                         String(alt_m, 1) + "," +
                         String(vs_mps, 2) + "," +
                         String(st.lat_deg, 6) + "," +
                         String(st.lon_deg, 6) + "," +
                         String((int)gps_alt_m) + "," +
                         String(st.satellites) + "," +
                         String(st.status_flags) + "," +
                         String((int)(st.radar_dist_m * 100)) + "," +
                         String((int)(st.radio_alt_ft * 304.8)) + ",0," +
                         String(st.radar_status) + "*";

      // Doble emision: serial USB para conexion local y UDP broadcast
      Serial.println(telPacket);

      udp.beginPacket(broadcastIP, udpPort);
      udp.print(telPacket);
      udp.endPacket();
    }
  }

  // Telemetria del enlace a 1 Hz
  if (now - lastStatsTime >= 1000) {
    static uint32_t prevRxCount = 0;
    static uint32_t prevLostCount = 0;

    uint32_t rxThisSec = rxCount - prevRxCount;
    uint32_t lostThisSec = lostCount - prevLostCount;
    
    // Nueva lógica de cálculo para porcentaje de pérdidas basándose en paquetes de status
    uint32_t statusTotal = statusRxCount + lostCount;
    float lossPercent = (statusTotal > 0) ? (lostCount * 100.0f / statusTotal) : 0.0f;

    Serial.print("$STATS,");
    Serial.print(rxThisSec); Serial.print(",");
    Serial.print(lostThisSec); Serial.print(",");
    Serial.print(lossPercent, 1); Serial.print(",");
    Serial.print(rxCount);
    Serial.println("*");

    prevRxCount = rxCount;
    prevLostCount = lostCount;
    lastStatsTime = now;

    // Aviso de enlace caido: 3 s sin recibir nada
    if (lastRxTime > 0 && (now - lastRxTime > 3000)) {
      Serial.println("$WARN,LINK_LOST*");
    }
  }
}