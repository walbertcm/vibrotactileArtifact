// === CONFIGURAÇÕES GERAIS ===
const int motorLimiar = 2;
const int motorA = 2;
const int motorB = 7;

const int botaoMais = 43;
const int botaoMenos = 39;
const int botaoConfirma = 35;

const int botaoAMaior = 43;
const int botaoBMaior = 39;
const int botaoIgual = 35;

const int freqFixa = 60;

const int incrementoDuracaoMin = 20;
const int incrementoDuracaoMax = 20;

const int numEnsaiosTarefaA = 3;
const int numEnsaiosTarefaB = 3;
const int numEnsaiosTipo = 10; // para Tarefa C
const int numEnsaiosTarefaC = numEnsaiosTipo * 3;

const int duracaoMin = 20;
const int duracaoMax = 1500;
const int duracaoBaseMin = 300;
const int duracaoBaseMax = 700;

int botaoPortas[8] = {43, 39, 35, 31, 52, 50, 46, 42};
int motorPortas[8] = {12, 11, 10, 9, 8, 7, 6, 5};

// === ESTRUTURA PARA TAREFA C ===
struct ParDuracao {
  int durA;
  int durB;
  int tipo;  // 0 = A>B, 1 = B>A, 2 = A=B
};
ParDuracao paresDuracao[numEnsaiosTarefaC];

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

int lerBotaoRestrito(int b1, int b2, int b3) {
  while (true) {
    for (int i = 0; i < 8; i++) {
      if (!digitalRead(botaoPortas[i])) {
        if (botaoPortas[i] == b1 || botaoPortas[i] == b2 || botaoPortas[i] == b3) {
          delay(200);
          return botaoPortas[i];
        }
      }
    }
  }
}

void pararTodosOsMotores() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(motorPortas[i], LOW);
  }
}

void aguardarCliqueInicial() {
  Serial.println("Aperte qualquer botão para iniciar a próxima tarefa...");
  while (true) {
    for (int i = 0; i < 8; i++) {
      if (!digitalRead(botaoPortas[i])) {
        delay(300);
        return;
      }
    }
  }
}

