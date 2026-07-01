// Flight Instruments Display - archivo principal del PFD
// Lee la telemetria por puerto serie en formato "$TEL,...*" y pinta los
// instrumentos en layout Basic-T conforme a la guia FAA AC 25-11B. El
// archivo GPSMap.pde anade la vista de mapa, accesible con la tecla M

import processing.serial.*;

Serial myPort;

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
float baroSetting = 30.13; // inHg (ajuste Kollsman)

// Input
boolean keyLeft = false, keyRight = false;

// Flags y timers de UI
boolean showHelp = false;
long lastDataTime = 0;
boolean linkActive = false;

// Escalado
float baseWidth = 1280;
float baseHeight = 800;

// Timing
float dt = 1.0 / 60.0;

void settings() {
  size(1280, 800, P2D); // Engine cambiado para mejor render
  smooth(8);
  pixelDensity(1);
}

void setup() {
  surface.setResizable(true);
  frameRate(60);

  initGPSMap();

// Apertura del puerto serie
  try {
    String portName = "COM6"; 

    myPort = new Serial(this, portName, 115200);
    myPort.bufferUntil('\n');  // dispara serialEvent() una vez por linea
    println("Escuchando telemetria en: " + portName);
    
  } catch (Exception e) {
    println(e.getMessage());
    println("Puertos disponibles:");
    for (String p : Serial.list()) {
      println("-" + p);
    }
  }
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
  targetGroundSpeed = gpsSpeed / 1.852;  // km/h -> knots

  // Heading bug
  float hdgBugRate = 45;  // deg/s cuando se mantiene la tecla
  if (keyLeft)  targetHdgBug -= hdgBugRate * dt;
  if (keyRight) targetHdgBug += hdgBugRate * dt;
  if (targetHdgBug < 0) targetHdgBug += 360;
  if (targetHdgBug >= 360) targetHdgBug -= 360;

  // Suavizado por interpolacion lineal
  // El valor 0.4 un buen balance entre respuesta y suavidad
  //  para la cadencia de 20 Hz del paquete.
  if (linkActive) {
    float smoothFactor = 0.4;
    float diffpitch = targetPitch - pitch;
    float diffroll = targetRoll - roll;
    
    if (abs(diffpitch)>0.1){  
      pitch = lerp(pitch, targetPitch, smoothFactor);
    }
    if (abs(diffroll)>0.1){
      roll = lerp(roll, targetRoll, smoothFactor);
    }
    altitude = lerp(altitude, targetAltitude, smoothFactor);
    verticalSpeed = lerp(verticalSpeed,targetVerticalSpeed,smoothFactor);
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
    if (showHelp) drawHelpOverlay();
  }

  popMatrix();
}

// Parser del paquete "$TEL,...*"
// Formato de 15 campos:
//   $TEL,pitch,roll,hdg,alt,vspeed,lat,lon,gpsAlt,sats,flags,
//        radarDist,tofDist,radarEnergy,radarAlert*
void serialEvent(Serial p) {
  try {
    String inString = p.readStringUntil('\n');

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

          targetAltitude = float(data[4]) * 3.28084;         // m -> ft
          targetVerticalSpeed = float(data[5]) * 196.8504;   // m/s -> ft/min

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
    // enlace inalambrico o por bytes perdidos en el puerto USB
    println("Paquete corrupto, ignorado: " + e.getMessage());
  }
}

// Teclado
void keyPressed() {
  if (key == 'm' || key == 'M') {
    showMap = !showMap;
  }
  // En vista de mapa, las teclas de zoom se gestionan en GPSMap.pde
  if (showMap) {
    handleMapKeys();
    return;
  }
  if (key == 'h' || key == 'H') showHelp = !showHelp;

  // Ajuste de la presion de referencia del barometro
  if (key == ']') baroSetting = constrain(baroSetting + 0.01, 28.00, 31.50);
  if (key == '[') baroSetting = constrain(baroSetting - 0.01, 28.00, 31.50);

  if (key == CODED) {
    if (keyCode == LEFT)  keyLeft = true;
    if (keyCode == RIGHT) keyRight = true;
  }
}

void keyReleased() {
  if (key == CODED) {
    if (keyCode == LEFT)  keyLeft = false;
    if (keyCode == RIGHT) keyRight = false;
  }
}

// Barra de estado arriba a la izquierda
void drawStatusBar() {
  float y = 24; 
  textAlign(LEFT, BASELINE);

  textSize(20); 
  if (linkActive) {
    fill(100, 255, 100);
    text("LIVE TELEMETRY (20Hz)", 15, y);
  } else {
    fill(255, 50, 50);
    text("NO DATA / LINK LOST", 15, y);
  }

  y += 24; 
  
  // Tamaño aumentado para los datos técnicos
  textSize(18); 
  fill(150);
  text("P:" + nf(pitch, 0, 1) + "\u00B0  R:" + nf(roll, 0, 1) + "\u00B0  H:" + nf(heading, 0, 0) + "\u00B0", 15, y);
  
  y += 20; 
  float displayedAltitude = altitude + ((baroSetting - 29.92) * 1000.0);
  text("GS:" + int(groundSpeed) + "kt  ALT:" + int(displayedAltitude) + "ft  VS:" + int(verticalSpeed) + "fpm", 15, y);
  
  y += 20;
  fill(130);
  text("BUG:" + nf(int(targetHdgBug), 3) + "\u00B0  BARO:" + nf(baroSetting, 2, 2), 15, y);
}

// Panel de ayuda arriba a la derecha
void drawHelpOverlay() {
  float ox = baseWidth - 260;
  float oy = 15;
  float lh = 16;

  float panelH = 130;
  fill(0, 160);
  noStroke();
  rectMode(CORNER);
  rect(ox - 10, oy - 5, 255, panelH, 6);

  fill(220);
  textSize(12);
  textAlign(LEFT, TOP);
  text("[ H ]        Muestra/oculta esta ayuda", ox, oy); oy += lh;
  text("[ M ]        Vista de mapa GPS", ox, oy); oy += lh;
  text("[ [ / ] ]    Baro -/+", ox, oy); oy += lh;
  text("[ \u2190 / \u2192 ]    Heading bug izq/der", ox, oy); oy += lh * 1.5;

  fill(100, 255, 100);
  text("DATA STREAM", ox, oy); oy += lh;
  fill(180);
  text("Esperando paquetes NRF24L01...", ox, oy);
}
