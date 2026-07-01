// Vista de mapa GPS - tiles topograficos del ICGC descargados en disco
// No requiere internet en uso: los tiles se descargan antes y quedan guardados.
//
// Referencias:
//   ICGC Base Map Service (capa topografica).
//     Licencia: CC-BY (Cataluna) / ODbL (resto del mundo).
//     https://www.icgc.cat/en/Geoinformation-and-Maps/Base-Map-Service
//   OpenStreetMap Wiki - "Slippy map tilenames"
//   FAA HF-STD-001B sec.5.6.3.2 - guia de map displays
//   FAA AC 25-11B sec.6.2.5 - ubicacion de "other information"

import java.util.HashMap;
import java.io.File; 
import android.os.Environment;

// Datos GPS
float gpsLat = 0;
float gpsLon = 0;
float gpsSpeed = 0;
int   gpsSats = 0;
boolean gpsFix = false;

// Traza
// Los puntos son float[2] {lat, lon}. Un null en la lista marca un "hueco"
// para que drawMapTrail no una con una linea recta dos posiciones separadas en el tiempo
ArrayList<float[]> gpsTrail;
int maxTrailPoints = 2000;
long lastTrailTime = 0;
int trailIntervalMs = 500;
boolean lastFixState = false; // para detectar transicion fix->sin fix

// Estado del mapa
boolean showMap = false;
int mapZoom = 15; // zoom entero Web Mercator
final int MAP_ZOOM_MIN = 8;
final int MAP_ZOOM_MAX = 17;
boolean mapCenterLocked = true; // true = el mapa sigue al avion
float mapCenterLat;
float mapCenterLon;

// Vista por defecto cuando aun no hay fix.
// Si despues llega, el mapa se recentra solo
final float DEFAULT_MAP_LAT = 41.5638;
final float DEFAULT_MAP_LON = 2.0222;

// Cache de tiles
HashMap<String, PImage> tileCache;
final int CACHE_MAX_SIZE = 300;

// Colores (paleta para mapa topografico claro)
final color MAP_FALLBACK_BG   = color(240, 238, 232);
final color MAP_FALLBACK_GRID = color(200, 198, 190);
final color MAP_TRAIL         = color(220, 30, 30);
final color MAP_TRAIL_OLD     = color(220, 150, 150);
final color MAP_AIRCRAFT      = color(0, 110, 220);
final color MAP_AIRCRAFT_LINE = color(255);
final color MAP_WARNING       = color(220, 140, 0);
final color MAP_PANEL_BG      = color(255, 220);
final color MAP_PANEL_TEXT    = color(30);
final color MAP_STRIP_BG      = color(30);
final color MAP_STRIP_LABEL   = color(180);
final color MAP_STRIP_VALUE   = color(230);

// Llamar desde setup()
void initGPSMap() {
  gpsTrail = new ArrayList<float[]>();
  tileCache = new HashMap<String, PImage>();
  mapCenterLat = DEFAULT_MAP_LAT;
  mapCenterLon = DEFAULT_MAP_LON;
  // Los tiles que falten se cachean como null y se dibujan como celda gris
}

