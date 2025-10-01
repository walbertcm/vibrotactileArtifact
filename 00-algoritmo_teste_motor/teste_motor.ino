// === Pinos e canais PWM ===
int motorPins[8] = {14, 27, 26, 25, 33, 32, 4, 16};
int pwmChannels[8] = {0, 1, 2, 3, 4, 5, 6, 7};  // canais PWM para cada motor

#define PWM_FREQ 200          // Frequência da vibração
#define PWM_RESOLUTION 8      // 8 bits: valores de 0 a 255
#define PWM_INTENSITY 255     // Intensidade máxima

void setup() {
  // Configura cada canal e associa ao pino correspondente
  for (int i = 0; i < 8; i++) {
    ledcSetup(pwmChannels[i], PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(motorPins[i], pwmChannels[i]);
    ledcWrite(pwmChannels[i], PWM_INTENSITY);  // vibração contínua
  }
}

void loop() {
  // Nada no loop: os motores permanecem ligados continuamente
}
