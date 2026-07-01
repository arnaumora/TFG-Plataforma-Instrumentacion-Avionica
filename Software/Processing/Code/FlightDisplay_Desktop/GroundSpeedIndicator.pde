// Indicador de ground speed (GS, knots) desde GPS
// Cinta vertical centrada en la velocidad actual. Marcas cada 5 kt,
// numeradas cada 10

void drawGroundSpeedIndicator(float centerX, float centerY, float tapeWidth, float tapeHeight) {
  float halfH = tapeHeight / 2;
  float halfW = tapeWidth / 2;
  float rangeVisible = 80;                  // knots visibles en la cinta
  float pxPerKt = tapeHeight / rangeVisible;

  // Fondo
  fill(40);
  stroke(80);
  strokeWeight(2);
  rectMode(CENTER);
  rect(centerX, centerY, tapeWidth, tapeHeight);

  // Marcas
  float minSpd = groundSpeed - (rangeVisible / 2) - 10;
  float maxSpd = groundSpeed + (rangeVisible / 2) + 10;
  int startTick = ceil(minSpd / 5.0) * 5;

  for (int spdVal = startTick; spdVal <= maxSpd; spdVal += 5) {
    if (spdVal < 0) continue;   // no se dibujan velocidades negativas

    float delta = spdVal - groundSpeed;
    float y = centerY - (delta * pxPerKt);

    if (y < centerY - halfH + 2 || y > centerY + halfH - 2) continue;

    float xRight = centerX + halfW;
    boolean isMajor = (spdVal % 10 == 0);
    float tickLen = isMajor ? 12 : 6;
    float weight = isMajor ? 2 : 1;

    stroke(255);
    strokeWeight(weight);
    line(xRight, y, xRight - tickLen, y);

    if (isMajor && spdVal > 0) {
      fill(255);
      textAlign(RIGHT, CENTER);
      textSize(15);
      text(str(spdVal), xRight - tickLen - 4, y - 1);
    }
  }

  // Readout central 
  // El triangulo en el lado derecho de la caja indica el valor actual
  float boxW = 55;
  float boxH = 16;
  float boxX = centerX + halfW - 5;

  fill(0);
  stroke(255);
  strokeWeight(2);
  beginShape();
  vertex(boxX - boxW, centerY - boxH);
  vertex(boxX, centerY - boxH);
  vertex(boxX, centerY - 6);
  vertex(boxX + 6, centerY);
  vertex(boxX, centerY + 6);
  vertex(boxX, centerY + boxH);
  vertex(boxX - boxW, centerY + boxH);
  endShape(CLOSE);

  fill(0, 255, 0);
  textAlign(CENTER, CENTER);
  textSize(18);
  text(int(groundSpeed), boxX - boxW / 2, centerY - 1);

  // Etiqueta arriba ("GS") y abajo ("KTS")
  fill(180);
  textAlign(CENTER, TOP);
  textSize(11);
  text("GS", centerX, centerY - halfH + 4);

  fill(180);
  textAlign(CENTER, BOTTOM);
  textSize(11);
  text("KTS", centerX, centerY + halfH - 4);

  rectMode(CORNER);
  textAlign(LEFT);
}
