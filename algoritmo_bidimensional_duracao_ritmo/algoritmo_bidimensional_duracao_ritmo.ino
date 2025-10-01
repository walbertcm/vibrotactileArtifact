/* =========================================================
 *  VibroScale 150 Hz – Duração × Ritmo (ON, OFF)
 *  T1: Identificação (22=SIM, 19=NAO) – vibra até resposta
 *  T2: Comparação Pareada (25=A, 26=B, 27==) – vibra continuamente até resposta
 * ---------------------------------------------------------
 *  Serial: 115200 baud
 *  CSV Header:
 *  task,trial,stimA_idx,stimB_idx,onA,offA,onB,offB,response,rt_ms
 * ========================================================= */

#include <Arduino.h>

/* ===================== PINAGEM ===================== */
// Motores (pinos PWM seguros no ESP32)
const int MOTOR_A_PIN = 25;    // Motor A
const int MOTOR_B_PIN = 32;   // Motor B

// T1 – Identificação (botões Sim/Não)
const int BTN_SIM_PIN = 22;   // SIM (percebeu)
const int BTN_NAO_PIN = 19;   // NÃO (não percebeu)

// T2 – Comparação Pareada (A maior / B maior / Iguais)
const int BTN_A_PIN  = 22;    // A maior
const int BTN_B_PIN  = 18;    // B maior
const int BTN_EQ_PIN = 19;    // Iguais

// Todos os botões usam pull-up interno; ative com GND.
const bool USE_BUTTONS_T2 = true;   // T2 só com botões

/* ==================== PARÂMETROS =================== */
const unsigned long MAX_RESPONSE_MS     = 60000; // timeout de resposta (ms)
const unsigned long GAP_BETWEEN_TRIALS  = 1000;  // intervalo entre tentativas (ms)
const unsigned long DEBOUNCE_MS         = 25;    // debounce dos botões

// PWM / Intensidade
const int PWM_DUTY    = 220;   // 0..255
const int PWM_FREQ_HZ = 150;   // alvo 150 Hz (ESP32 configura LEDC; AVR usa default)

// Semente aleatória
const int RNG_SEED_PIN = A0;

/* =========== Escala: 16 tuplas (ON_ms, OFF_ms) =========== */
struct Pattern { float on_ms; float off_ms; };
Pattern PATTERNS[] = {
  {  50.0, 1000.0 },
  {  91.0,  548.5 },
  { 165.6,  301.4 },
  { 301.4,  165.6 },
  { 548.5,   91.0 },
  {1000.0,   50.0 },
  {  50.0,   50.0 },
  {  91.0,   91.0 },
  { 165.6,  165.6 },
  { 301.4,  301.4 },
  { 548.5,  548.5 },
  {1000.0, 1000.0 },
  {  50.0,  301.4 },
  {1000.0,  301.4 },
  { 301.4,   50.0 },
  { 301.4, 1000.0 }
};
const int N_PAT = sizeof(PATTERNS)/sizeof(PATTERNS[0]);

/* ================== Utilidades gerais ================== */
void shuffleInt(int *arr, int n){
  for (int i=n-1;i>0;--i){
    int j = random(0, i+1);
    int tmp = arr[i]; arr[i]=arr[j]; arr[j]=tmp;
  }
}

/* ================== PWM (ESP32 vs AVR) ================= */
#if defined(ARDUINO_ARCH_ESP32)
  const int LEDC_CH_A = 0;
  const int LEDC_CH_B = 1;
  const int LEDC_RES  = 8; // 0..255

  void pwmSetupPin(int pin, int ch){
    ledcSetup(ch, PWM_FREQ_HZ, LEDC_RES);
    ledcAttachPin(pin, ch);
    ledcWrite(ch, 0);
  }
  void motorOnA(int duty){ ledcWrite(LEDC_CH_A, duty); }
  void motorOffA(){        ledcWrite(LEDC_CH_A, 0);    }
  void motorOnB(int duty){ ledcWrite(LEDC_CH_B, duty); }
  void motorOffB(){        ledcWrite(LEDC_CH_B, 0);    }
#else
  void pwmSetupPin(int pin){
    pinMode(pin, OUTPUT);
    analogWrite(pin, 0); // frequência de portadora padrão (≈490/980 Hz)
  }
  void motorOnA(int duty){ analogWrite(MOTOR_A_PIN, duty); }
  void motorOffA(){        analogWrite(MOTOR_A_PIN, 0);    }
  void motorOnB(int duty){ analogWrite(MOTOR_B_PIN, duty); }
  void motorOffB(){        analogWrite(MOTOR_B_PIN, 0);    }
#endif

