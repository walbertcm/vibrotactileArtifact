/**
 * ESP32 .ino — Padrões vibrotáteis 3D (freq, duração ON, ritmo OFF)
 * Estágio 1 (T1): Avaliação Individual (SIM / NAO)
 * Estágio 2 (T2): Comparação Pareada (A maior / B maior / Iguais)
 *
 * - PWM: frequência = freq_hz do padrão; duty proporcional à frequência:
 *     duty_frac = clamp(freq_hz / FREQ_MAX_HZ, DUTY_MIN_FRACTION, 1.0)
 * - Kick-start opcional no início de cada ON para vencer inércia.
 *
 * Ajuste os pinos conforme seu hardware.
 */

#include <Arduino.h>

// ======================== CONFIGURAÇÃO DE PINOS ========================
// Motores (ajuste conforme seu hardware)
#define MOTOR_SINGLE_PIN   25   // Estágio 1
#define MOTOR_A_PIN        25   // Estágio 2 - Motor A
#define MOTOR_B_PIN        32   // Estágio 2 - Motor B

// Botões (INPUT_PULLUP; nível ativo = LOW)
// Estágio 1
#define BTN1_YES_PIN       22   // SIM / "Percebe"
#define BTN1_NO_PIN        19   // NÃO / "Não percebe"
// Estágio 2
#define BTN2_A_GT_PIN      22   // A maior
#define BTN2_B_GT_PIN      18   // B maior
#define BTN2_EQ_PIN        19   // Iguais

#define BTN_ACTIVE_LEVEL   LOW

// ======================== PWM / LEDC (ESP32) ===========================
#define LEDC_TIMER_RES     8          // resolução 8 bits
#define LEDC_DUTY_OFF      0

// Canais dedicados
#define CH_SINGLE          0
#define CH_A               1
#define CH_B               2

// ======================== CONTROLES GERAIS =============================
#define DEBOUNCE_MS        300
#define BAUDRATE           115200

// Embaralhamento (use um pino analógico flutuante para semente aleatória)
#define RNG_SEED_PIN       34

// ======================== DUTY ~ FREQUÊNCIA ============================
// 150 Hz → duty 100%; 75 Hz → duty 50%; limite mínimo p/ evitar estol
#define FREQ_MAX_HZ         150.0f
#define DUTY_MIN_FRACTION   0.30f    // 30% (ajuste 0.25~0.35 conforme motor)

// Kick-start no início do ON (0 para desativar)
#define KICK_MS             80       // ms de impulso inicial
#define KICK_DUTY_FRAC      0.85f    // ~85% de duty durante o kick

// ======================== PADRÕES 3D (freq, durON, durOFF) =============
// Tuplas fornecidas pelo usuário (mantidas como enviadas)
struct Pattern3D {
  float freq_hz;     // frequência PWM (Hz)
  float dur_on_ms;   // duração do pulso ON (ms)
  float dur_off_ms;  // duração do pulso OFF (ritmo) (ms)
};

const Pattern3D PATTERNS[] = {
  {   40.0f,   50.0f,   50.0f },
  {  150.0f,   50.0f,   50.0f },
  {   40.0f, 1000.0f,   50.0f },
  {  150.0f, 1000.0f,   50.0f },
  {  55.66f,   91.0f,   91.0f },
  { 107.86f,   91.0f,   91.0f },
  {  55.66f,  548.5f,   91.0f },
  { 107.86f,  548.5f,   91.0f },
  {  77.46f,   50.0f,  165.6f },
  {   40.0f,  165.6f,  165.6f },
  {  77.46f,  165.6f,  165.6f },
  {  150.0f,  165.6f,  165.6f },
  {  77.46f,  301.4f,  165.6f },
  {  77.46f, 1000.0f,  165.6f },
  {  77.46f,  165.6f,  301.4f },
  {  77.46f,  301.4f,  301.4f },
  {  55.66f,   91.0f,  548.5f },
  { 107.86f,   91.0f,  548.5f },
  {  55.66f,  548.5f,  548.5f },
  { 107.86f,  548.5f,  548.5f },
  {   40.0f,   50.0f, 1000.0f },
  {  150.0f,   50.0f, 1000.0f },
  {   40.0f, 1000.0f, 1000.0f },
  {  150.0f, 1000.0f, 1000.0f }
};
const int N_PAT = sizeof(PATTERNS) / sizeof(PATTERNS[0]);

