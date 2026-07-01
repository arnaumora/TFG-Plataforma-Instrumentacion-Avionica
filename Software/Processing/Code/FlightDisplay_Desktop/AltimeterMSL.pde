// Altimetro (altitud MSL en pies)
// Cinta vertical analoga a la del ASI con su propia escala y una ventana
// Kohlsman (ajuste barometrico) debajo, controlada por las teclas [ y ].
// El rango visible esta reducido respecto al estandar aeronautico para
// adaptar el instrumento a la operacion en banco de pruebas (que se pueda ver movimiento)

void drawAltimeter(float centerX, float centerY, float tapeWidth, float tapeHeight, float altitude) {
  float halfH = tapeHeight / 2;
  float halfW = tapeWidth / 2;
  float rangeVisible = 100;                 // ft visibles en la cinta
  float pxPerFt = tapeHeight / rangeVisible;

  // Fondo
  fill(40);
  stroke(80);
  strokeWeight(2);
  rectMode(CENTER);
  rect(centerX, centerY, tapeWidth, tapeHeight);

  // Marcas: cada 5 ft tick pequeno, cada 25 ft tick grande etiquetado
  float minAlt = altitude - (rangeVisible / 2) - 10;
  float maxAlt = altitude + (rangeVisible / 2) + 10;
  int startTick = ceil(minAlt / 5.0) * 5;

  for (int altVal = startTick; altVal <= maxAlt; altVal += 5) {
    float delta = altVal - altitude;
    float y = centerY - (delta * pxPerFt);

    if (y < centerY - halfH + 2 || y > centerY + halfH - 2) continue;

    float xLeft = centerX - halfW;
    float tickLen = (altVal % 25 == 0) ? 15 : 8;
    float weight = (altVal % 25 == 0) ? 2 : 1;
    boolean showText = (altVal % 25 == 0);

    stroke(255);
    strokeWeight(weight);
    line(xLeft, y, xLeft + tickLen, y);

    if (showText) {
      fill(255);
      textAlign(LEFT, CENTER);
      textSize(15);
      text(str(altVal), xLeft + tickLen + 4, y - 1);
    }
  }

  // Marca central (flecha apuntando a la izquierda)
  float boxW = 60;
  float boxH = 16;
  float boxX = centerX - halfW + 15;

  fill(0);
  stroke(255);
  strokeWeight(2);
  beginShape();
  vertex(boxX, centerY - boxH);
  vertex(boxX + boxW, centerY - boxH);
  vertex(boxX + boxW, centerY + boxH);
  vertex(boxX, centerY + boxH);
  vertex(boxX, centerY + 6);
  vertex(boxX - 6, centerY);    // flecha hacia la izquierda
  vertex(boxX, centerY - 6);
  endShape(CLOSE);

  fill(0, 255, 0);
  textAlign(CENTER, CENTER);
  textSize(18);
  text(int(altitude), boxX + boxW / 2, centerY - 1);

  // Etiqueta "ALT"
  fill(180);
  textAlign(CENTER, TOP);
  textSize(11);
  text("ALT", centerX, centerY - halfH + 4);

  // Ventana Kohlsman: muestra el ajuste barometrico actual en inHg.
  // Referencia estandar ISA: 29.92 inHg.
  float baroY = centerY + halfH + 16;
  fill(0);
  stroke(80);
  strokeWeight(1);
  rectMode(CENTER);
  rect(centerX, baroY, tapeWidth + 6, 22);

  fill(0, 255, 255);
  textAlign(CENTER, CENTER);
  textSize(12);
  text(nf(baroSetting, 2, 2) + " IN", centerX, baroY - 1);

  rectMode(CORNER);
  textAlign(LEFT);
}
