// Indicador de proximidad al suelo
// Muestra la distancia mas cercana medida por los sensores y un codigo
// de color por umbrales. La medida del radar HLK-LD2410C se recibe pero
// queda fuera de la decision por falta de calidad en banco, el ToF
// VL53L1X es la unica fuente

void drawProximityIndicator(float centerX, float centerY, float boxWidth, float boxHeight) {
  // Conversion a cm para tratar ambas medidas en la misma unidad
  float tofDistCm = tofDist / 10.0;
  float radarDistCm = radarDist;

  // Seleccion de la distancia activa
  // El radar queda excluido: sus medidas son poco fiables actualmente
  // La variable se conserva por si en un futuro se reactiva su contribucion
  float currentDist = Float.MAX_VALUE;
  String activeSensor = "NONE";

  if (tofDistCm > 0 && tofDistCm < currentDist) {
    currentDist = tofDistCm;
    activeSensor = "LIDAR";
  }
  // Rama del radar deshabilitada:
  // if (radarDistCm > 0 && radarDistCm < currentDist) {
  //   currentDist = radarDistCm;
  //   activeSensor = "RADAR";
  // }

  // Caja de fondo con esquinas redondeadas
  fill(40);
  stroke(80);
  strokeWeight(2);
  rectMode(CENTER);
  rect(centerX, centerY, boxWidth, boxHeight, 8);

  // Titulo
  fill(180);
  textAlign(CENTER, TOP);
  textSize(12);
  text("PROXIMITY", centerX, centerY - boxHeight / 2 + 4);

  // Estados de salida: sin datos, fuera de rango o medida activa
  float safeDistanceThreshold = 500;

  if (currentDist == Float.MAX_VALUE) {
    // No hay lectura valida
    fill(130);
    textAlign(CENTER, CENTER);
    textSize(18);
    text("NO DATA", centerX, centerY);

  } else if (currentDist > safeDistanceThreshold) {
    // Distancia por encima del umbral de seguridad
    fill(100, 255, 100);
    textAlign(CENTER, CENTER);
    textSize(22);
    text("SAFE", centerX, centerY);

    fill(130);
    textSize(10);
    text("OUT OF RANGE", centerX, centerY + 18);

  } else {
    // Medida valida en zona de aterrizaje: color segun umbral
    color alertColor;
    if (currentDist <= 50) {
      alertColor = color(255, 50, 50);   // rojo: < 50 cm
    } else if (currentDist <= 150) {
      alertColor = color(255, 200, 0);   // amarillo: 50 - 150 cm
    } else {
      alertColor = color(50, 255, 50);   // verde: > 150 cm
    }

    fill(alertColor);
    textAlign(CENTER, CENTER);
    textSize(26);
    text(int(currentDist) + " cm", centerX, centerY - 2);

    // Etiqueta del sensor activo
    fill(180);
    textSize(11);
    text(activeSensor, centerX, centerY + 20);
  }

  // Restaurar modos de dibujo por defecto
  rectMode(CORNER);
  textAlign(LEFT);
}
