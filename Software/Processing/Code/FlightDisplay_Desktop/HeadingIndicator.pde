// Indicador de rumbo con heading bug
// Cinta horizontal centrada en el heading actual, con etiquetas cardinales
// (N/E/S/W), marcas cada 5deg, 10deg y 30deg, y un bug cyan ajustable por el
// piloto con las flechas izquierda/derecha

void drawHeadingIndicator(float centerX, float centerY, float tapeWidth, float tapeHeight) {
  float halfW = tapeWidth / 2;
  float degreesVisible = 60;              // rango total visible en la cinta
  float pxPerDeg = tapeWidth / degreesVisible;

  // Fondo
  fill(40);
  stroke(80);
  strokeWeight(2);
  rectMode(CENTER);
  rect(centerX, centerY, tapeWidth, tapeHeight);

  // Heading bug
  // Diferencia circular entre bug y heading
  float bugDelta = targetHdgBug - heading;
  if (bugDelta > 180) bugDelta -= 360;
  if (bugDelta < -180) bugDelta += 360;
  float bugX = centerX + (bugDelta * pxPerDeg);

  // El bug solo se dibuja si cae dentro de la cinta visible
  if (bugX > centerX - halfW + 8 && bugX < centerX + halfW - 8) {
    fill(0, 255, 255);
    noStroke();
    float bugTop = centerY - tapeHeight / 2;

    // Forma del bug: una pequena "U" invertida
    beginShape();
    vertex(bugX - 8, bugTop + 1);
    vertex(bugX - 8, bugTop + 13);
    vertex(bugX - 4, bugTop + 13);
    vertex(bugX - 4, bugTop + 5);
    vertex(bugX + 4, bugTop + 5);
    vertex(bugX + 4, bugTop + 13);
    vertex(bugX + 8, bugTop + 13);
    vertex(bugX + 8, bugTop + 1);
    endShape(CLOSE);
  }

  // Marcas
  float minDeg = heading - (degreesVisible / 2) - 5;
  float maxDeg = heading + (degreesVisible / 2) + 5;
  int startTick = ceil(minDeg / 5.0) * 5;

  for (int tickVal = startTick; tickVal <= maxDeg; tickVal += 5) {
    float delta = tickVal - heading;
    float x = centerX + (delta * pxPerDeg);

    if (x < centerX - halfW + 2 || x > centerX + halfW - 2) continue;

    // Normaliza el valor a [0, 360)
    int displayDeg = tickVal % 360;
    if (displayDeg < 0) displayDeg += 360;

    float tickLen;
    float weight;
    boolean showText = false;
    int textSz = 16;

    // Marcas mayores cada 30deg: cardinales o numero grande
    if (displayDeg % 30 == 0) {
      tickLen = 25;
      weight = 3;
      showText = true;
      textSz = 20;
    } else if (displayDeg % 10 == 0) {
      tickLen = 15;
      weight = 2;
      showText = true;
      textSz = 16;
    } else {
      tickLen = 8;
      weight = 1;
    }

    stroke(255);
    strokeWeight(weight);
    float tickTop = centerY - tapeHeight / 2;
    line(x, tickTop, x, tickTop + tickLen);

    if (showText) {
      fill(255);
      textAlign(CENTER, TOP);
      textSize(textSz);

      // Cardinales en las cuatro direcciones principales. El resto de
      // marcas mayores se etiquetan como numero/10 segun la convencion
      // aeronautica (090 deg se muestra como "9").
      String label;
      if (displayDeg == 0)        label = "N";
      else if (displayDeg == 90)  label = "E";
      else if (displayDeg == 180) label = "S";
      else if (displayDeg == 270) label = "W";
      else label = str(displayDeg / 10);

      text(label, x, tickTop + tickLen + 3);
    }
  }

  // Puntero fijo en el centro (triangulo amarillo apuntando hacia abajo)
  fill(255, 200, 0);
  noStroke();
  triangle(centerX - 10, centerY - tapeHeight / 2 - 2,
           centerX + 10, centerY - tapeHeight / 2 - 2,
           centerX, centerY - tapeHeight / 2 + 15);

  // Readout numerico del heading actual
  float boxY = centerY + tapeHeight / 2 + 20;
  fill(0);
  stroke(255);
  strokeWeight(2);
  rectMode(CENTER);
  rect(centerX, boxY, 64, 26);

  fill(0, 255, 0);
  textAlign(CENTER, CENTER);
  textSize(18);
  float hdgDisplay = heading % 360;
  if (hdgDisplay < 0) hdgDisplay += 360;
  text(nf(int(hdgDisplay), 3) + "\u00B0", centerX, boxY - 2);

  // Valor del bug al lado
  fill(0, 255, 255);
  textAlign(LEFT, CENTER);
  textSize(12);
  text("HDG " + nf(int(targetHdgBug), 3), centerX + 40, boxY - 2);

  rectMode(CORNER);
  textAlign(LEFT);
}
