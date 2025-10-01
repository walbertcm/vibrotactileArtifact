/*
 * ===============================================================
 * == CÓDIGO DO ESP32 MESTRE (V4 - MÚLTIPLOS DISTRATORES) ==
 * ===============================================================
 * Função: Gerenciar tarefa de identificação usando padrões 3D.
 * - Sorteia 1 padrão de referência e MÚLTIPLOS padrões distratores.
 * - Envia os ÍNDICES para o Escravo via ESP-NOW.
 * - Reproduz os padrões 3D localmente usando players não-bloqueantes.
 */

#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <numeric>
#include <algorithm>

// ======================= CONFIGURAÇÕES DO EXPERIMENTO =======================
const int NUM_ENSAIOS = 10;
const int NUM_DISTRATORES_DIFERENTES = 3; // Quantos padrões distratores únicos por ensaio
const int TEMPO_ESTIMULO_MS = 50000;
const int TEMPO_ESPERA_ENTRE_ENSAIOS_MS = 10000;
const int BOTAO_PARADA_PIN = 15;

// ======================= CONFIGURAÇÕES ESP-NOW =======================
uint8_t slaveAddress[] = {0xB4, 0xE6, 0x2D, 0x85, 0xE8, 0xE1};

typedef struct struct_message {
  int8_t pattern_indices[8];
} struct_message;

struct_message commandToSlave;
esp_now_peer_info_t peerInfo;
int ensaioAtual = 0;

// ======================== CONTROLE DOS MOTORES (PADRÕES 3D) ========================
int motorPinsMaster[8] = {14, 27, 26, 25, 33, 32, 4, 16};
int pwmChannelsMaster[8] = {0, 1, 2, 3, 4, 5, 6, 7};
#define LEDC_TIMER_RES 8

#define FREQ_MAX_HZ       150.0f
#define DUTY_MIN_FRACTION   0.30f

struct Pattern3D {
  float freq_hz;
  float dur_on_ms;
  float dur_off_ms;
};

// --- LISTA DE PADRÕES (ATUALIZADA CONFORME SOLICITADO)
// Formato: { freq_hz, dur_on_ms, dur_off_ms }
// dur_off_ms é fixo em 200.0f para todos os padrões.

// --- LISTA DE PADRÕES (ATUALIZADA CONFORME SOLICITADO)
// Formato: { freq_hz, dur_on_ms, dur_off_ms }
// dur_on_ms é fixo em 200.0f para todos os padrões.

const Pattern3D PATTERNS[] = {
  { 150.0f,   50.0f, 1000.0f }, // Par (50; 1000)
  { 150.0f,   91.0f,  548.5f }, // Par (91; 548,5)
  { 150.0f,  165.6f,  301.4f }, // Par (165,6; 301,4)
  { 150.0f,  301.4f,  165.6f }, // Par (301,4; 165,6)
  { 150.0f,  548.5f,   91.0f }, // Par (548,5; 91)
  { 150.0f, 1000.0f,   50.0f }, // Par (1000; 50)
  { 150.0f,   50.0f,   50.0f }, // Par (50; 50)
  { 150.0f,   91.0f,   91.0f }, // Par (91; 91)
  { 150.0f,  165.6f,  165.6f }, // Par (165,6; 165,6)
  { 150.0f,  301.4f,  301.4f }, // Par (301,4; 301,4)
  { 150.0f,  548.5f,  548.5f }, // Par (548,5; 548,5)
  { 150.0f, 1000.0f, 1000.0f }, // Par (1000; 1000)
  { 150.0f,   50.0f,  301.4f }, // Par (50; 301,4)
  { 150.0f, 1000.0f,  301.4f }, // Par (1000; 301,4)
  { 150.0f,  301.4f,   50.0f }, // Par (301,4; 50)
  { 150.0f,  301.4f, 1000.0f }  // Par (301,4; 1000)
};


// Esta linha calcula o número de padrões automaticamente e não precisa de alteração[cite: 8].
const int N_PAT = sizeof(PATTERNS) / sizeof(PATTERNS[0]);

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

MotorPlayer playersMaster[8];

// =========================== LÓGICA DO EXPERIMENTO ===========================
void printPatternDetails(int index) {
  if (index >= 0 && index < N_PAT) {
    Serial.print("    -> { Freq: ");
    Serial.print(PATTERNS[index].freq_hz);
    Serial.print(" Hz, On: ");
    Serial.print(PATTERNS[index].dur_on_ms);
    Serial.print(" ms, Off: ");
    Serial.print(PATTERNS[index].dur_off_ms);
    Serial.println(" ms }");
  }
}

void shuffleVector(std::vector<int>& vec) {
    for (size_t i = vec.size() - 1; i > 0; --i) {
        int j = random(i + 1);
        std::swap(vec[i], vec[j]);
    }
}

