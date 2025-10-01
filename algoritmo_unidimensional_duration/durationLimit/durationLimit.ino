// === CONFIGURAÇÕES GERAIS ===
const int motorLimiar = 2;

const int botaoMais = 43;
const int botaoMenos = 39;
const int botaoConfirma = 35;

const int numEnsaiosLimiarDuracao = 3;
const int incrementoDuracao = 20;
const int duracaoMin = 20;
const int duracaoMax = 1000;
const int freqFixa = 60;

int botaoPortas[8] = {43, 39, 35, 31, 52, 50, 46, 42};
int motorPortas[8] = {12, 11, 10, 9, 8, 7, 6, 5};

// === FUNÇÕES AUXILIARES ===
void vibrarPorDuracao(int freq, int duracaoMs, int motor) {
  int periodo = 1000000 / freq;
  unsigned long t0 = micros();
  while ((micros() - t0) / 1000 < duracaoMs) {
    digitalWrite(motor, HIGH);
    delayMicroseconds(periodo / 2);
    digitalWrite(motor, LOW);
    delayMicroseconds(periodo / 2);
  }
}

int lerBotaoLimiar() {
  for (int i = 0; i < 8; i++) {
    if (!digitalRead(botaoPortas[i])) {
      delay(200);  // debounce
      return botaoPortas[i];
    }
  }
  return -1;
}

void pararTodosOsMotores() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(motorPortas[i], LOW);
  }
}

// === TAREFA: LIMIAR DE DURAÇÃO ===
void executarTarefaLimiarDuracao(int ensaio) {
  int duracaoAtual = 500;
  int ajustes = 0;
  unsigned long inicio = millis();

  Serial.print("TAR_LIMIAR_DURACAO_INICIO;");
  Serial.print("ensaio="); Serial.print(ensaio);
  Serial.print(";duracao="); Serial.print(duracaoAtual);
  Serial.println("ms");

  bool confirmado = false;

  while (!confirmado) {
    vibrarPorDuracao(freqFixa, duracaoAtual, motorLimiar);
    delay(300);

    int botao = -1;
    while (botao == -1) {
      botao = lerBotaoLimiar();
      delay(100);
    }

    if (botao == botaoMais) {
      duracaoAtual = min(duracaoAtual + incrementoDuracao, duracaoMax);
      ajustes++;
      Serial.print("TAR_LIMIAR_DURACAO_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";duracao="); Serial.print(duracaoAtual);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoMenos) {
      duracaoAtual = max(duracaoAtual - incrementoDuracao, duracaoMin);
      ajustes++;
      Serial.print("TAR_LIMIAR_DURACAO_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";duracao="); Serial.print(duracaoAtual);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoConfirma) {
      unsigned long tempoTotal = millis() - inicio;
      Serial.print("TAR_LIMIAR_DURACAO_CONFIRMA;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";duracao="); Serial.print(duracaoAtual);
      Serial.print("ms;ajustes="); Serial.print(ajustes);
      Serial.print(";tempo="); Serial.print(tempoTotal);
      Serial.println("ms");
      confirmado = true;
    }

    delay(200);
  }

  digitalWrite(motorLimiar, LOW);
  delay(1000);
}

// === SETUP E LOOP ===
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 8; i++) {
    pinMode(botaoPortas[i], INPUT_PULLUP);
    pinMode(motorPortas[i], OUTPUT);
    digitalWrite(motorPortas[i], LOW);
  }

  Serial.println("INICIO DO EXPERIMENTO - LIMIAR DE DURAÇÃO");
}

void loop() {
  for (int i = 0; i < numEnsaiosLimiarDuracao; i++) {
    executarTarefaLimiarDuracao(i);
  }

  pararTodosOsMotores();
  Serial.println("FIM DO EXPERIMENTO");
  while (true);
}