// ======================== UTILIDADES ===================================
static inline bool btnPressed(int pin) {
  return digitalRead(pin) == BTN_ACTIVE_LEVEL;
}

void waitRelease(int pin) {
  while (btnPressed(pin)) { delay(5); }
  delay(DEBOUNCE_MS);
}

// Converte fração [0..1] para duty (0..255), com clamp
static inline uint8_t fracToDuty8(float frac) {
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  int v = (int)lroundf(frac * 255.0f);
  if (v < 0) v = 0; if (v > 255) v = 255;
  return (uint8_t)v;
}

// Duty proporcional à frequência, com mínimo
static inline uint8_t dutyFromFreq(float freqHz) {
  float frac = freqHz / FREQ_MAX_HZ;               // ex.: 75/150 = 0.5
  if (frac < DUTY_MIN_FRACTION) frac = DUTY_MIN_FRACTION;
  if (frac > 1.0f) frac = 1.0f;
  return fracToDuty8(frac);
}

// Duty do kick-start
static inline uint8_t kickDuty() {
  return fracToDuty8(KICK_DUTY_FRAC);
}

// LEDC helpers
void ledcStartTone(uint8_t channel, uint8_t pin, float freqHz, uint8_t duty) {
  ledcDetachPin(pin);                 // reconfiguração limpa
  ledcSetup(channel, freqHz, LEDC_TIMER_RES);
  ledcAttachPin(pin, channel);
  ledcWrite(channel, duty);
}

void ledcStop(uint8_t channel, uint8_t pin) {
  ledcWrite(channel, LEDC_DUTY_OFF);
  ledcDetachPin(pin);
}

// MotorPlayer: gera pulsos ON/OFF independentes (não bloqueante)
struct MotorPlayer {
  uint8_t channel;
  uint8_t pin;
  float   freqHz;
  uint32_t durOnMs;
  uint32_t durOffMs;

  // estado
  bool isOn = false;
  bool kickPhase = false;
  uint32_t nextToggleMs = 0;
  uint32_t kickEndMs = 0;

  void configure(uint8_t ch, uint8_t gpio, float fHz, float onMs, float offMs) {
    channel = ch;
    pin = gpio;
    freqHz = fHz;
    durOnMs = (uint32_t)round(onMs);
    durOffMs = (uint32_t)round(offMs);
    isOn = false;
    kickPhase = false;
    nextToggleMs = millis(); // permite iniciar imediatamente
    ledcStop(channel, pin);  // garante OFF
  }

  void update() {
    uint32_t now = millis();
    if (now < nextToggleMs) return;

    if (!isOn) {
      // Transição para ON
      ledcDetachPin(pin);
      ledcSetup(channel, freqHz, LEDC_TIMER_RES);
      ledcAttachPin(pin, channel);

      if (KICK_MS > 0) {
        ledcWrite(channel, kickDuty());
        kickPhase = true;
        kickEndMs = now + (uint32_t)KICK_MS;
      } else {
        ledcWrite(channel, dutyFromFreq(freqHz));
        kickPhase = false;
      }

      isOn = true;
      nextToggleMs = now + durOnMs;
    } else {
      // Durante ON: se kick acabou, cai para duty proporcional
      if (kickPhase && millis() >= kickEndMs) {
        ledcWrite(channel, dutyFromFreq(freqHz));
        kickPhase = false;
      }

      // Fim do ON → vai para OFF
      if (now >= nextToggleMs) {
        ledcWrite(channel, LEDC_DUTY_OFF);
        isOn = false;
        nextToggleMs = now + durOffMs;
      }
    }
  }

  void stop() {
    ledcStop(channel, pin);
    isOn = false;
    kickPhase = false;
  }
};

// Embaralhamento Fisher–Yates
void shuffleIntArray(int *arr, int n) {
  for (int i = n - 1; i > 0; --i) {
    int j = random(i + 1); // [0..i]
    int tmp = arr[i];
    arr[i] = arr[j];
    arr[j] = tmp;
  }
}

