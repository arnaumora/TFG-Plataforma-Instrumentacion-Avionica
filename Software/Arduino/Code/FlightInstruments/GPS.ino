// GPS.ino - driver del NEO-6M (UART1) con TinyGPSPlus

#include <TinyGPSPlus.h>

TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);  // UART1

void initGPS() {
  GPS_Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  Serial.print("GPS: UART1 en RX=GPIO");
  Serial.print(GPS_RX_PIN);
  Serial.print(", TX=GPIO");
  Serial.print(GPS_TX_PIN);
  Serial.print(", Baud=");
  Serial.println(GPS_BAUD);

  // Chequeo rapido: esta llegando algo por el puerto?
  unsigned long start = millis();
  bool dataReceived = false;
  while (millis() - start < 1500) {
    if (GPS_Serial.available()) {
      dataReceived = true;
      break;
    }
  }

  if (dataReceived) {
    Serial.println("GPS: NMEA detectada -- OK");
  } else {
    Serial.println("GPS: AVISO -- aun no llega NMEA (suele necesitar cielo abierto)");
  }

}

// TinyGPSPlus necesita recibir los bytes en cuanto llegan. Esta funcion se
// llama cada iteracion del loop para que no se pierda nada en el buffer
// de la UART.
void feedGPS() {
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }
}

void readGPS(GPSData &data) {
  feedGPS();

  // "ok" = UART recibe datos (aunque no haya fix)
  data.ok = (gps.charsProcessed() > 10);

  // Posicion
  if (gps.location.isValid()) {
    data.lat = gps.location.lat();
    data.lon = gps.location.lng();
    data.fix = true;
  } else {
    data.fix = false;
  }

  // Solo se actualiza el resto de campos si hay dato valido, asi los
  // ultimos valores conocidos se mantienen si hay algun problema
  if (gps.altitude.isValid())   data.altitude_m = gps.altitude.meters();
  if (gps.speed.isValid())      data.speed_kmh  = gps.speed.kmph();
  if (gps.course.isValid())     data.course_deg = gps.course.deg();
  if (gps.satellites.isValid()) data.satellites = gps.satellites.value();
}
