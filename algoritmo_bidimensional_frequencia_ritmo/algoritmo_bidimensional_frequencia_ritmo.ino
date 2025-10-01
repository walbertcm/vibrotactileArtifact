#include <Arduino.h>
#include "esp_system.h"  // esp_random()

// ========================= ESTADOS =========================
enum TestState {
  TEST_SINGLE_EVAL,       // Estágio 1: Identificação individual
  TEST_PAIR_COMPARISON,   // Estágio 2: Comparação em pares
  ALL_TESTS_DONE
};
TestState currentTestState = TEST_SINGLE_EVAL;

// =================== HARDWARE / PWM =======================
const int MOTOR_1_PIN = 27; // Teste 1 (identificação)
const int MOTOR_3_PIN = 25; // Teste 2 - A
const int MOTOR_4_PIN = 32; // Teste 2 - B

const int BUTTON_3_PIN = 23; // T1: responder / T2: A maior
const int BUTTON_4_PIN = 22; // T1: responder / T2: B maior
const int BUTTON_5_PIN = 19; // T1: responder / T2: Iguais

const int PWM_CHANNEL_1 = 0; // p/ MOTOR_1
const int PWM_CHANNEL_3 = 1; // p/ MOTOR_3 (A)
const int PWM_CHANNEL_4 = 2; // p/ MOTOR_4 (B)
const int PWM_RESOLUTION = 8;                 // 0..255
const int PWM_CARRIER_HZ = 20000;             // 20 kHz (portadora fixa)
const uint8_t PWM_DUTY_MIN = 60;              // limites de segurança (ajuste)
const uint8_t PWM_DUTY_MAX = 235;

// =================== PADRÕES (Frequência, RitmoOFF) =======================
// Pulso ON é sempre 200ms
const float PULSE_ON_MS = 200.0f;

struct VibroPattern {
  float freq_hz;   // (dataset) mapeada -> intensidade (duty)
  float ritmo_ms;  // OFF entre pulsos (ritmo)
};

const int NUM_PATTERNS = 14;
VibroPattern patterns[NUM_PATTERNS] = {
  {  40.0f, 1000.0f },
  {  55.7f,  548.5f },
  {  77.5f,  301.4f },
  { 107.9f,  165.6f },
  { 150.0f,   91.0f },
  {  40.0f,   50.0f },
  {  55.7f,   91.0f },
  {  77.5f,  165.6f },
  { 107.9f,  301.4f },
  { 150.0f,  548.5f },
  {  40.0f,  301.4f },
  { 150.0f,  301.4f },
  {  77.5f,   50.0f },
  {  77.5f, 1000.0f }
};

// =================== MAPEAMENTO FREQ -> DUTY ==============================
// Opção 1: LUT explícita para os 5 níveis mais comuns (ajuste fino por sensação)
uint8_t dutyFromFreq(float f) {
  // valores iniciais sugeridos (calibre no protótipo):
  // 40   Hz -> duty ~ 100
  // 55.7 Hz -> duty ~ 130
  // 77.5 Hz -> duty ~ 160
  // 107.9Hz -> duty ~ 195
  // 150  Hz -> duty ~ 225
  // interpolação linear entre pontos
  struct Pair { float f; uint8_t d; };
  const Pair lut[] = {
    {  40.0f, 100},
    {  55.7f, 130},
    {  77.5f, 160},
    { 107.9f, 195},
    { 150.0f, 225}
  };
  const int N = sizeof(lut)/sizeof(lut[0]);
  if (f <= lut[0].f) return lut[0].d;
  if (f >= lut[N-1].f) return lut[N-1].d;
  for (int i = 0; i < N-1; ++i) {
    if (f >= lut[i].f && f <= lut[i+1].f) {
      float t = (f - lut[i].f) / (lut[i+1].f - lut[i].f);
      float d = lut[i].d + t * (lut[i+1].d - lut[i].d);
      if (d < PWM_DUTY_MIN) d = PWM_DUTY_MIN;
      if (d > PWM_DUTY_MAX) d = PWM_DUTY_MAX;
      return (uint8_t)(d + 0.5f);
    }
  }
  return 150; // fallback
}

// =================== CONTROLE TESTE 1 (Identificação) ======================
int orderT1[NUM_PATTERNS];
int idxT1 = 0;
bool m1On = false;
unsigned long tM1 = 0;
uint8_t dutyT1 = 0;

