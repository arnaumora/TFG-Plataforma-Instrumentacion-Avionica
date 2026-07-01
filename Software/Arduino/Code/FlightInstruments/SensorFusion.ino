// SensorFusion.ino - Filtro MARG de Mahony (9 ejes).


// Estado del filtro
static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;                 // cuaternion
static float integralFBx = 0.0f, integralFBy = 0.0f, integralFBz = 0.0f; // acumulador Ki

void initFusion(FlightData &data) {
  q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
  integralFBx = 0.0f; integralFBy = 0.0f; integralFBz = 0.0f;

  data.pitch = 0; data.roll = 0; data.heading = 0;
  data.q0 = q0; data.q1 = q1; data.q2 = q2; data.q3 = q3;
  data.lastUpdate = micros();

  Serial.println("FUSION: filtro Mahony MARG inicializado");
  Serial.print("FUSION: Kp="); Serial.print(MAHONY_KP, 2);
  Serial.print("  Ki="); Serial.println(MAHONY_KI, 2);
  Serial.print("FUSION: declinacion="); Serial.print(MAG_DECLINATION_DEG, 2);
  Serial.print("  body offset="); Serial.println(HEADING_BODY_OFFSET_DEG, 2);
}

static float invSqrt(float x) {
  return 1.0f / sqrtf(x);
}

void updateFusion(RawIMU &imu, FlightData &data) {
  unsigned long now = micros();
  float dt = (now - data.lastUpdate) / 1000000.0f;
  data.lastUpdate = now;
  // Proteccion: dt negativo (overflow) o enorme (sleep) -> se salta un ciclo
  if (dt <= 0.0f || dt > 0.5f) return;

  // Gyro a rad/s
  float gx = imu.gx * 0.0174533f;
  float gy = imu.gy * 0.0174533f;
  float gz = imu.gz * 0.0174533f;

  // Remap del magnetometro al frame del MPU (accel/gyro).
  // El AK8963 tiene los ejes orientados diferente respecto al accel/gyro
  // del MPU (los dos chips estan rotados internamente). Remap segun
  // PS-MPU-9250A-01 sec.9.1, validado por la libreria de referencia
  // de Kris Winer.
  float mx =  imu.my;
  float my =  imu.mx;
  float mz = -imu.mz;

  // Normaliza el accel
  float ax = imu.ax, ay = imu.ay, az = imu.az;
  float recipNorm = invSqrt(ax * ax + ay * ay + az * az);
  bool useAccel = (recipNorm < 10.0f);  // si la norma es ~0 lo ignoramos
  if (useAccel) {
    ax *= recipNorm;
    ay *= recipNorm;
    az *= recipNorm;
  }

  // Normaliza el mag. Si la norma no es logica (desconectado o saturado),
  // se ignora al mag en este ciclo y el filtro utiliza solo gyro+accel.
  bool useMag = true;
  recipNorm = invSqrt(mx * mx + my * my + mz * mz);
  if (recipNorm > 100.0f || recipNorm < 0.001f) {
    useMag = false;
  } else {
    mx *= recipNorm;
    my *= recipNorm;
    mz *= recipNorm;
  }

  // Nucleo de Mahony

  // Productos precalculados del cuaternion
  float q0q0 = q0 * q0, q0q1 = q0 * q1, q0q2 = q0 * q2, q0q3 = q0 * q3;
  float q1q1 = q1 * q1, q1q2 = q1 * q2, q1q3 = q1 * q3;
  float q2q2 = q2 * q2, q2q3 = q2 * q3;
  float q3q3 = q3 * q3;

  float ex = 0.0f, ey = 0.0f, ez = 0.0f;  // error acumulado

  // Correccion con el magnetometro
  if (useMag) {
    // Rota el mag medido al frame Earth
    float hx = 2.0f * (mx * (0.5f - q2q2 - q3q3) + my * (q1q2 - q0q3) + mz * (q1q3 + q0q2));
    float hy = 2.0f * (mx * (q1q2 + q0q3) + my * (0.5f - q1q1 - q3q3) + mz * (q2q3 - q0q1));
    float hz = 2.0f * (mx * (q1q3 - q0q2) + my * (q2q3 + q0q1) + mz * (0.5f - q1q1 - q2q2));

    // Referencia [bx, 0, bz]: se elimina la componente Y para que el
    // heading no influya en el error, solo la inclinacion
    float bx = sqrtf(hx * hx + hy * hy);
    float bz = hz;

    // Mag esperado en body frame: w = q* (x) [0,bx,0,bz] (x) q
    float wx = 2.0f * (bx * (0.5f - q2q2 - q3q3) + bz * (q1q3 - q0q2));
    float wy = 2.0f * (bx * (q1q2 - q0q3) + bz * (q0q1 + q2q3));
    float wz = 2.0f * (bx * (q0q2 + q1q3) + bz * (0.5f - q1q1 - q2q2));

    // Error = producto vectorial del mag medido vs esperado
    ex += (my * wz - mz * wy);
    ey += (mz * wx - mx * wz);
    ez += (mx * wy - my * wx);
  }

  // Correccion con el accel
  if (useAccel) {
    // Direccion de la gravedad esperada en body frame (segun el cuaternion)
    float vx = 2.0f * (q1q3 - q0q2);
    float vy = 2.0f * (q0q1 + q2q3);
    float vz = q0q0 - q1q1 - q2q2 + q3q3;

    // Error = producto vectorial gravedad medida vs esperada
    ex += (ay * vz - az * vy);
    ey += (az * vx - ax * vz);
    ez += (ax * vy - ay * vx);
  }

  // Controlador PI
  // Ki integra el error y estima online el bias residual del gyro
  if (MAHONY_KI > 0.0f) {
    integralFBx += MAHONY_KI * ex * dt;
    integralFBy += MAHONY_KI * ey * dt;
    integralFBz += MAHONY_KI * ez * dt;
    gx += integralFBx;
    gy += integralFBy;
    gz += integralFBz;
  }

  // Termino proporcional
  gx += MAHONY_KP * ex;
  gy += MAHONY_KP * ey;
  gz += MAHONY_KP * ez;

  // Integracion del cuaternion
  // Se guarda el estado completo antes de integrar para no mezclar
  // componentes ya actualizados con los que aun no lo estan.
  float qa = q0, qb = q1, qc = q2, qd = q3;

  q0 += (-qb * gx - qc * gy - qd * gz) * (0.5f * dt);
  q1 += ( qa * gx + qc * gz - qd * gy) * (0.5f * dt);
  q2 += ( qa * gy - qb * gz + qd * gx) * (0.5f * dt);
  q3 += ( qa * gz + qb * gy - qc * gx) * (0.5f * dt);

  // Renormalizacion, evita que el cuaternion se aleje del manifold unitario
  recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
  q0 *= recipNorm; q1 *= recipNorm; q2 *= recipNorm; q3 *= recipNorm;

  // Cuaternion -> angulos de Euler (convencion ZYX, frame MPU nativo)
  float sinPitch = 2.0f * (q0 * q2 - q3 * q1);
  sinPitch = constrain(sinPitch, -1.0f, 1.0f);  // evita NaN si hay ruido numerico

  // Conversion a frame aeronautico NED (X forward, Y right, Z down).
  //   - Pitch (sobre Y): se niega
  //   - Roll  (sobre X): no cambia
  //   - Yaw   (sobre Z): se niega
  data.pitch = -(asinf(sinPitch) * 57.2958f) - PITCH_MOUNT_OFFSET_DEG;
  data.roll  = atan2f(2.0f * (q0 * q1 + q2 * q3),
                       1.0f - 2.0f * (q1 * q1 + q2 * q2)) * 57.2958f - ROLL_MOUNT_OFFSET_DEG;

  float yaw = -(atan2f(2.0f * (q0 * q3 + q1 * q2),
                       1.0f - 2.0f * (q2 * q2 + q3 * q3)) * 57.2958f);

  // Yaw [-180, +180] -> heading [0, 360], con dos correcciones:
  //   1. MAG_DECLINATION_DEG
  //   2. HEADING_BODY_OFFSET_DEG
  yaw += MAG_DECLINATION_DEG;
  yaw += HEADING_BODY_OFFSET_DEG;
  if (yaw < 0.0f) yaw += 360.0f;
  if (yaw >= 360.0f) yaw -= 360.0f;
  data.heading = yaw;

  data.q0 = q0; data.q1 = q1; data.q2 = q2; data.q3 = q3;
}