// Llamar desde draw() antes del render
void updateGPSData() {
  // Deteccion de la transicion fix -> sin fix. Cuando se pierde el fix
  // se inserta un null en la traza para que, al recuperar la senal,
  // drawMapTrail no una los dos extremos con una recta inventada
  if (lastFixState && !gpsFix && gpsTrail.size() > 0) {
    // Solo se inserta el marcador si el ultimo punto no es ya null
    if (gpsTrail.get(gpsTrail.size() - 1) != null) {
      gpsTrail.add(null);
    }
  }
  lastFixState = gpsFix;

  // Anade un punto a la traza cada trailIntervalMs solo si hay fix
  if (gpsFix && millis() - lastTrailTime > trailIntervalMs) {
    // Calculo de la velocidad sobre el suelo (GS) entre el ultimo punto
    // valido y la nueva posicion, mediante proyeccion: dx = dLon*cos(lat), dy = dLat.
    // Origen: aproximacion plana derivada de la formula de Haversine
    // para distancias pequenas (ver Snyder, "Map Projections", 1987).
    float[] lastPt = null;
    long lastDt = 0;
    for (int i = gpsTrail.size() - 1; i >= 0; i--) {
      if (gpsTrail.get(i) != null) {
        lastPt = gpsTrail.get(i);
        lastDt = millis() - lastTrailTime;
        break;
      }
    }

    if (lastPt != null && lastDt > 0 && lastDt < 3000) {
      float dLat = gpsLat - lastPt[0];
      float dLon = (gpsLon - lastPt[1]) * cos(radians(gpsLat));
      float dist_m = sqrt(dLat * dLat + dLon * dLon) * 111320.0;
      float speed_ms = dist_m / (lastDt / 1000.0);
      float speed_kmh = speed_ms * 3.6;

      // Filtro EMA sobre la velocidad calculada para absorber el jitter
      // de la posicion GPS (~= +/-0.3 m/s en condiciones normales).
      float alpha = 0.4;
      gpsSpeed = gpsSpeed * (1 - alpha) + speed_kmh * alpha;
    }

    gpsTrail.add(new float[]{gpsLat, gpsLon});
    if (gpsTrail.size() > maxTrailPoints) {
      gpsTrail.remove(0);
    }
    lastTrailTime = millis();
  }

  // Sin fix, el GS decae suavemente hacia cero en lugar de quedarse
  // congelado en el ultimo valor calculado
  if (!gpsFix) {
    gpsSpeed *= 0.9;
    if (gpsSpeed < 0.1) gpsSpeed = 0;
  }

  // Auto-centrado del mapa sobre el avion mientras el lock este activo
  if (mapCenterLocked && gpsFix) {
    mapCenterLat = gpsLat;
    mapCenterLon = gpsLon;
  }
}

// Proyeccion Web Mercator
float lonToPixelX(float lon, int z) {
  return (lon + 180.0) / 360.0 * 256.0 * pow(2, z);
}

float latToPixelY(float lat, int z) {
  float latRad = radians(lat);
  return (1 - log(tan(latRad) + 1.0 / cos(latRad)) / PI)
         / 2.0 * 256.0 * pow(2, z);
}

// Carga de tiles desde los assets internos del APK (carpeta data)
// Devuelve null para tiles que no existen. El null se cachea igual que un tile
// valido, asi no se llama a leer disco en bucle cada frame.
PImage getTile(int z, int x, int y) {
  String key = z + "/" + x + "/" + y;
  if (tileCache.containsKey(key)) {
    return tileCache.get(key);
  }

  String path = "tiles/" + z + "/" + x + "/" + y + ".png";
  PImage img = null;

  // Usa createInput() para comprobar si el archivo existe dentro del APK
  // sin que lance un error gordo en la consola si cae fuera del mapa
  InputStream is = createInput(path);
  if (is != null) {
    try {
      is.close(); // Solo queriamos ver si existia, asi que lo cerramos
    } catch (Exception e) { }
    
    img = loadImage(path); // Processing sabe buscar en la carpeta data automaticamente
    if (img != null && img.width == 0) img = null;
  }

  // Cache: cuando se llena, se vacia por completo.
  if (tileCache.size() > CACHE_MAX_SIZE) {
    tileCache.clear();
  }

  tileCache.put(key, img);
  return img;
}

// Dibujo del mapa (sustituye al PFD cuando showMap = true)
void drawGPSMap() {
  float mapX = 0;
  float mapY = 0;
  float mapW = baseWidth;
  float mapH = baseHeight - 60; // reservamos franja inferior para el status
  float mapCX = mapX + mapW / 2;
  float mapCY = mapY + mapH / 2;

  // Fondo, se ve por los huecos donde falten tiles
  fill(MAP_FALLBACK_BG);
  noStroke();
  rect(mapX, mapY, mapW, mapH);

  drawTiles(mapCX, mapCY, mapW, mapH);
  drawMapTrail(mapCX, mapCY);

  if (gpsFix) {
    drawAircraftSymbol(mapCX, mapCY);
  }

  drawCompassRose(mapW - 60, 60);
  drawScaleBar(mapX + 20, mapH - 20);
  drawZoomIndicator(mapX + 15, mapY + 20);
  drawAttribution(mapW, mapH);
  
  if (!gpsFix) {
    drawNoFixWarning(mapCX, mapCY);
  }

  drawMapStatusStrip(mapX, mapH, mapW);
}