void executarEnsaio() {
  ensaioAtual++;
  Serial.println("------------------------------------------");
  Serial.print("Iniciando Ensaio: ");
  Serial.print(ensaioAtual);
  Serial.print("/");
  Serial.println(NUM_ENSAIOS);

  // 1. SELECIONAR PADRÕES (REFERÊNCIA E MÚLTIPLOS DISTRATORES)
  std::vector<int> indicesDisponiveis(N_PAT);
  std::iota(indicesDisponiveis.begin(), indicesDisponiveis.end(), 0);
  shuffleVector(indicesDisponiveis);

  int idxRef = indicesDisponiveis[0];
  std::vector<int> indicesDistrator;
  for (int i = 0; i < NUM_DISTRATORES_DIFERENTES; ++i) {
    indicesDistrator.push_back(indicesDisponiveis[i + 1]);
  }

  Serial.print("  - Padrão Referência (Índice): "); Serial.println(idxRef);
  printPatternDetails(idxRef);
  for (int i = 0; i < indicesDistrator.size(); ++i) {
    Serial.print("  - Padrão Distrator "); Serial.print(i + 1);
    Serial.print(" (Índice): "); Serial.println(indicesDistrator[i]);
    printPatternDetails(indicesDistrator[i]);
  }

  // 2. ATRIBUIR PADRÕES AOS MOTORES
  int numAlvos = random(2, 5);
  Serial.print("  - Número de Alvos (com padrão referência): "); Serial.println(numAlvos);

  std::vector<int> motoresCandidatos(15);
  std::iota(motoresCandidatos.begin(), motoresCandidatos.end(), 0);
  shuffleVector(motoresCandidatos);

  int8_t indicesGerais[16];
  std::vector<int> motoresAlvo;
  
  for (int i = 0; i < numAlvos; i++) {
    indicesGerais[motoresCandidatos[i]] = idxRef;
    motoresAlvo.push_back(motoresCandidatos[i] + 1);
  }
  
  int distratorAtual = 0;
  for (int i = numAlvos; i < 15; i++) {
    int motorIndex = motoresCandidatos[i];
    int indiceDistratorEscolhido = indicesDistrator[distratorAtual % NUM_DISTRATORES_DIFERENTES];
    indicesGerais[motorIndex] = indiceDistratorEscolhido;
    distratorAtual++;
  }
  indicesGerais[15] = idxRef;

  // 3. IMPRIMIR O MAPA DE ATRIBUIÇÃO
  std::sort(motoresAlvo.begin(), motoresAlvo.end());
  Serial.print("  - Gabarito (Motores com padrão referência): 16 (fixo)");
  for(int motor : motoresAlvo) { Serial.print(", "); Serial.print(motor); }
  Serial.println();
  printPatternDetails(idxRef);
  
  for (int i = 0; i < indicesDistrator.size(); ++i) {
    int idDistrator = indicesDistrator[i];
    Serial.print("  - Motores com Distrator "); Serial.print(i + 1);
    Serial.print(" (Índice "); Serial.print(idDistrator); Serial.print("): ");
    
    bool primeiroMotor = true;
    for (int motorNum = 1; motorNum <= 15; ++motorNum) {
        if (indicesGerais[motorNum - 1] == idDistrator) {
            if (!primeiroMotor) {
                Serial.print(", ");
            }
            Serial.print(motorNum);
            primeiroMotor = false;
        }
    }
    Serial.println();
    printPatternDetails(idDistrator);
  }

  // 4. EXECUTAR E FINALIZAR O ENSAIO
  for (int i = 0; i < 8; i++) {
    playersMaster[i].configure(pwmChannelsMaster[i], motorPinsMaster[i], PATTERNS[indicesGerais[i]]);
  }
  for (int i = 0; i < 8; i++) {
    commandToSlave.pattern_indices[i] = indicesGerais[i + 8];
  }
  esp_now_send(slaveAddress, (uint8_t *) &commandToSlave, sizeof(commandToSlave));
  
  Serial.println("  - Estímulos ativados. Pressione o botão para parar...");
  
  unsigned long startTime = millis();
  while (millis() - startTime < TEMPO_ESTIMULO_MS) {
    for (int i = 0; i < 8; i++) { playersMaster[i].update(); }
    if (digitalRead(BOTAO_PARADA_PIN) == LOW) {
      Serial.println("  - Botão de parada pressionado.");
      break;
    }
    delay(1);
  }
  
  for (int i = 0; i < 8; i++) { playersMaster[i].stop(); }
  for (int i = 0; i < 8; i++) { commandToSlave.pattern_indices[i] = -1; }
  esp_now_send(slaveAddress, (uint8_t *) &commandToSlave, sizeof(commandToSlave));
  
  Serial.println("  - Ensaio finalizado. Pausa para o próximo.");
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  pinMode(BOTAO_PARADA_PIN, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { return; }
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){ return; }
  Serial.println("Mestre pronto. O experimento começará em 5 segundos...");
  delay(5000);
}

void loop() {
  if (ensaioAtual < NUM_ENSAIOS) {
    executarEnsaio();
    delay(TEMPO_ESPERA_ENTRE_ENSAIOS_MS);
  } else {
    Serial.println("\n===== EXPERIMENTO FINALIZADO! =====");
    while(true) { delay(1000); }
  }
}