/*
 * ===============================================================
 * == CÓDIGO DO ESP32 ESCRAVO (V2 - PADRÕES 3D) ==
 * ===============================================================
 * Função: Receber ÍNDICES de padrões do Mestre e reproduzir
 * os padrões 3D correspondentes em seus 8 motores.
 */

#include <esp_now.h>
#include <WiFi.h>

// ======================== CONTROLE DOS MOTORES (PADRÕES 3D) ========================
// --- Pinos e Canais PWM para os 8 motores do Escravo
int motorPins[8] = {14, 27, 26, 25, 33, 32, 4, 16};
int pwmChannels[8] = {0, 1, 2, 3, 4, 5, 6, 7};

#define LEDC_TIMER_RES 8

// --- Duty Cycle Proporcional à Frequência (igual ao seu primeiro código)
#define FREQ_MAX_HZ         150.0f
#define DUTY_MIN_FRACTION   0.30f

// --- Estrutura dos Padrões 3D
struct Pattern3D {
  float freq_hz;
  float dur_on_ms;
  float dur_off_ms;
};

// --- LISTA DE PADRÕES (fornecida pelo usuário)
const Pattern3D PATTERNS[] = {
  { 150.0f,   50.0f, 200.0f }, // Par (50; 150,0)
  { 107.9f,   91.0f, 200.0f }, // Par (91; 107,9)
  {  77.5f,  165.6f, 200.0f }, // Par (165,6; 77,5)
  {  55.7f,  301.4f, 200.0f }, // Par (301,40; 55,7)
  {  40.0f,  548.5f, 200.0f }, // Par (548,50; 40,0)
  {  40.0f,   50.0f, 200.0f }, // Par (50; 40,0)
  {  55.7f,   91.0f, 200.0f }, // Par (91; 55,7)
  { 107.9f,  301.4f, 200.0f }, // Par (301,40; 107,9)
  { 150.0f,  548.5f, 200.0f }, // Par (548,5; 150,0)
  {  77.5f,   50.0f, 200.0f }, // Par (50; 77,5)
  {  77.5f, 1000.0f, 200.0f }, // Par (1000; 77,5)
  {  40.0f,  301.4f, 200.0f }, // Par (301,4; 40,0)
  { 150.0f,  301.4f, 200.0f }  // Par (301,4; 150,0)
};
const int N_PAT = sizeof(PATTERNS) / sizeof(PATTERNS[0]);

// --- Lógica de Player Não-Bloqueante (adaptada do seu primeiro código)
static inline uint8_t fracToDuty8(float frac) {
  if (frac < 0.0f) frac = 0.0f; if (frac > 1.0f) frac = 1.0f;
  return (uint8_t)lroundf(frac * 255.0f);
}
static inline uint8_t dutyFromFreq(float freqHz) {
  float frac = freqHz / FREQ_MAX_HZ;
  if (frac < DUTY_MIN_FRACTION) frac = DUTY_MIN_FRACTION;
  if (frac > 1.0f) frac = 1.0f;
  return fracToDuty8(frac);
}

struct MotorPlayer {
  uint8_t channel, pin;
  float freqHz;
  uint32_t durOnMs, durOffMs;
  bool isOn = false;
  uint32_t nextToggleMs = 0;

  void configure(uint8_t ch, uint8_t p, const Pattern3D& pattern) {
    channel = ch; pin = p;
    freqHz = pattern.freq_hz;
    durOnMs = (uint32_t)round(pattern.dur_on_ms);
    durOffMs = (uint32_t)round(pattern.dur_off_ms);
    isOn = false; nextToggleMs = millis();
    stop();
  }

  void update() {
    uint32_t now = millis();
    if (now < nextToggleMs) return;
    isOn = !isOn;
    if (isOn) {
      uint8_t duty = dutyFromFreq(freqHz);
      ledcSetup(channel, freqHz, LEDC_TIMER_RES);
      ledcAttachPin(pin, channel);
      ledcWrite(channel, duty);
      nextToggleMs = now + durOnMs;
    } else {
      ledcWrite(channel, 0);
      nextToggleMs = now + durOffMs;
    }
  }

  void stop() {
    isOn = false;
    ledcWrite(channel, 0);
  }
};

MotorPlayer players[8]; // Um player para cada motor do escravo

// ============================= LÓGICA ESP-NOW =============================

// Estrutura para receber os índices dos padrões
typedef struct struct_message {
  int8_t pattern_indices[8]; // -1 para desligado, 0 a N_PAT-1 para um padrão
} struct_message;

struct_message incomingCommand;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingCommand, incomingData, sizeof(incomingCommand));
  
  for (int i = 0; i < 8; i++) {
    int8_t idx = incomingCommand.pattern_indices[i];
    if (idx >= 0 && idx < N_PAT) {
      players[i].configure(pwmChannels[i], motorPins[i], PATTERNS[idx]);
    } else {
      players[i].stop();
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);
  Serial.println("======================================");
  Serial.print("Endereço MAC do Escravo: ");
  Serial.println(WiFi.macAddress());
  Serial.println("======================================");

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao inicializar ESP-NOW");
    return;
  }
  
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Escravo pronto. Aguardando comandos do Mestre.");
}
 
void loop() {
  // A Lógica de vibração (ON/OFF) exige atualização constante
  for (int i = 0; i < 8; i++) {
    players[i].update();
  }
}