// =================== CONTROLE TESTE 2 (Pares) ==============================
// C(14,2) = 91
const int NUM_COMB = (NUM_PATTERNS*(NUM_PATTERNS-1))/2;
struct PairIdx { int a; int b; };
PairIdx pairs[NUM_COMB];
int orderPairs[NUM_COMB];
int idxT2 = 0;

bool m3On = false, m4On = false;
unsigned long tM3 = 0, tM4 = 0;
uint8_t dutyA = 0, dutyB = 0;

// =================== BOTÕES / DEBOUNCE / TIMEOUT ==========================
unsigned long lastBtn = 0;
const unsigned long DEBOUNCE_MS = 250;
const unsigned long TRIAL_TIMEOUT_MS = 60000; // 60s
unsigned long trialStart = 0;

int readButton() {
  unsigned long now = millis();
  if (now - lastBtn < DEBOUNCE_MS) return 0;
  if (digitalRead(BUTTON_3_PIN) == LOW) { lastBtn = now; return 3; }
  if (digitalRead(BUTTON_4_PIN) == LOW) { lastBtn = now; return 4; }
  if (digitalRead(BUTTON_5_PIN) == LOW) { lastBtn = now; return 5; }
  return 0;
}

void shuffleInt(int *arr, int n) {
  for (int i = n - 1; i > 0; --i) {
    int j = random(0, i + 1);
    int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
  }
}

// =================== T1 helpers ==========================
void startPatternT1(int i) {
  VibroPattern p = patterns[ orderT1[i] ];
  dutyT1 = dutyFromFreq(p.freq_hz);
  // imprime no estilo solicitado
  Serial.println("----------------------------------------");
  Serial.print("TESTE 1: Apresentando Padrão ");
  Serial.print(i+1); Serial.print("/"); Serial.print(NUM_PATTERNS);
  Serial.print(": (Dur: "); Serial.print(p.ritmo_ms, 2);
  Serial.print("ms, Freq: "); Serial.print(p.freq_hz, 2);
  Serial.println("Hz)");

  // ciclo
  m1On = false;
  tM1 = millis();
  trialStart = tM1;
}

void applyNextT1() {
  idxT1++;
  if (idxT1 >= NUM_PATTERNS) {
    currentTestState = TEST_PAIR_COMPARISON;
    // desligar motor 1
    ledcWrite(PWM_CHANNEL_1, 0);
    Serial.println();
    Serial.println("======= INICIANDO ESTÁGIO 2: COMPARAÇÃO DE PARES =======");
    // preparar T2
    int k = 0;
    for (int i = 0; i < NUM_PATTERNS; ++i)
      for (int j = i+1; j < NUM_PATTERNS; ++j)
        pairs[k++] = { i, j };
    for (int i = 0; i < NUM_COMB; ++i) orderPairs[i] = i;
    shuffleInt(orderPairs, NUM_COMB);
    // iniciar primeiro par
    int rp = orderPairs[idxT2];
    VibroPattern A = patterns[ pairs[rp].a ];
    VibroPattern B = patterns[ pairs[rp].b ];
    dutyA = dutyFromFreq(A.freq_hz);
    dutyB = dutyFromFreq(B.freq_hz);
    Serial.println("----------------------------------------");
    Serial.print("TESTE 2: Apresentando Combinação ");
    Serial.print(idxT2+1); Serial.print("/"); Serial.println(NUM_COMB);
    m3On = m4On = false;
    tM3 = tM4 = millis();
    trialStart = tM3;
  } else {
    startPatternT1(idxT1);
  }
}

// =================== T2 helpers ==========================
void applyNextT2() {
  idxT2++;
  if (idxT2 >= NUM_COMB) {
    currentTestState = ALL_TESTS_DONE;
    ledcWrite(PWM_CHANNEL_3, 0);
    ledcWrite(PWM_CHANNEL_4, 0);
    Serial.println();
    Serial.println("--- FIM DE TODOS OS TESTES ---");
    return;
  }
  int rp = orderPairs[idxT2];
  VibroPattern A = patterns[ pairs[rp].a ];
  VibroPattern B = patterns[ pairs[rp].b ];
  dutyA = dutyFromFreq(A.freq_hz);
  dutyB = dutyFromFreq(B.freq_hz);

  Serial.println("----------------------------------------");
  Serial.print("TESTE 2: Apresentando Combinação ");
  Serial.print(idxT2+1); Serial.print("/"); Serial.println(NUM_COMB);

  m3On = m4On = false;
  tM3 = tM4 = millis();
  trialStart = tM3;
}