// ======================== ESTÁGIO 1 ====================================
void runStage1_SingleEvaluation() {
  Serial.println(F("=== ESTAGIO 1 — Avaliacao Individual (SIM/NAO) ==="));

  // vetor de índices embaralhado
  int order[N_PAT];
  for (int i = 0; i < N_PAT; ++i) order[i] = i;
  shuffleIntArray(order, N_PAT);

  pinMode(BTN1_YES_PIN, INPUT_PULLUP);
  pinMode(BTN1_NO_PIN, INPUT_PULLUP);

  MotorPlayer player;
  for (int k = 0; k < N_PAT; ++k) {
    int idx = order[k];
    const auto &p = PATTERNS[idx];

    // Configura motor single com o padrão atual
    player.configure(CH_SINGLE, MOTOR_SINGLE_PIN, p.freq_hz, p.dur_on_ms, p.dur_off_ms);

    // Log de início
    Serial.print(F(">> T1 START: idx=")); Serial.print(idx);
    Serial.print(F(" ; (f,ON,OFF)=("));
    Serial.print(p.freq_hz, 1); Serial.print(F(","));
    Serial.print(p.dur_on_ms, 1); Serial.print(F(","));
    Serial.print(p.dur_off_ms, 1); Serial.print(F(") ; duty="));
    Serial.println(dutyFromFreq(p.freq_hz));

    // Espera resposta SIM ou NÃO enquanto toca o padrão
    int chosen = 0; // 1=SIM ; 2=NAO
    while (chosen == 0) {
      player.update();

      if (btnPressed(BTN1_YES_PIN)) {
        waitRelease(BTN1_YES_PIN);
        chosen = 1;
      } else if (btnPressed(BTN1_NO_PIN)) {
        waitRelease(BTN1_NO_PIN);
        chosen = 2;
      }
      delay(1);
    }

    // Para o motor e loga a resposta
    player.stop();
    Serial.print(F(">> RESPOSTA T1: "));
    Serial.print((chosen == 1) ? F("SIM") : F("NAO"));
    Serial.print(F(" ; (f,ON,OFF)=("));
    Serial.print(p.freq_hz, 1); Serial.print(F(","));
    Serial.print(p.dur_on_ms, 1); Serial.print(F(","));
    Serial.print(p.dur_off_ms, 1); Serial.println(F(")"));

    delay(250); // respiro entre padrões
  }

  Serial.println(F("=== T1 DONE ==="));
}

// ======================== ESTÁGIO 2 ====================================
void runStage2_PairedComparison() {
  Serial.println(F("=== ESTAGIO 2 — Comparacao Pareada (A/B/Iguais) ==="));

  pinMode(BTN2_A_GT_PIN, INPUT_PULLUP);
  pinMode(BTN2_B_GT_PIN, INPUT_PULLUP);
  pinMode(BTN2_EQ_PIN, INPUT_PULLUP);

  // Gera todas as combinações i<j
  const int MAX_PAIRS = (N_PAT * (N_PAT - 1)) / 2;
  static int pairA[MAX_PAIRS];
  static int pairB[MAX_PAIRS];
  int nPairs = 0;

  for (int i = 0; i < N_PAT; ++i) {
    for (int j = i + 1; j < N_PAT; ++j) {
      pairA[nPairs] = i;
      pairB[nPairs] = j;
      nPairs++;
    }
  }

  // Embaralha as combinações
  int order[MAX_PAIRS];
  for (int i = 0; i < nPairs; ++i) order[i] = i;
  shuffleIntArray(order, nPairs);

  MotorPlayer A, B;

  for (int k = 0; k < nPairs; ++k) {
    int idx = order[k];
    int ia = pairA[idx];
    int ib = pairB[idx];

    const auto &pa = PATTERNS[ia];
    const auto &pb = PATTERNS[ib];

    // Configura A e B
    A.configure(CH_A, MOTOR_A_PIN, pa.freq_hz, pa.dur_on_ms, pa.dur_off_ms);
    B.configure(CH_B, MOTOR_B_PIN, pb.freq_hz, pb.dur_on_ms, pb.dur_off_ms);

    // Log de início do par
    Serial.print(F(">> T2 START: Aidx=")); Serial.print(ia);
    Serial.print(F(" (f,ON,OFF)=("));
    Serial.print(pa.freq_hz, 1); Serial.print(F(","));
    Serial.print(pa.dur_on_ms, 1); Serial.print(F(","));
    Serial.print(pa.dur_off_ms, 1); Serial.print(F(") ; duty="));
    Serial.print(dutyFromFreq(pa.freq_hz));
    Serial.print(F(" ; Bidx=")); Serial.print(ib);
    Serial.print(F(" (f,ON,OFF)=("));
    Serial.print(pb.freq_hz, 1); Serial.print(F(","));
    Serial.print(pb.dur_on_ms, 1); Serial.print(F(","));
    Serial.print(pb.dur_off_ms, 1); Serial.print(F(") ; duty="));
    Serial.println(dutyFromFreq(pb.freq_hz));

    // Espera clique A/B/Iguais enquanto toca ambos
    int chosen = 0; // 1=A ; 2=B ; 3=IGUAIS
    while (chosen == 0) {
      A.update();
      B.update();

      if (btnPressed(BTN2_A_GT_PIN)) {
        waitRelease(BTN2_A_GT_PIN);
        chosen = 1;
      } else if (btnPressed(BTN2_B_GT_PIN)) {
        waitRelease(BTN2_B_GT_PIN);
        chosen = 2;
      } else if (btnPressed(BTN2_EQ_PIN)) {
        waitRelease(BTN2_EQ_PIN);
        chosen = 3;
      }
      delay(1);
    }

    // Para motores e loga resposta
    A.stop(); B.stop();
    Serial.print(F(">> RESPOSTA T2: "));
    if (chosen == 1) Serial.print(F("A_MAIOR"));
    else if (chosen == 2) Serial.print(F("B_MAIOR"));
    else Serial.print(F("IGUAIS"));
    Serial.print(F(" ; A("));
    Serial.print(pa.freq_hz, 1); Serial.print(F(",")); Serial.print(pa.dur_on_ms, 1); Serial.print(F(",")); Serial.print(pa.dur_off_ms, 1);
    Serial.print(F(") ; B("));
    Serial.print(pb.freq_hz, 1); Serial.print(F(",")); Serial.print(pb.dur_on_ms, 1); Serial.print(F(",")); Serial.print(pb.dur_off_ms, 1);
    Serial.println(F(")"));

    delay(250); // respiro entre pares
  }

  Serial.println(F("=== T2 DONE ==="));
}

