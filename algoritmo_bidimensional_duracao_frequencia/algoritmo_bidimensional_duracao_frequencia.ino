#include <Arduino.h>

// --- Enum para controlar o estado do experimento ---
enum TestState {
  TEST_SINGLE_EVAL,   // Estágio 1: Avaliação individual
  TEST_PAIR_COMPARISON, // Estágio 2: Comparação em pares
  ALL_TESTS_DONE      // Fim de todo o processo
};
TestState currentTestState = TEST_SINGLE_EVAL;

// --- Configuração Geral de Hardware ---
const int MOTOR_1_PIN = 26; // Motor para o Teste 1
const int MOTOR_3_PIN = 25; // Motor A para o Teste 2
const int MOTOR_4_PIN = 33; // Motor B para o Teste 2

const int BUTTON_3_PIN = 23;
const int BUTTON_4_PIN = 22;
const int BUTTON_5_PIN = 19;

// --- Configuração Geral de PWM ---
const int PWM_CHANNEL_1 = 0; // Para motor 1
const int PWM_CHANNEL_3 = 1; // Para motor 3
const int PWM_CHANNEL_4 = 2; // Para motor 4
const int PWM_RESOLUTION = 8;
const int PWM_DUTY_CYCLE = 127; // Intensidade do motor (meia força)

// --- Estrutura e Lista de Padrões (igual ao anterior) ---
struct PulsePattern {
  float duration_ms;
  float frequency_hz;
};
const int NUM_PATTERNS = 13;
PulsePattern patterns[NUM_PATTERNS] = {
  {50.0, 150.0}, {91.0, 107.9}, {165.6, 77.5}, {301.4, 55.7},
  {548.5, 40.0}, {50.0, 40.0}, {91.0, 55.7}, {301.4, 107.9},
  {548.5, 150.0}, {50.0, 77.5}, {1000.0, 77.5}, {301.4, 40.0},
  {301.4, 150.0}
};

// --- Variáveis de Controle para o TESTE 1 ---
int patternOrder[NUM_PATTERNS];
int currentPatternIdx_T1 = 0;
bool isMotor1On = false;
unsigned long lastChangeTime_M1 = 0;

// --- Variáveis de Controle para o TESTE 2 ---
// Fórmula de combinação: C(13, 2) = (13 * 12) / 2 = 78
const int NUM_COMBINATIONS = 78; 
struct Combination {
  int indexA;
  int indexB;
};
Combination combinations[NUM_COMBINATIONS];
int combinationOrder[NUM_COMBINATIONS];
int currentCombinationIdx_T2 = 0;
bool isMotor3On = false;
bool isMotor4On = false;
unsigned long lastChangeTime_M3 = 0;
unsigned long lastChangeTime_M4 = 0;

// --- Controle de Botões (Debounce) ---
unsigned long lastButtonPressTime = 0;
const long DEBOUNCE_DELAY_MS = 300;
const int PAUSE_BETWEEN_PULSES_MS = 200;

// --- Protótipos das funções ---
void setupComparisonTest();
void applyCombination(int combinationIndex);

// =========================================================================
// ==                           FUNÇÕES DE SETUP                          ==
// =========================================================================

// Função genérica para embaralhar um array de inteiros
void shuffleArray(int arr[], int size) {
  for (int i = size - 1; i > 0; i--) {
    int j = random(0, i + 1);
    int temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
  }
}

// Aplica o padrão para o Teste 1
void applyPattern(int patternIndex) {
  PulsePattern currentPattern = patterns[patternOrder[patternIndex]];
  ledcSetup(PWM_CHANNEL_1, currentPattern.frequency_hz, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_1_PIN, PWM_CHANNEL_1);
  Serial.println("----------------------------------------");
  Serial.print("TESTE 1: Apresentando Padrão ");
  Serial.print(currentPatternIdx_T1 + 1); Serial.print("/"); Serial.print(NUM_PATTERNS);
  Serial.print(": (Dur: "); Serial.print(currentPattern.duration_ms);
  Serial.print("ms, Freq: "); Serial.print(currentPattern.frequency_hz); Serial.println("Hz)");
  isMotor1On = false;
  lastChangeTime_M1 = millis();
}