// =================== SETUP ================================================
void setup() {
  Serial.begin(115200);
  delay(50);

  // seed aleatória robusta
  randomSeed((uint32_t)esp_random());

  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  pinMode(BUTTON_4_PIN, INPUT_PULLUP);
  pinMode(BUTTON_5_PIN, INPUT_PULLUP);

  // Configura canais PWM com portadora fixa (20 kHz)
  ledcSetup(PWM_CHANNEL_1, PWM_CARRIER_HZ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_1_PIN, PWM_CHANNEL_1);
  ledcSetup(PWM_CHANNEL_3, PWM_CARRIER_HZ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_3_PIN, PWM_CHANNEL_3);
  ledcSetup(PWM_CHANNEL_4, PWM_CARRIER_HZ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_4_PIN, PWM_CHANNEL_4);

  Serial.println("======= INICIANDO ESTÁGIO 1: AVALIAÇÃO INDIVIDUAL =======");
  // Ordem aleatória do Teste 1
  for (int i = 0; i < NUM_PATTERNS; ++i) orderT1[i] = i;
  shuffleInt(orderT1, NUM_PATTERNS);

  startPatternT1(idxT1);
}

// =================== LOOP ================================================
void loop() {
  unsigned long now = millis();
  int btn = readButton();

  switch (currentTestState) {
    case TEST_SINGLE_EVAL: {
      VibroPattern p = patterns[ orderT1[idxT1] ];
      // vibração contínua em ciclos: ON fixo (200ms), OFF = ritmo
      if (m1On) {
        if (now - tM1 >= (unsigned long)PULSE_ON_MS) {
          ledcWrite(PWM_CHANNEL_1, 0);
          m1On = false; tM1 = now;
        }
      } else {
        if (now - tM1 >= (unsigned long)p.ritmo_ms) {
          ledcWrite(PWM_CHANNEL_1, dutyT1);
          m1On = true; tM1 = now;
        }
      }

      // timeout opcional
      if (now - trialStart >= TRIAL_TIMEOUT_MS) {
        Serial.print("T1_TIMEOUT,trial="); Serial.println(idxT1+1);
        applyNextT1();
        break;
      }

      // resposta (aceita 3, 4 ou 5 — se quiser, restrinja)
      if (btn == 3 || btn == 4 || btn == 5) {
        Serial.print(">> RESPOSTA T1: D=");
        Serial.print(PULSE_ON_MS, 2);
        Serial.print(", F=");
        Serial.print(p.freq_hz, 2);
        Serial.print(" -> Botão: ");
        Serial.println(btn);
        applyNextT1();
      }
    } break;

    case TEST_PAIR_COMPARISON: {
      int rp = orderPairs[idxT2];
      VibroPattern A = patterns[ pairs[rp].a ];
      VibroPattern B = patterns[ pairs[rp].b ];

      // Motor A (canal 3)
      if (m3On) {
        if (now - tM3 >= (unsigned long)PULSE_ON_MS) {
          ledcWrite(PWM_CHANNEL_3, 0);
          m3On = false; tM3 = now;
        }
      } else {
        if (now - tM3 >= (unsigned long)A.ritmo_ms) {
          ledcWrite(PWM_CHANNEL_3, dutyA);
          m3On = true; tM3 = now;
        }
      }

      // Motor B (canal 4)
      if (m4On) {
        if (now - tM4 >= (unsigned long)PULSE_ON_MS) {
          ledcWrite(PWM_CHANNEL_4, 0);
          m4On = false; tM4 = now;
        }
      } else {
        if (now - tM4 >= (unsigned long)B.ritmo_ms) {
          ledcWrite(PWM_CHANNEL_4, dutyB);
          m4On = true; tM4 = now;
        }
      }

      // timeout
      if (now - trialStart >= TRIAL_TIMEOUT_MS) {
        Serial.print("T2_TIMEOUT,trial="); Serial.println(idxT2+1);
        applyNextT2();
        break;
      }

      if (btn == 3 || btn == 4 || btn == 5) {
        // Formato como no exemplo (Dur, Freq) — Dur=OFF Variavel fixo
        Serial.print("A(");
        Serial.print(A.ritmo_ms, 2); Serial.print(","); Serial.print(A.freq_hz, 2);
        Serial.print(");B(");
        Serial.print(B.ritmo_ms, 2); Serial.print(","); Serial.print(B.freq_hz, 2);
        Serial.print(");"); Serial.println(btn);

        applyNextT2();
      }
    } break;

    case ALL_TESTS_DONE:
      delay(100);
      break;
  }
}

