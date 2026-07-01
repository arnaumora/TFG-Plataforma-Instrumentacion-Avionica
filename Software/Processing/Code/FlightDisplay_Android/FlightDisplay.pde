// Flight Instruments Display (ANDROID UDP VERSION)
// Lee la telemetria por red UDP en formato "$TEL,...*" y pinta los
// instrumentos en layout Basic-T conforme a la guia FAA AC 25-11B. El
// archivo GPSMap_android.pde anade la vista de mapa.

import java.net.*;
import java.io.*;

// Valores que se pintan en pantalla
float pitch = 0;           // grados (nariz arriba +)
float roll = 0;            // grados (ala derecha abajo +)
float heading = 0;         // 0-360 deg, magnetico
float slip = 0;            // deflexion de la bola de slip/skid
float altitude = 0;        // ft MSL
float verticalSpeed = 0;   // ft/min
float groundSpeed = 0;     // knots (desde GPS)

// Valores objetivo (ultimo paquete recibido)
float targetPitch = 0;
float targetRoll = 0;
float targetHeading = 0;
float targetAltitude = 0;
float targetVerticalSpeed = 0;
float targetGroundSpeed = 0;
float targetRadarDist = 0;
float targetTofDist = 0;
float radarDist = 0;
float tofDist = 0;

// Referencias que ajusta el piloto
float targetHdgBug = 0;    // heading bug (deg)
float baroSetting = 29.92; // inHg (ajuste Kollsman)

// Input y UI
boolean keyLeft = false, keyRight = false;
boolean showHelp = false;

// Flags y timers de UI
long lastDataTime = 0;
boolean linkActive = false;

// Escalado
float baseWidth = 1280;
float baseHeight = 800;

// Timing
float dt = 1.0 / 60.0;

// UDP Networking
DatagramSocket socket;
byte[] buffer = new byte[1024];
int udpPort = 12345; // El puerto en el que escucha el Android

void settings() {
  fullScreen(P2D);
  smooth(8);
}

void setup() {
  orientation(LANDSCAPE);
  frameRate(60);

  initGPSMap();
  startUDP(); // Iniciar el hilo de escucha de red UDP
}

void draw() {
  background(30);

  dt = 1.0 / frameRate;
  if (dt > 0.05) dt = 0.05;

  // El enlace se considera perdido tras 2 s sin recibir paquetes
  if (millis() - lastDataTime > 2000) {
    linkActive = false;
  }

  updateGPSData();
  
  // gpsSpeed se actualiza en updateGPSData() a partir de la traza GPS
  targetGroundSpeed = gpsSpeed / 1.852; // km/h -> knots

  // Heading bug
  float hdgBugRate = 45; // deg/s cuando se mantiene pulsada la pantalla
  if (keyLeft)  targetHdgBug -= hdgBugRate * dt;
  if (keyRight) targetHdgBug += hdgBugRate * dt;
  if (targetHdgBug < 0) targetHdgBug += 360;
  if (targetHdgBug >= 360) targetHdgBug -= 360;

  // Suavizado por interpolacion lineal
  // El valor 0.4 un buen balance entre respuesta y suavidad
  //  para la cadencia de 20 Hz del paquete.
  if (linkActive) {
    float smoothFactor = 0.4;
    pitch = lerp(pitch, targetPitch, smoothFactor);
    roll = lerp(roll, targetRoll, smoothFactor);
    altitude = lerp(altitude, targetAltitude, smoothFactor);
    verticalSpeed = targetVerticalSpeed;
    radarDist = targetRadarDist;
    tofDist = targetTofDist;
    groundSpeed = lerp(groundSpeed, targetGroundSpeed, smoothFactor);

    // El heading requiere aritmetica circular para no atravesar el rango
    // largo al cruzar la discontinuidad 0/360
    float diff = targetHeading - heading;
    if (diff > 180) diff -= 360;
    if (diff < -180) diff += 360;
    if (abs(diff) > 0.3) {
      heading += diff * smoothFactor;
    }
    if (heading >= 360) heading -= 360;
    if (heading < 0) heading += 360;
  }

  // Escalado del canvas
  float scaleFactor = min((float)width / baseWidth, (float)height / baseHeight);
  pushMatrix();
  scale(scaleFactor);

  float virtualWidth = width / scaleFactor;
  float virtualHeight = height / scaleFactor;
  float centeringOffset = (virtualWidth - baseWidth) / 2;
  float centeringOffsetY = (virtualHeight - baseHeight) / 2;
  translate(centeringOffset, centeringOffsetY);

  // Render
  if (showMap) {
    drawGPSMap();
  } else {
    float cx = baseWidth / 2;
    float cy = baseHeight / 2 - 50;

    // Basic-T
    float displayedAltitude = altitude + ((baroSetting - 29.92) * 1000.0);
    drawAttitudeIndicator(cx, cy, 400);
    drawGroundSpeedIndicator(cx - 250, cy, 80, 400);
    drawAltimeter(cx + 250, cy, 80, 400, displayedAltitude);
    drawVSI(cx + 330, cy, 50, 400);
    drawHeadingIndicator(cx, 630, 550, 50);
    drawProximityIndicator(cx + 345, 630, 110, 60);

    drawStatusBar();
    drawTouchHints(); // Dibuja ayudas visuales para los botones tactiles
  }
  popMatrix();
}