void setup() {
  Serial.begin(115220);
  randomSeed(analogRead(0));

  // Configura todos os botões
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  pinMode(BUTTON_4_PIN, INPUT_PULLUP);
  pinMode(BUTTON_5_PIN, INPUT_PULLUP);

  // Inicia o TESTE 1
  Serial.println("======= INICIANDO ESTÁGIO 1: AVALIAÇÃO INDIVIDUAL =======");
  for (int i = 0; i < NUM_PATTERNS; i++) { patternOrder[i] = i; }
  shuffleArray(patternOrder, NUM_PATTERNS);
  applyPattern(currentPatternIdx_T1);
}

// =========================================================================
// ==                FUNÇÕES DO TESTE DE COMPARAÇÃO (TESTE 2)             ==
// =========================================================================

void setupComparisonTest() {
  // Desliga o motor do teste anterior
  ledcWrite(PWM_CHANNEL_1, 0);
  ledcDetachPin(MOTOR_1_PIN);

  Serial.println("\n======= INICIANDO ESTÁGIO 2: COMPARAÇÃO DE PARES =======");
  
  // 1. Gerar todas as 78 combinações
  int count = 0;
  for (int i = 0; i < NUM_PATTERNS; i++) {
    for (int j = i + 1; j < NUM_PATTERNS; j++) {
      if (count < NUM_COMBINATIONS) {
        combinations[count].indexA = i;
        combinations[count].indexB = j;
        count++;
      }
    }
  }

  // 2. Criar e embaralhar a ordem de apresentação das combinações
  for (int i = 0; i < NUM_COMBINATIONS; i++) { combinationOrder[i] = i; }
  shuffleArray(combinationOrder, NUM_COMBINATIONS);

  // 3. Aplica a primeira combinação
  applyCombination(currentCombinationIdx_T2);
}

void applyCombination(int combinationIndex) {
  int actualCombinationIdx = combinationOrder[combinationIndex];
  int patternIdxA = combinations[actualCombinationIdx].indexA;
  int patternIdxB = combinations[actualCombinationIdx].indexB;

  PulsePattern patternA = patterns[patternIdxA];
  PulsePattern patternB = patterns[patternIdxB];

  // Configura Motor 3 (A)
  ledcSetup(PWM_CHANNEL_3, patternA.frequency_hz, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_3_PIN, PWM_CHANNEL_3);
  
  // Configura Motor 4 (B)
  ledcSetup(PWM_CHANNEL_4, patternB.frequency_hz, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_4_PIN, PWM_CHANNEL_4);

  Serial.println("----------------------------------------");
  Serial.print("TESTE 2: Apresentando Combinação ");
  Serial.print(currentCombinationIdx_T2 + 1); Serial.print("/"); Serial.print(NUM_COMBINATIONS);
  Serial.println();

  // Reinicia o estado dos motores
  isMotor3On = false;
  isMotor4On = false;
  lastChangeTime_M3 = millis();
  lastChangeTime_M4 = millis();
}


// =========================================================================
// ==                            LOOP PRINCIPAL                           ==
// =========================================================================

