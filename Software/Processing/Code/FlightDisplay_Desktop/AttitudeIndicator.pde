// Indicador de actitud (horizonte artificial)
// El cielo y la tierra se pintan como dos rectangulos rotados segun
// los angulos pitch/roll del paquete de telemetria, sobre los que se
// superponen la escala de pitch y el arco de bank. Por encima se dibuja
// un rectangulo con un agujero circular que actua como mascara y solo
// deja visible la parte interior del instrumento

void drawAttitudeIndicator(float centerX, float centerY, float size) {
  float radius = size / 2;
  float pixelsPerDegree = size / 45;
  float pitchOffset = pitch * pixelsPerDegree;

  pushMatrix();
  translate(centerX, centerY);
  rotate(radians(-roll));

  // Cielo: gradiente de azul oscuro a azul claro en 8 franjas
  float coverage = size * 5;
  int gradSteps = 8;
  float stripH = (size * 3) / gradSteps;

  for (int i = 0; i < gradSteps; i++) {
    float t = (float) i / (gradSteps - 1);
    fill(lerpColor(color(0, 40, 100), color(0, 140, 230), t));
    noStroke();
    rect(-coverage, -size * 3 + pitchOffset + i * stripH, coverage * 2, stripH + 1);
  }

  // Tierra: marron claro a marron oscuro
  for (int i = 0; i < gradSteps; i++) {
    float t = (float) i / (gradSteps - 1);
    fill(lerpColor(color(130, 85, 40), color(45, 25, 8), t));
    noStroke();
    rect(-coverage, pitchOffset + i * stripH, coverage * 2, stripH + 1);
  }

  // Linea del horizonte
  stroke(255);
  strokeWeight(2);
  line(-size, pitchOffset, size, pitchOffset);

  // Escala de pitch: marcas cada 5deg, numeradas cada 10deg
  textAlign(CENTER, CENTER);
  textSize(14);
  int minDeg = floor((pitch - 30) / 5.0) * 5;
  int maxDeg = ceil((pitch + 30) / 5.0) * 5;

  for (int deg = minDeg; deg <= maxDeg; deg += 5) {
    if (deg == 0) continue;

    float y = pitchOffset - (deg * pixelsPerDegree);
    float lineLength = (deg % 10 == 0) ? 60 : 30;

    stroke(255);
    strokeWeight(2);
    line(-lineLength, y, lineLength, y);

    if (deg % 10 == 0) {
      fill(255);
      noStroke();
      text(abs(deg), -lineLength - 20, y);
      text(abs(deg), lineLength + 20, y);
    }
  }

  // Arco de bank angle con marcas (dentro de la matriz rotada)
  float arcRadius = radius - 20;
  stroke(255);
  strokeWeight(2);
  noFill();
  arc(0, 0, arcRadius * 2, arcRadius * 2, radians(-150), radians(-30));

  int[] majorAngles = {30, 60};
  int[] minorAngles = {10, 20, 45};

  for (int angle : majorAngles) {
    drawBankTick(0, 0, arcRadius, angle, 15);
    drawBankTick(0, 0, arcRadius, -angle, 15);
  }
  for (int angle : minorAngles) {
    drawBankTick(0, 0, arcRadius, angle, 8);
    drawBankTick(0, 0, arcRadius, -angle, 8);
  }
  drawBankTick(0, 0, arcRadius, 0, 15);

  popMatrix(); // <--- Cerramos la rotacion del horizonte

  // Triangulo de referencia (Puntero fijo al case, estilo "Sky Pointer")
  fill(255);
  noStroke();
  float refY = centerY - arcRadius;
  triangle(centerX - 8, refY + 12, centerX + 8, refY + 12, centerX, refY);

  // Bola de deslizamiento entre las dos referencias (fija horizontal al panel)
  float brickY = centerY - radius + 40;
  stroke(255);
  strokeWeight(2);
  line(centerX - 12, brickY - 6, centerX - 12, brickY + 6);
  line(centerX + 12, brickY - 6, centerX + 12, brickY + 6);

  fill(255);
  noStroke();
  rectMode(CENTER);
  rect(centerX + slip, brickY, 20, 8);
  rectMode(CORNER);

  // Simbolo del avion, fijo y centrado
  stroke(255, 200, 0);
  strokeWeight(3);
  line(centerX - 80, centerY, centerX - 30, centerY);
  line(centerX + 30, centerY, centerX + 80, centerY);
  fill(255, 200, 0);
  noStroke();
  ellipse(centerX, centerY, 10, 10);

  // Mascara circular: rectangulo grande con un agujero redondo
  // recortado. Todo lo pintado fuera del circulo queda tapado
  float maskExt = size * 5;
  fill(30);
  noStroke();
  beginShape();
  vertex(centerX - maskExt, centerY - maskExt);
  vertex(centerX + maskExt, centerY - maskExt);
  vertex(centerX + maskExt, centerY + maskExt);
  vertex(centerX - maskExt, centerY + maskExt);
  beginContour();
  // Paso de 2deg: 180 vertices, resolucion suficiente para que el
  // contorno del circulo se vea suave sin penalizar el rendimiento
  for (float a = 360; a >= 0; a -= 2) {
    float angle = radians(a);
    vertex(centerX + cos(angle) * radius, centerY + sin(angle) * radius);
  }
  endContour();
  endShape(CLOSE);

  // Bezel
  stroke(80);
  strokeWeight(4);
  noFill();
  ellipse(centerX, centerY, size, size);
}

void drawBankTick(float cx, float cy, float radius, int angle, float length) {
  float screenAngle = radians(-90 - angle);
  float x1 = cx + cos(screenAngle) * radius;
  float y1 = cy + sin(screenAngle) * radius;
  float x2 = cx + cos(screenAngle) * (radius - length);
  float y2 = cy + sin(screenAngle) * (radius - length);

  stroke(255);
  strokeWeight(2);
  line(x1, y1, x2, y2);
}
