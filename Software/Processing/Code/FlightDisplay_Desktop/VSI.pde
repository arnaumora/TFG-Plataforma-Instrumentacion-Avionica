// Indicador de velocidad vertical (VSI)
// Fondo de escala reducido respecto al estandar aeronautico para adaptar
// el instrumento a la operacion en banco de pruebas

void drawVSI(float centerX, float centerY, float tapeWidth, float tapeHeight) {
  float halfH = tapeHeight / 2;
  float halfW = tapeWidth / 2;

  // Fondo de escala
  float maxVS = 100;
  float padding = 10; // Añade 10 píxeles de margen arriba y abajo
  float pxPerFPM = (halfH - padding) / maxVS;


  // Fondo
  fill(40);
  stroke(80);
  strokeWeight(2);
  rectMode(CENTER);
  rect(centerX, centerY, tapeWidth, tapeHeight);

  // Linea del cero
  stroke(255);
  strokeWeight(1.5);
  line(centerX - halfW, centerY, centerX + halfW, centerY);

  // Marcas cada 25 fpm, mayores cada 50
  for (int vs = -100; vs <= 100; vs += 25) {
    float y = centerY - (vs * pxPerFPM);
    float xLeft = centerX - halfW;

    boolean isMajor = (vs % 50 == 0);
    float tickLen = isMajor ? 10 : 6;
    float weight = isMajor ? 1.5 : 1;

    stroke(255);
    strokeWeight(weight);
    line(xLeft, y, xLeft + tickLen, y);

    // Solo se etiquetan las marcas mayores, excluyendo el cero
    if (isMajor && vs != 0) {
      fill(255);
      textAlign(CENTER, CENTER);
      textSize(11);
      text(str(abs(vs)), centerX + 12, y);
    }
  }

  // Barra de tendencia (rellena desde 0 hasta el valor actual)
  float clampedVS = constrain(verticalSpeed, -maxVS, maxVS);
  float vsY = centerY - (clampedVS * pxPerFPM);

  // Color de la barra en funcion de la magnitud (verde/amarillo/naranja)
  float absVS = abs(clampedVS);
  int barR, barG, barB;
  if (absVS < 50) {
    barR = 0;   barG = 200; barB = 0;
  } else if (absVS < 75) {
    barR = 180; barG = 200; barB = 0;
  } else {
    barR = 255; barG = 180; barB = 0;
  }

  float barX = centerX - halfW + 2;
  float barW = tapeWidth - 16;

  noStroke();
  fill(barR, barG, barB, 130);

  rectMode(CORNERS);
  if (clampedVS > 0) {
    rect(barX, vsY, barX + barW, centerY);
  } else if (clampedVS < 0) {
    rect(barX, centerY, barX + barW, vsY);
  }
  rectMode(CORNER);

  // Puntero (triangulo verde apuntando a la cinta desde la izquierda)
  fill(0, 255, 0);
  noStroke();
  float triX = centerX - halfW;
  triangle(triX, vsY - 4, triX, vsY + 4, triX + 8, vsY);

  // Readout numerico arriba del instrumento
  float rdY = centerY - halfH - 14;
  fill(0);
  stroke(80);
  strokeWeight(1);
  rectMode(CENTER);
  rect(centerX, rdY, tapeWidth + 2, 20);

  fill(0, 255, 0);
  textAlign(CENTER, CENTER);
  textSize(12);

  // Valor sin redondear a la decena, para ver variaciones pequenas
  int displayVS = round(verticalSpeed);
  String vsText;
  if (displayVS > 0) vsText = "+" + displayVS;
  else vsText = str(displayVS);
  text(vsText, centerX, rdY - 1);

  // Etiqueta "VS"
  fill(150);
  textAlign(CENTER, BOTTOM);
  textSize(10);
  text("VS", centerX, rdY - 12);

  rectMode(CORNER);
  textAlign(LEFT);
}