/* ================== Botões / debounce ================== */
void setupButtons(){
  pinMode(BTN_SIM_PIN, INPUT_PULLUP);
  pinMode(BTN_NAO_PIN, INPUT_PULLUP);
  pinMode(BTN_A_PIN,   INPUT_PULLUP);
  pinMode(BTN_B_PIN,   INPUT_PULLUP);
  pinMode(BTN_EQ_PIN,  INPUT_PULLUP);
}

bool isPressed(int pin){
  if (digitalRead(pin)==LOW){
    delay(DEBOUNCE_MS);
    return (digitalRead(pin)==LOW);
  }
  return false;
}

/* ======== Execução de padrões (single e dual) =========== */
// T1 – execução contínua do padrão até haver resposta ou timeout
// Retorna "sim", "nao" ou "timeout".
String playPatternUntilResponse(const Pattern& p, unsigned long timeoutMs){
  unsigned long t0 = millis();
  while (millis()-t0 < timeoutMs){
    // ON
    motorOnA(PWM_DUTY);
    unsigned long tend = millis() + (unsigned long)(p.on_ms);
    while (millis() < tend){
      if (isPressed(BTN_SIM_PIN)) { motorOffA(); return "sim"; }
      if (isPressed(BTN_NAO_PIN)) { motorOffA(); return "nao"; }
      delay(1);
    }
    // OFF
    motorOffA();
    tend = millis() + (unsigned long)(p.off_ms);
    while (millis() < tend){
      if (isPressed(BTN_SIM_PIN)) { return "sim"; }
      if (isPressed(BTN_NAO_PIN)) { return "nao"; }
      delay(1);
    }
  }
  motorOffA();
  return "timeout";
}

// T2 – A e B vibram continuamente (pulsos ON/OFF) até algum botão ser pressionado.
// Retorna "a", "b", "=", ou "timeout".
String playPatternDualUntilResponse(const Pattern& a, const Pattern& b, unsigned long timeoutMs){
  bool onA = true, onB = true;
  unsigned long nextA = millis() + (unsigned long)(a.on_ms);
  unsigned long nextB = millis() + (unsigned long)(b.on_ms);

  motorOnA(PWM_DUTY);
  motorOnB(PWM_DUTY);

  unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs){
    unsigned long now = millis();

    // Alternância A
    if (now >= nextA){
      if (onA){ onA = false; motorOffA(); nextA = now + (unsigned long)(a.off_ms); }
      else     { onA = true;  motorOnA(PWM_DUTY); nextA = now + (unsigned long)(a.on_ms); }
    }

    // Alternância B
    if (now >= nextB){
      if (onB){ onB = false; motorOffB(); nextB = now + (unsigned long)(b.off_ms); }
      else     { onB = true;  motorOnB(PWM_DUTY); nextB = now + (unsigned long)(b.on_ms); }
    }

    // Leitura de botões (INPUT_PULLUP → ativo em LOW)
    if (digitalRead(BTN_A_PIN)  == LOW){ delay(DEBOUNCE_MS); if (digitalRead(BTN_A_PIN)==LOW){ motorOffA(); motorOffB(); return "a"; } }
    if (digitalRead(BTN_B_PIN)  == LOW){ delay(DEBOUNCE_MS); if (digitalRead(BTN_B_PIN)==LOW){ motorOffA(); motorOffB(); return "b"; } }
    if (digitalRead(BTN_EQ_PIN) == LOW){ delay(DEBOUNCE_MS); if (digitalRead(BTN_EQ_PIN)==LOW){ motorOffA(); motorOffB(); return "="; } }

    delay(1);
  }

  motorOffA(); motorOffB();
  return "timeout";
}

/* ===================== I/O helpers ===================== */
bool waitSerialLine(String &out, unsigned long timeoutMs){
  unsigned long t0 = millis();
  while (millis()-t0 < timeoutMs){
    if (Serial.available()){
      out = Serial.readStringUntil('\n');
      out.trim();
      if (out.length()>0) return true;
    }
    delay(2);
  }
  return false;
}

/* ================= Tarefa 1: Identificação =============== */
void printCSVHeader(){
  //Serial.println(F("task,trial,stimA_idx,stimB_idx,onA,offA,onB,offB,response,rt_ms"));
}