// ======================== FLUXO PRINCIPAL ==============================
enum Stage { TEST_SINGLE_EVAL = 0, TEST_PAIR_COMPARISON = 1, ALL_TESTS_DONE = 2 };
Stage stage = TEST_SINGLE_EVAL;

void setup() {
  Serial.begin(BAUDRATE);
  delay(50);
  Serial.println();
  Serial.println(F("Sistema iniciado (padrões 3D: freq/durON/ritmoOFF; duty proporcional à frequência)."));

  // Semente pseudo-aleatória
  analogReadResolution(12);
  uint32_t seed = analogRead(RNG_SEED_PIN);
  randomSeed(seed ^ millis());

  // Pull-ups dos botões
  pinMode(BTN1_YES_PIN, INPUT_PULLUP);
  pinMode(BTN1_NO_PIN, INPUT_PULLUP);
  pinMode(BTN2_A_GT_PIN, INPUT_PULLUP);
  pinMode(BTN2_B_GT_PIN, INPUT_PULLUP);
  pinMode(BTN2_EQ_PIN, INPUT_PULLUP);

  // Motores em nível seguro
  pinMode(MOTOR_SINGLE_PIN, OUTPUT);
  pinMode(MOTOR_A_PIN, OUTPUT);
  pinMode(MOTOR_B_PIN, OUTPUT);
  digitalWrite(MOTOR_SINGLE_PIN, LOW);
  digitalWrite(MOTOR_A_PIN, LOW);
  digitalWrite(MOTOR_B_PIN, LOW);

  // Relatório inicial
  Serial.print(F("N_PADROES = ")); Serial.println(N_PAT);
  Serial.print(F("Mapeamento duty: duty = clamp(freq / "));
  Serial.print(FREQ_MAX_HZ, 1);
  Serial.print(F(", "));
  Serial.print(DUTY_MIN_FRACTION * 100.0f, 0);
  Serial.println(F("% .. 100%)"));
  if (KICK_MS > 0) {
    Serial.print(F("Kick-start: ")); Serial.print(KICK_MS); Serial.print(F(" ms @ "));
    Serial.print((int)(KICK_DUTY_FRAC*100.0f)); Serial.println(F("%)"));
  } else {
    Serial.println(F("Kick-start: desativado"));
  }
  Serial.println(F("Estagios: T1 (SIM/NAO) -> T2 (A/B/Iguais)"));
}

void loop() {
  if (stage == TEST_SINGLE_EVAL) {
    runStage1_SingleEvaluation();
    stage = TEST_PAIR_COMPARISON;
  } else if (stage == TEST_PAIR_COMPARISON) {
    runStage2_PairedComparison();
    stage = ALL_TESTS_DONE;
  } else {
    Serial.println(F("TODOS OS TESTES CONCLUIDOS."));
    while (true) { delay(1000); }
  }
}