// Inicializa el cuaternion a partir de una sola muestra de accel + mag
// Sin este valor el filtro Mahony arranca con
// cuaternion identidad y necesita varios segundos para converger
void seedQuaternionFromIMU(RawIMU &imu) {
  // Pitch/roll desde el accel asumiendo gravedad pura
  float ax = imu.ax, ay = imu.ay, az = imu.az;
  float roll  = atan2f(ay, az);
  float pitch = atan2f(-ax, sqrtf(ay*ay + az*az));

  // Mismo remap del magnetometro que en updateFusion()
  float mx =  imu.my;
  float my =  imu.mx;
  float mz = -imu.mz;

  // Heading compensado por inclinacion: proyecta el vector magnetico
  // al plano horizontal antes de extraer el angulo
  float cr = cosf(roll),  sr = sinf(roll);
  float cp = cosf(pitch), sp = sinf(pitch);
  float mxh = mx * cp + my * sr * sp + mz * cr * sp;
  float myh = my * cr - mz * sr;
  float yaw = atan2f(-myh, mxh);   // signo segun la convencion NED del filtro

  // Conversion Euler ZYX -> cuaternion
  float cy = cosf(yaw * 0.5f),   sy = sinf(yaw * 0.5f);
  float cp2 = cosf(pitch * 0.5f), sp2 = sinf(pitch * 0.5f);
  float cr2 = cosf(roll * 0.5f),  sr2 = sinf(roll * 0.5f);

  q0 = cr2*cp2*cy + sr2*sp2*sy;
  q1 = sr2*cp2*cy - cr2*sp2*sy;
  q2 = cr2*sp2*cy + sr2*cp2*sy;
  q3 = cr2*cp2*sy - sr2*sp2*cy;
}