// UDP NETWORK THREAD
void startUDP() {
  try {
    socket = new DatagramSocket(udpPort);
    println("Escuchando telemetria UDP en el puerto: " + udpPort);
    
    Thread udpThread = new Thread(new Runnable() {
      public void run() {
        while (true) {
          try {
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
            socket.receive(packet);
            String data = new String(packet.getData(), 0, packet.getLength());
        
            processTelemetry(data);
          } catch (Exception e) {
            e.printStackTrace();
          }
        }
      }
    });
    udpThread.start();
  } catch (Exception e) {
    println("Error abriendo puerto UDP: " + e.getMessage());
  }
}

// Parser del paquete "$TEL,...*" (Reemplaza serialEvent)
// Formato de 15 campos:
//   $TEL,pitch,roll,hdg,alt,vspeed,lat,lon,gpsAlt,sats,flags,
//        radarDist,tofDist,radarEnergy,radarAlert*
void processTelemetry(String inString) {
  try {
    if (inString != null) {
      inString = trim(inString);

      if (inString.startsWith("$TEL,")) {
        // Eliminacion del asterisco terminador
        if (inString.endsWith("*")) {
          inString = inString.substring(0, inString.length() - 1);
        }

        String[] data = split(inString, ',');

        if (data.length >= 15) {
          // Se actualizan los valores objetivo, no los de display
          targetPitch = float(data[1]);
          targetRoll = float(data[2]);
          targetHeading = float(data[3]);
          targetAltitude = float(data[4]) * 3.28084;       // m -> ft
          targetVerticalSpeed = float(data[5]) * 196.8504; // m/s -> ft/min
          
          // targetGroundSpeed se calcula en draw() a partir de gpsSpeed
          gpsLat = float(data[6]);
          gpsLon = float(data[7]);
          gpsSats = int(data[9]);
          int statusFlags = int(data[10]);
          gpsFix = (statusFlags & 0x04) != 0;
          
          targetRadarDist = float(data[11]);
          targetTofDist = float(data[12]);

          linkActive = true;
          lastDataTime = millis();
        }
      }
    }
  } catch (Exception e) {
    // Paquete corrupto: causado por posibles interferencias RF en el
    // enlace inalambrico o perdida de paquetes UDP
    println("Paquete corrupto, ignorado: " + e.getMessage());
  }
}

// TOUCH INPUT (Reemplaza el teclado físico)
void mousePressed() {
  float scaleFactor = min((float)width / baseWidth, (float)height / baseHeight);
  float virtualWidth = width / scaleFactor;
  float virtualHeight = height / scaleFactor;
  float centeringOffset = (virtualWidth - baseWidth) / 2;
  float centeringOffsetY = (virtualHeight - baseHeight) / 2;

  float vx = mouseX / scaleFactor - centeringOffset;
  float vy = mouseY / scaleFactor - centeringOffsetY;

  // Mapa Toggle (Esquina superior derecha)
  if (vx > baseWidth - 150 && vy < 100) {
    showMap = !showMap;
  }
  
  if (!showMap) {
    // Ajuste de la presion de referencia del barometro (Mitad derecha)
    if (vx > baseWidth - 150 && vy > 150 && vy < 300) baroSetting = constrain(baroSetting + 0.01, 28.00, 31.50);
    if (vx > baseWidth - 150 && vy > 350 && vy < 500) baroSetting = constrain(baroSetting - 0.01, 28.00, 31.50);

    // Heading Bug izq/der (Esquinas inferiores)
    if (vx < 200 && vy > baseHeight - 200) keyLeft = true;
    if (vx > baseWidth - 200 && vy > baseHeight - 200) keyRight = true;
  }
}

void mouseReleased() {
  keyLeft = false;
  keyRight = false;
}

// Barra de estado arriba a la izquierda
void drawStatusBar() {
  float y = 18;
  textSize(13);
  textAlign(LEFT, BASELINE);

  if (linkActive) {
    fill(100, 255, 100);
    text("LIVE TELEMETRY (UDP)", 15, y);
  } else {
    fill(255, 50, 50);
    text("NO DATA / LINK LOST", 15, y);
  }

  y += 18;
  fill(150);
  text("P:" + nf(pitch, 0, 1) + "\u00B0  R:" + nf(roll, 0, 1) + "\u00B0  H:" + nf(heading, 0, 0) + "\u00B0", 15, y);
  y += 16;
  float displayedAltitude = altitude + ((baroSetting - 29.92) * 1000.0);
  text("GS:" + int(groundSpeed) + "kt  ALT:" + int(displayedAltitude) + "ft  VS:" + int(verticalSpeed) + "fpm", 15, y);
  y += 16;
  fill(130);
  text("BUG:" + nf(int(targetHdgBug), 3) + "\u00B0  BARO:" + nf(baroSetting, 2, 2), 15, y);
}

void drawTouchHints() {
  // Dibuja sutiles cajas semitransparentes para que sepas donde tocar
  fill(255, 30); noStroke();
  rectMode(CORNER);

  // Map Toggle
  rect(baseWidth - 120, 20, 100, 60, 8);
  fill(255, 150); textAlign(CENTER, CENTER); textSize(12);
  text("MAP", baseWidth - 70, 50);

  // Baro
  fill(255, 30); rect(baseWidth - 120, 150, 100, 60, 8);
  fill(255, 150);
  text("BARO +", baseWidth - 70, 180);
  fill(255, 30); rect(baseWidth - 120, 350, 100, 60, 8);
  fill(255, 150);
  text("BARO -", baseWidth - 70, 380);

  // Bug
  fill(255, 30);
  rect(20, baseHeight - 100, 120, 80, 8);
  rect(baseWidth - 140, baseHeight - 100, 120, 80, 8);
  fill(255, 150);
  text("< BUG", 80, baseHeight - 60);
  text("BUG >", baseWidth - 80, baseHeight - 60);
  
  textAlign(LEFT, BASELINE);
}