// Pinta la rejilla de tiles alrededor del centro del mapa
void drawTiles(float mapCX, float mapCY, float mapW, float mapH) {
  int z = mapZoom;
  float centerPxX = lonToPixelX(mapCenterLon, z);
  float centerPxY = latToPixelY(mapCenterLat, z);

  int centerTileX = (int)(centerPxX / 256);
  int centerTileY = (int)(centerPxY / 256);

  int halfX = (int)ceil(mapW / 512.0) + 1;
  int halfY = (int)ceil(mapH / 512.0) + 1;

  int maxTile = (int)pow(2, z);

  imageMode(CORNER);
  noTint();
  for (int dy = -halfY; dy <= halfY; dy++) {
    for (int dx = -halfX; dx <= halfX; dx++) {
      int tx = centerTileX + dx;
      int ty = centerTileY + dy;

      // Coordenadas fuera del mundo Mercator -> skip
      if (tx < 0 || tx >= maxTile || ty < 0 || ty >= maxTile) continue;
      
      float screenX = mapCX + (tx * 256 - centerPxX);
      float screenY = mapCY + (ty * 256 - centerPxY);
      
      PImage tile = getTile(z, tx, ty);
      if (tile != null) {
        image(tile, screenX, screenY, 256, 256);
      } else {
        // Celda gris con borde muy tenue para que se note el grid
        fill(MAP_FALLBACK_BG);
        stroke(MAP_FALLBACK_GRID);
        strokeWeight(0.5);
        rect(screenX, screenY, 256, 256);
      }
    }
  }
}

// Traza GPS como polilinea, con fade a rojo hacia el punto mas reciente.
// Los puntos null marcan huecos (fix perdido): la linea se corta ahi
// y se retoma cuando vuelve a haber datos.
void drawMapTrail(float mapCX, float mapCY) {
  if (gpsTrail.size() < 2) return;

  int z = mapZoom;
  float centerPxX = lonToPixelX(mapCenterLon, z);
  float centerPxY = latToPixelY(mapCenterLat, z);
  int total = gpsTrail.size();

  noFill();
  strokeWeight(2.5);
  for (int i = 1; i < total; i++) {
    float[] p0 = gpsTrail.get(i - 1);
    float[] p1 = gpsTrail.get(i);

    if (p0 == null || p1 == null) continue;
    
    float x0 = mapCX + (lonToPixelX(p0[1], z) - centerPxX);
    float y0 = mapCY + (latToPixelY(p0[0], z) - centerPxY);
    float x1 = mapCX + (lonToPixelX(p1[1], z) - centerPxX);
    float y1 = mapCY + (latToPixelY(p1[0], z) - centerPxY);
    
    // age: 0 al principio de la traza, 1 al final
    float age = map(i, 0, total - 1, 0, 1);
    stroke(lerpColor(MAP_TRAIL_OLD, MAP_TRAIL, age));
    line(x0, y0, x1, y1);
  }
}

// Triangulo del avion, rotado al heading actual
void drawAircraftSymbol(float cx, float cy) {
  pushMatrix();
  translate(cx, cy);
  rotate(radians(heading));

  // Vector de "intencion" hacia adelante
  stroke(MAP_AIRCRAFT, 160);
  strokeWeight(2);
  line(0, -18, 0, -42);

  // Forma del avion
  stroke(MAP_AIRCRAFT_LINE);
  strokeWeight(2);
  fill(MAP_AIRCRAFT);
  beginShape();
  vertex(0, -14);
  vertex(-9, 10);
  vertex(0, 5);
  vertex(9, 10);
  endShape(CLOSE);

  // Punto central
  fill(MAP_AIRCRAFT_LINE);
  noStroke();
  ellipse(0, 0, 4, 4);

  popMatrix();
}

// Rosa de los vientos arriba a la derecha
void drawCompassRose(float x, float y) {
  float r = 30;

  fill(MAP_PANEL_BG);
  stroke(80);
  strokeWeight(1);
  ellipse(x, y, r * 2, r * 2);

  textSize(11);
  textAlign(CENTER, CENTER);

  String[] labels = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; i++) {
    float angle = i * HALF_PI - HALF_PI;
    float tx = x + cos(angle) * (r - 10);
    float ty = y + sin(angle) * (r - 10);
    fill(i == 0 ? color(200, 30, 30) : color(60));  // N en rojo (convencion)
    noStroke();
    text(labels[i], tx, ty);
  }

  // Aguja rotando con el heading
  pushMatrix();
  translate(x, y);
  rotate(radians(heading));
  stroke(MAP_AIRCRAFT);
  strokeWeight(2);
  line(0, 5, 0, -r + 8);
  fill(MAP_AIRCRAFT);
  noStroke();
  triangle(0, -r + 5, -4, -r + 12, 4, -r + 12);
  popMatrix();
}