void runTask1(){
  Serial.println(F("\n# T1 Identificacao (botoes 22=SIM, 19=NAO)"));
  int order[N_PAT]; for(int i=0;i<N_PAT;++i) order[i]=i;
  shuffleInt(order, N_PAT);

  for (int t=0; t<N_PAT; ++t){
    int idx = order[t];
    const Pattern& p = PATTERNS[idx];

    Serial.print(F(">> T1 trial ")); Serial.print(t+1);
    Serial.print(F(" | idx=")); Serial.print(idx);
    Serial.print(F(" | (on,off)=(")); Serial.print(p.on_ms,1); Serial.print(";"); Serial.print(p.off_ms,1); Serial.println(")");

    unsigned long tStartResp = millis();
    String resp = playPatternUntilResponse(p, MAX_RESPONSE_MS);
    unsigned long rt = millis() - tStartResp;

    // CSV: task,trial,stimA_idx,stimB_idx,onA,offA,onB,offB,response,rt_ms
    Serial.print(F("T1,")); Serial.print(t+1); Serial.print(F(";"));
    Serial.print(idx); Serial.print(F(";")); Serial.print(F("-1,")); // stimB_idx N/A
    Serial.print(p.on_ms,1); Serial.print(F(";")); Serial.print(p.off_ms,1); Serial.print(F(";"));
    Serial.print(F("-1,")); Serial.print(F("-1,")); // onB/offB
    Serial.print(resp); Serial.print(F(";")); Serial.println(rt);

    delay(GAP_BETWEEN_TRIALS);
  }
}

/* ============ Tarefa 2: Comparação Pareada ============== */
void runTask2(){
  Serial.println(F("\n# T2 Comparacao Pareada (A vs B) – vibra ate resposta"));
  // Gera todos os pares i<j
  const int MAX_PAIRS = N_PAT*(N_PAT-1)/2;
  static int pairA[MAX_PAIRS], pairB[MAX_PAIRS];
  int c=0;
  for (int i=0;i<N_PAT;++i)
    for (int j=i+1;j<N_PAT;++j){ pairA[c]=i; pairB[c]=j; c++; }

  int order[MAX_PAIRS]; for (int i=0;i<MAX_PAIRS;++i) order[i]=i;
  shuffleInt(order, MAX_PAIRS);

  for (int t=0; t<MAX_PAIRS; ++t){
    int k = order[t];
    int ia = pairA[k], ib = pairB[k];
    const Pattern& A = PATTERNS[ia];
    const Pattern& B = PATTERNS[ib];

    Serial.print(F(">> T2 trial ")); Serial.print(t+1);
    Serial.print(F(" | A idx=")); Serial.print(ia);
    Serial.print(F(" (")); Serial.print(A.on_ms,1); Serial.print(";"); Serial.print(A.off_ms,1); Serial.print(")");
    Serial.print(F(" vs B idx=")); Serial.print(ib);
    Serial.print(F(" (")); Serial.print(B.on_ms,1); Serial.print(";"); Serial.print(B.off_ms,1); Serial.println(")");

    // Apresenta continuamente até resposta pelos botões
    unsigned long tResp0 = millis();
    String resp = playPatternDualUntilResponse(A, B, MAX_RESPONSE_MS);
    unsigned long rt = millis() - tResp0;

    // CSV
    Serial.print(F("T2,")); Serial.print(t+1); Serial.print(F(";"));
    Serial.print(ia); Serial.print(F(";")); Serial.print(ib); Serial.print(F(";"));
    Serial.print(A.on_ms,1); Serial.print(F(";")); Serial.print(A.off_ms,1); Serial.print(F(";"));
    Serial.print(B.on_ms,1); Serial.print(F(";")); Serial.print(B.off_ms,1); Serial.print(F(";"));
    Serial.print(resp); Serial.print(F(";")); Serial.println(rt);

    delay(GAP_BETWEEN_TRIALS);
  }
}

/* ===================== SETUP / LOOP ==================== */
void setup(){
  Serial.begin(115200);
  delay(300);

  // Aleatoriedade
  randomSeed(analogRead(RNG_SEED_PIN));

  // PWM setup
#if defined(ARDUINO_ARCH_ESP32)
  pwmSetupPin(MOTOR_A_PIN, 0);
  pwmSetupPin(MOTOR_B_PIN, 1);
  Serial.println(F("# ESP32 LEDC @150 Hz configurado (8 bits)."));
#else
  pwmSetupPin(MOTOR_A_PIN);
  pwmSetupPin(MOTOR_B_PIN);
  Serial.println(F("# AVR: PWM padrao (~490/980 Hz)."));
#endif

  setupButtons();

  Serial.println(F("\n=== VibroScale 150Hz: Duracao × Ritmo ==="));
  printCSVHeader();

  // Executa T1 e T2 sequencialmente
  runTask1();
  runTask2();

  Serial.println(F("\n# FIM. Reinicie para repetir."));
}

void loop(){
  // Vazio de propósito: execução única controlada no setup()
}
