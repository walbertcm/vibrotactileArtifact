// === DEFINIÇÕES DE PINOS ===
int motorPins[6] = {14, 27, 26, 25, 33, 32};
int pwmChannels[6] = {0, 1, 2, 3, 4, 5};

// === TEMPOS POR MOTOR (em ms) ===
int stepDurations[6] = {50, 100, 160, 300, 548, 1000};


// === ESCALA PWM FADE-OUT (6 valores) ===
int dutySteps[] = {255, 200, 160, 120, 80, 0};
const int numSteps = sizeof(dutySteps) / sizeof(dutySteps[0]);

// === VARIÁVEIS DE CONTROLE ===
unsigned long lastUpdate[6] = {0, 0, 0, 0, 0, 0};
int currentStep[6] = {0, 0, 0, 0, 0, 0};

#define PWM_FREQ 200
#define PWM_RESOLUTION 8  // 0–255

void setup() {
  for (int i = 0; i < 6; i++) {
    ledcSetup(pwmChannels[i], PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(motorPins[i], pwmChannels[i]);
    ledcWrite(pwmChannels[i], dutySteps[0]); // Início com intensidade máxima
    lastUpdate[i] = millis();
  }
}

void loop() {
  unsigned long now = millis();

  // Executa para cada motor
  for (int i = 0; i < 6; i++) {
    if (currentStep[i] < numSteps && now - lastUpdate[i] >= stepDurations[i]) {
      currentStep[i]++;
      lastUpdate[i] = now;

      if (currentStep[i] < numSteps) {
        ledcWrite(pwmChannels[i], dutySteps[currentStep[i]]);
      } else {
        ledcWrite(pwmChannels[i], 0); // desliga ao final
      }
    }
  }

  // Reinício automático após 2s do último motor terminar
  if (todosCompletaram() && now - lastUpdate[5] > 1000) {
    for (int i = 0; i < 6; i++) {
      currentStep[i] = 0;
      lastUpdate[i] = millis();
      ledcWrite(pwmChannels[i], dutySteps[0]);
    }
  }
}

bool todosCompletaram() {
  for (int i = 0; i < 6; i++) {
    if (currentStep[i] < numSteps) return false;
  }
  return true;
}