// Barra de escala: dibuja la longitud equivalente en metros segun la
// resolucion real (m/px) de Web Mercator a la latitud actual.
void drawScaleBar(float x, float y) {
  // Formula estandar Mercator: mpp = 40075016.686 * cos(lat) / (256 * 2^z)
  float mpp = 40075016.686 * cos(radians(mapCenterLat))
              / (256.0 * pow(2, mapZoom));
              
  // Lista de distancias, se elige la que mejor se aproxima a ~150 px
  float[] niceDistances = {
    10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000
  };
  
  float targetPx = 150;
  float bestDist = niceDistances[0];
  float bestPx = bestDist / mpp;
  for (float d : niceDistances) {
    float px = d / mpp;
    if (abs(px - targetPx) < abs(bestPx - targetPx)) {
      bestDist = d;
      bestPx = px;
    }
  }

  float barLen = bestDist / mpp;

  // Fondo de la caja
  fill(MAP_PANEL_BG);
  noStroke();
  rect(x - 8, y - 22, barLen + 16, 32);

  // Linea con topes a los extremos
  stroke(30);
  strokeWeight(2);
  line(x, y, x + barLen, y);
  line(x, y - 5, x, y + 5);
  line(x + barLen, y - 5, x + barLen, y + 5);

  fill(MAP_PANEL_TEXT);
  noStroke();
  textSize(11);
  textAlign(CENTER, BOTTOM);
  String label;
  if (bestDist >= 1000) {
    label = nf(bestDist / 1000, 0, 1) + " km";
  } else {
    label = int(bestDist) + " m";
  }
  text(label, x + barLen / 2, y - 6);
}

// Indicador de zoom + recordatorio de teclas, arriba a la izquierda
void drawZoomIndicator(float x, float y) {
  fill(MAP_PANEL_BG);
  noStroke();
  rect(x - 5, y - 3, 235, 36);

  fill(MAP_PANEL_TEXT);
  textSize(12);
  textAlign(LEFT, TOP);
  text("ZOOM: " + mapZoom + "  (" + MAP_ZOOM_MIN + "-" + MAP_ZOOM_MAX + ")", x, y);
  text("[+/-] Zoom   [C] Centrar   [M] PFD", x, y + 16);
}

// Atribucion
void drawAttribution(float mapW, float mapH) {
  textSize(10);
  textAlign(RIGHT, BOTTOM);
  String attrib = "\u00A9 ICGC  \u00A9 OpenStreetMap contributors";
  float tw = textWidth(attrib);

  fill(MAP_PANEL_BG);
  noStroke();
  rect(mapW - tw - 16, mapH - 20, tw + 12, 16);

  fill(MAP_PANEL_TEXT);
  text(attrib, mapW - 10, mapH - 6);
}

// Overlay parpadeante cuando no hay fix
void drawNoFixWarning(float cx, float cy) {
  if ((millis() / 500) % 2 == 0) {  // parpadeo a 1 Hz
    fill(0, 150);
    noStroke();
    rectMode(CENTER);
    rect(cx, cy + 10, 320, 80);
    rectMode(CORNER);

    fill(MAP_WARNING);
    textSize(24);
    textAlign(CENTER, CENTER);
    text("NO GPS FIX", cx, cy);
    textSize(14);
    fill(255);
    text("Esperando satelites...", cx, cy + 30);
  }
}