void loop() {
  unsigned long currentTime = millis();
  int buttonPressed = 0;
  
  // --- Leitura dos botões (com debounce) ---
  if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY_MS) {
    if (digitalRead(BUTTON_3_PIN) == LOW) buttonPressed = 3;
    else if (digitalRead(BUTTON_4_PIN) == LOW) buttonPressed = 4;
    else if (digitalRead(BUTTON_5_PIN) == LOW) buttonPressed = 5;

    if (buttonPressed > 0) {
      lastButtonPressTime = currentTime; // Atualiza o tempo do debounce
    }
  }

  // --- Máquina de Estados: executa o código do teste atual ---
  switch (currentTestState) {
    
    //-------------------------------------------------------------
    case TEST_SINGLE_EVAL: {
      // Lógica de pulso para o motor 1
      PulsePattern currentPattern = patterns[patternOrder[currentPatternIdx_T1]];
      if (isMotor1On) {
        if (currentTime - lastChangeTime_M1 >= currentPattern.duration_ms) {
          ledcWrite(PWM_CHANNEL_1, 0); isMotor1On = false; lastChangeTime_M1 = currentTime;
        }
      } else {
        if (currentTime - lastChangeTime_M1 >= PAUSE_BETWEEN_PULSES_MS) {
          ledcWrite(PWM_CHANNEL_1, PWM_DUTY_CYCLE); isMotor1On = true; lastChangeTime_M1 = currentTime;
        }
      }

      // Lógica de resposta para o Teste 1 (botões 3 e 4)
      if (buttonPressed == 3 || buttonPressed == 4) {
        Serial.print(">> RESPOSTA T1: D=");
        Serial.print(currentPattern.duration_ms); Serial.print(", F=");
        Serial.print(currentPattern.frequency_hz); Serial.print(" -> Botão: ");
        Serial.println(buttonPressed);
        
        currentPatternIdx_T1++;
        if (currentPatternIdx_T1 >= NUM_PATTERNS) {
          // Fim do Teste 1, transição para o Teste 2
          currentTestState = TEST_PAIR_COMPARISON;
          setupComparisonTest();
        } else {
          applyPattern(currentPatternIdx_T1);
        }
      }
      break;
    }

    //-------------------------------------------------------------
    case TEST_PAIR_COMPARISON: {
      int actualCombIdx = combinationOrder[currentCombinationIdx_T2];
      PulsePattern patternA = patterns[combinations[actualCombIdx].indexA];
      PulsePattern patternB = patterns[combinations[actualCombIdx].indexB];

      // Lógica de pulso para o motor 3 (A)
      if (isMotor3On) {
        if (currentTime - lastChangeTime_M3 >= patternA.duration_ms) {
          ledcWrite(PWM_CHANNEL_3, 0); isMotor3On = false; lastChangeTime_M3 = currentTime;
        }
      } else {
        if (currentTime - lastChangeTime_M3 >= PAUSE_BETWEEN_PULSES_MS) {
          ledcWrite(PWM_CHANNEL_3, PWM_DUTY_CYCLE); isMotor3On = true; lastChangeTime_M3 = currentTime;
        }
      }

      // Lógica de pulso para o motor 4 (B)
      if (isMotor4On) {
        if (currentTime - lastChangeTime_M4 >= patternB.duration_ms) {
          ledcWrite(PWM_CHANNEL_4, 0); isMotor4On = false; lastChangeTime_M4 = currentTime;
        }
      } else {
        if (currentTime - lastChangeTime_M4 >= PAUSE_BETWEEN_PULSES_MS) {
          ledcWrite(PWM_CHANNEL_4, PWM_DUTY_CYCLE); isMotor4On = true; lastChangeTime_M4 = currentTime;
        }
      }

      // Lógica de resposta para o Teste 2 (botões 3, 4 e 5)
      if (buttonPressed > 0) { // Qualquer botão pressionado avança
        // Formato de saída: A(X1,Y1);B(X2,Y2);numero do botao
        Serial.print("A("); Serial.print(patternA.duration_ms);
        Serial.print(","); Serial.print(patternA.frequency_hz);
        Serial.print(");B("); Serial.print(patternB.duration_ms);
        Serial.print(","); Serial.print(patternB.frequency_hz);
        Serial.print(");"); Serial.println(buttonPressed);

        currentCombinationIdx_T2++;
        if (currentCombinationIdx_T2 >= NUM_COMBINATIONS) {
          // Fim do Teste 2
          currentTestState = ALL_TESTS_DONE;
          ledcDetachPin(MOTOR_3_PIN);
          ledcDetachPin(MOTOR_4_PIN);
          Serial.println("\n--- FIM DE TODOS OS TESTES ---");
        } else {
          applyCombination(currentCombinationIdx_T2);
        }
      }
      break;
    }

    //-------------------------------------------------------------
    case ALL_TESTS_DONE: {
      // Não faz nada. O experimento acabou.
      return;
    }
  }
}