// === TAREFA A ===
void executarTarefaLimiarDuracaoMin(int ensaio) {
  int duracaoAtual = 500;
  int ajustes = 0;
  unsigned long inicio = millis();

  Serial.print("TAR_LIMIAR_DURACAO_MIN_INICIO;");
  Serial.print("ensaio="); Serial.print(ensaio);
  Serial.print(";duracao="); Serial.print(duracaoAtual);
  Serial.println("ms");

  while (true) {
    vibrarPorDuracao(freqFixa, duracaoAtual, motorLimiar);
    delay(300);
    int botao = lerBotaoRestrito(botaoMais, botaoMenos, botaoConfirma);

    if (botao == botaoMais) {
      duracaoAtual = min(duracaoAtual + incrementoDuracaoMin, duracaoMax);
      ajustes++;
      Serial.print("TAR_LIMIAR_DURACAO_MIN_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";duracao="); Serial.print(duracaoAtual);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoMenos) {
      duracaoAtual = max(duracaoAtual - incrementoDuracaoMin, duracaoMin);
      ajustes++;
      Serial.print("TAR_LIMIAR_DURACAO_MIN_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";duracao="); Serial.print(duracaoAtual);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoConfirma) {
      unsigned long tempoTotal = millis() - inicio;
      Serial.print("TAR_LIMIAR_DURACAO_MIN_CONFIRMA;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";duracao="); Serial.print(duracaoAtual);
      Serial.print("ms;ajustes="); Serial.print(ajustes);
      Serial.print(";tempo="); Serial.print(tempoTotal);
      Serial.println("ms");
      break;
    }
  }
  digitalWrite(motorLimiar, LOW);
  delay(1000);
}

// === TAREFA B ===
void executarTarefaLimiarDuracaoMax(int ensaio) {
  int duracaoAtual = 400;
  int ajustes = 0;
  unsigned long inicio = millis();

  Serial.print("TAR_LIMIAR_DURACAO_MAX_INICIO;");
  Serial.print("ensaio="); Serial.print(ensaio);
  Serial.print(";duracao="); Serial.print(duracaoAtual);
  Serial.println("ms");

  while (true) {
    vibrarPorDuracao(freqFixa, duracaoAtual, motorLimiar);
    delay(300);
    int botao = lerBotaoRestrito(botaoMais, botaoMenos, botaoConfirma);

    if (botao == botaoMais) {
      duracaoAtual = min(duracaoAtual + incrementoDuracaoMax, duracaoMax);
      ajustes++;
      Serial.print("TAR_LIMIAR_DURACAO_MAX_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";duracao="); Serial.print(duracaoAtual);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoMenos) {
      duracaoAtual = max(duracaoAtual - incrementoDuracaoMax, duracaoMin);
      ajustes++;
      Serial.print("TAR_LIMIAR_DURACAO_MAX_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";duracao="); Serial.print(duracaoAtual);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoConfirma) {
      unsigned long tempoTotal = millis() - inicio;
      Serial.print("TAR_LIMIAR_DURACAO_MAX_CONFIRMA;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";duracao="); Serial.print(duracaoAtual);
      Serial.print("ms;ajustes="); Serial.print(ajustes);
      Serial.print(";tempo="); Serial.print(tempoTotal);
      Serial.println("ms");
      break;
    }
  }
  digitalWrite(motorLimiar, LOW);
  delay(1000);
}

// === TAREFA C ===
void gerarParesComparacaoDuracao() {
  int index = 0;
  int deltas[] = {50, 100, 150};
  for (int t = 0; t < numEnsaiosTipo; t++) {
    int base = random(duracaoBaseMin, duracaoBaseMax);
    int delta = deltas[random(0, 3)];

    paresDuracao[index++] = {base + delta, base, 0};
    paresDuracao[index++] = {base, base + delta, 1};
    paresDuracao[index++] = {base, base, 2};
  }

  for (int i = numEnsaiosTarefaC - 1; i > 0; i--) {
    int j = random(0, i + 1);
    ParDuracao temp = paresDuracao[i];
    paresDuracao[i] = paresDuracao[j];
    paresDuracao[j] = temp;
  }
}

void executarTarefaComparacaoDuracaoPareada(int ensaio) {
  int durA = paresDuracao[ensaio].durA;
  int durB = paresDuracao[ensaio].durB;
  int tipo = paresDuracao[ensaio].tipo;

  Serial.print("TAR_COMPARACAO_DURACAO;");
  Serial.print("ensaio="); Serial.print(ensaio);
  Serial.print(";tipo="); Serial.print(tipo);
  Serial.print(";durA="); Serial.print(durA);
  Serial.print(";durB="); Serial.println(durB);

  digitalWrite(motorA, LOW);
  digitalWrite(motorB, LOW);

  unsigned long inicio = millis();
  vibrarPorDuracao(freqFixa, durA, motorA);
  vibrarPorDuracao(freqFixa, durB, motorB);
  unsigned long tempoResp = millis() - inicio;

  int resposta = lerBotaoRestrito(botaoAMaior, botaoBMaior, botaoIgual);
  String acerto = "INCORRETO";
  if ((tipo == 0 && resposta == botaoAMaior) ||
      (tipo == 1 && resposta == botaoBMaior) ||
      (tipo == 2 && resposta == botaoIgual)) {
    acerto = "CORRETO";
  }

  Serial.print("resposta="); Serial.print(resposta);
  Serial.print(";tempo="); Serial.print(tempoResp);
  Serial.print("ms;acerto="); Serial.println(acerto);
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

  gerarParesComparacaoDuracao();
  Serial.println("INICIO DO EXPERIMENTO");
}

void loop() {
  for (int i = 0; i < numEnsaiosTarefaA; i++) {
    aguardarCliqueInicial();
    executarTarefaLimiarDuracaoMin(i);
  }

  for (int i = 0; i < numEnsaiosTarefaB; i++) {
    aguardarCliqueInicial();
    executarTarefaLimiarDuracaoMax(i);
  }

  for (int i = 0; i < numEnsaiosTarefaC; i++) {
    aguardarCliqueInicial();
    executarTarefaComparacaoDuracaoPareada(i);
  }

  pararTodosOsMotores();
  Serial.println("FIM DO EXPERIMENTO");
  while (true);
}