// Franja inferior con datos compactos
void drawMapStatusStrip(float x, float y, float w) {
  float stripH = 60;
  fill(MAP_STRIP_BG);
  noStroke();
  rect(x, y, w, stripH);

  textSize(13);

  float row1Y = y + 18;
  textAlign(LEFT, CENTER);

  fill(MAP_STRIP_LABEL);
  text("LAT", x + 15, row1Y);
  fill(MAP_STRIP_VALUE);
  text(nf(abs(gpsLat), 0, 5) + "\u00B0" + (gpsLat >= 0 ? "N" : "S"), x + 45, row1Y);
  
  fill(MAP_STRIP_LABEL);
  text("LON", x + 200, row1Y);
  fill(MAP_STRIP_VALUE);
  text(nf(abs(gpsLon), 0, 5) + "\u00B0" + (gpsLon >= 0 ? "E" : "W"), x + 235, row1Y);
  
  fill(MAP_STRIP_LABEL);
  text("SATS", x + 430, row1Y);
  fill(gpsSats >= 4 ? MAP_STRIP_VALUE : MAP_WARNING); // <4 no hay solucion 3D
  text(str(gpsSats), x + 475, row1Y);
  
  // Semaforo de fix a la derecha
  fill(gpsFix ? color(50, 220, 50) : color(220, 50, 50));
  ellipse(x + w - 25, row1Y, 10, 10);
  fill(MAP_STRIP_LABEL);
  textAlign(RIGHT, CENTER);
  text(gpsFix ? "FIX" : "---", x + w - 35, row1Y);

  float row2Y = y + 42;
  textAlign(LEFT, CENTER);

  fill(MAP_STRIP_LABEL);
  text("HDG", x + 15, row2Y);
  fill(MAP_STRIP_VALUE);
  text(nf(int(heading), 3) + "\u00B0", x + 50, row2Y);

  fill(MAP_STRIP_LABEL);
  text("GS", x + 130, row2Y);
  fill(MAP_STRIP_VALUE);
  text(nf(gpsSpeed, 0, 1) + " km/h", x + 155, row2Y);

  fill(MAP_STRIP_LABEL);
  text("ALT", x + 290, row2Y);
  fill(MAP_STRIP_VALUE);
  text(int(altitude) + " ft", x + 325, row2Y);

  fill(MAP_STRIP_LABEL);
  text("TRK", x + 430, row2Y);
  fill(MAP_STRIP_VALUE);
  float track = getTrackAngle();
  text(nf(int(track), 3) + "\u00B0", x + 465, row2Y);

  // Contador de puntos de traza a la derecha
  fill(150);
  textAlign(RIGHT, CENTER);
  text(gpsTrail.size() + " pts", x + w - 15, row2Y);
}

// Angulo de track calculado entre los dos ultimos puntos de la traza.
// Util para contrastar con el heading del IMU (si hay mucha diferencia
// es que hay viento cruzado). Salta los null (huecos por perdida de fix).
float getTrackAngle() {
  // Busca los dos ultimos puntos NO-null consecutivos
  float[] p1 = null;
  float[] p0 = null;
  for (int i = gpsTrail.size() - 1; i >= 0; i--) {
    float[] pt = gpsTrail.get(i);
    if (pt == null) {
      // Si hay un hueco entre los dos ultimos, no podemos calcular track
      if (p1 != null) return heading;
      continue;
    }
    if (p1 == null) p1 = pt;
    else { p0 = pt; break; }
  }
  if (p0 == null || p1 == null) return heading;
  
  // Correccion de la longitud por latitud para que la direccion sea real
  // (a alta latitud un grado de lon es menos metros que un grado de lat)
  float dLon = (p1[1] - p0[1]) * cos(radians(p1[0]));
  float dLat = p1[0] - p0[0];

  float track = degrees(atan2(dLon, dLat));
  if (track < 0) track += 360;
  return track;
}

// Llamado desde keyPressed() en FlightDisplay_android.pde cuando showMap == true
// En la version tactil real, el zoom se suele gestionar de otra forma, pero se deja
// esta funcion por compatibilidad si se le enchufa un teclado Bluetooth al movil.
void handleMapKeys() {
  if (key == '+' || key == '=') {
    mapZoom = min(MAP_ZOOM_MAX, mapZoom + 1);
    tileCache.clear();  // cambio de zoom = otros tiles, vaciamos cache
  }
  if (key == '-' || key == '_') {
    mapZoom = max(MAP_ZOOM_MIN, mapZoom - 1);
    tileCache.clear();
  }
  if (key == 'c' || key == 'C') {
    mapCenterLocked = true;
  }
  if (key == 'x' || key == 'X') {
    gpsTrail.clear();
    println("Traza GPS borrada");
  }
}
