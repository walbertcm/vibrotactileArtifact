// === CONFIGURAÇÕES GERAIS ===
const int motorAjuste = 2;          
const int motorA = 2;               
const int motorB = 7;               
const int botaoMais = 43; //Botao 0 (1)
const int botaoMenos = 39; //Botao 1 (2)
const int botaoConfirma = 35; //Botao 2 (3)
const int botaoAMaior = 43;       
const int botaoBMaior = 39;       
const int botaoIgual   = 35;      
const int ledPin = 22;

const int freqMin = 40;
const int freqMax = 120;
const int incrementoFreq = 1;
const int numEnsaiosTarefaA = 1;
const int numEnsaiosTarefaB = 1;
const int numEnsaiosTarefaC = 1;
const int numEnsaiosTarefaD = 3;

int botaoPortas[8] = {43, 39, 35, 31, 52, 50, 46, 42};

struct ParComparacao {
  int freqA;
  int freqB;
  int tipo;
};

ParComparacao pares[30];

// === Funções Auxiliares ===
void vibrarComFrequencia(int freq) {
  int periodo = 1000 / freq;
  digitalWrite(motorAjuste, HIGH);
  delay(periodo / 2);
  digitalWrite(motorAjuste, LOW);
  delay(periodo / 2);
}

int lerBotaoResposta() {
  for (int i = 0; i < 8; i++) {
    if (!digitalRead(botaoPortas[i])) {
      delay(200);
      return botaoPortas[i];
    }
  }
  return -1;
}

void aguardarCliqueInicial() {
  Serial.println("Aperte qualquer botão para iniciar o próximo ensaio...");
  while (true) {
    for (int i = 0; i < 8; i++) {
      if (!digitalRead(botaoPortas[i])) {
        delay(200);
        return;
      }
    }
  }
}

void pararMotorAjuste() {
  digitalWrite(motorAjuste, LOW);
}

void pararMotoresComparacao() {
  digitalWrite(motorA, LOW);
  digitalWrite(motorB, LOW);
}

void gerarParesComparacao() {
  int index = 0;
  int deltas[3] = {5, 10, 15};
  for (int i = 0; i < numEnsaiosTarefaD; i++) {
    int base = random(60, 80);
    int delta = deltas[i % 3];
    pares[index++] = {base + delta, base, 0};
    pares[index++] = {base, base + delta, 1};
    pares[index++] = {base, base, 2};
  }

  for (int i = 29; i > 0; i--) {
    int j = random(0, i + 1);
    ParComparacao temp = pares[i];
    pares[i] = pares[j];
    pares[j] = temp;
  }
}




int lerBotaoComparacao() {
  for (int i = 0; i < 8; i++) {
    if (!digitalRead(botaoPortas[i])) {
      delay(200);
      int porta = botaoPortas[i];
      if (porta == botaoAMaior || porta == botaoBMaior || porta == botaoIgual) {
        return porta;
      }
    }
  }
  return -1;
}

// === TAREFAS ===
void executarTarefaAjusteCrescente(int ensaio) {
  int freqAtual = random(55, 65);
  int ajustes = 0;
  unsigned long inicio = millis();
  bool confirmado = false;
  Serial.print("TAR_AJUSTE_CRESCENTE_INICIO;");
  Serial.print("ensaio="); Serial.print(ensaio);
  Serial.print(";freq="); Serial.println(freqAtual);
  digitalWrite(ledPin, HIGH);
  while (!confirmado) {
    vibrarComFrequencia(freqAtual);
    int botao = lerBotaoResposta();
    if (botao == botaoMais) {
      freqAtual = min(freqAtual + incrementoFreq, freqMax);
      ajustes++;
      Serial.print("TAR_AJUSTE_CRESCENTE_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";freq="); Serial.print(freqAtual);
      Serial.print(";tempo="); Serial.print(millis() - inicio);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoMenos) {
      freqAtual = max(freqAtual - incrementoFreq, freqMin);
      ajustes++;
      Serial.print("TAR_AJUSTE_CRESCENTE_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";freq="); Serial.print(freqAtual);
      Serial.print(";tempo="); Serial.print(millis() - inicio);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoConfirma) {
      Serial.print("TAR_AJUSTE_CRESCENTE_CONFIRMA;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";freq="); Serial.print(freqAtual);
      Serial.print(";tempo="); Serial.print(millis() - inicio);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
      confirmado = true;
    }
  }
  pararMotorAjuste();
  digitalWrite(ledPin, LOW);
  aguardarCliqueInicial();
}

void executarTarefaAjusteDecrescente(int ensaio) {
  int freqAtual = random(90, 110);
  int ajustes = 0;
  unsigned long inicio = millis();
  bool confirmado = false;
  Serial.print("TAR_AJUSTE_DECRESCENTE_INICIO;");
  Serial.print("ensaio="); Serial.print(ensaio);
  Serial.print(";freq="); Serial.println(freqAtual);
  digitalWrite(ledPin, HIGH);
  while (!confirmado) {
    vibrarComFrequencia(freqAtual);
    int botao = lerBotaoResposta();
    if (botao == botaoMenos) {
      freqAtual = max(freqAtual - incrementoFreq, freqMin);
      ajustes++;
      Serial.print("TAR_AJUSTE_DECRESCENTE_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";freq="); Serial.print(freqAtual);
      Serial.print(";tempo="); Serial.print(millis() - inicio);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoMais) {
      freqAtual = min(freqAtual + incrementoFreq, freqMax);
      ajustes++;
      Serial.print("TAR_AJUSTE_DECRESCENTE_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";freq="); Serial.print(freqAtual);
      Serial.print(";tempo="); Serial.print(millis() - inicio);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoConfirma) {
      Serial.print("TAR_AJUSTE_DECRESCENTE_CONFIRMA;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";freq="); Serial.print(freqAtual);
      Serial.print(";tempo="); Serial.print(millis() - inicio);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
      confirmado = true;
    }
  }
  pararMotorAjuste();
  digitalWrite(ledPin, LOW);
  aguardarCliqueInicial();
}

void executarTarefaAjusteMaximo(int ensaio) {
  int freqAtual = random(85, 95);
  int ajustes = 0;
  unsigned long inicio = millis();
  bool confirmado = false;
  Serial.print("TAR_AJUSTE_MAXIMO_INICIO;");
  Serial.print("ensaio="); Serial.print(ensaio);
  Serial.print(";freq="); Serial.println(freqAtual);
  digitalWrite(ledPin, HIGH);
  while (!confirmado) {
    vibrarComFrequencia(freqAtual);
    int botao = lerBotaoResposta();
    if (botao == botaoMais) {
      freqAtual = min(freqAtual + incrementoFreq, freqMax);
      ajustes++;
      Serial.print("TAR_AJUSTE_MAXIMO_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";freq="); Serial.print(freqAtual);
      Serial.print(";tempo="); Serial.print(millis() - inicio);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoMenos) {
      freqAtual = max(freqAtual - incrementoFreq, freqMin);
      ajustes++;
      Serial.print("TAR_AJUSTE_MAXIMO_AJUSTE;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";freq="); Serial.print(freqAtual);
      Serial.print(";tempo="); Serial.print(millis() - inicio);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
    } else if (botao == botaoConfirma) {
      Serial.print("TAR_AJUSTE_MAXIMO_CONFIRMA;");
      Serial.print("ensaio="); Serial.print(ensaio);
      Serial.print(";freq="); Serial.print(freqAtual);
      Serial.print(";tempo="); Serial.print(millis() - inicio);
      Serial.print("ms;ajustes="); Serial.println(ajustes);
      confirmado = true;
    }
  }
  pararMotorAjuste();
  digitalWrite(ledPin, LOW);
  aguardarCliqueInicial();
}

void executarTarefaComparacaoPareada(int ensaio) {
  int freqA = pares[ensaio].freqA;
  int freqB = pares[ensaio].freqB;
  int tipo = pares[ensaio].tipo;

 // Verificação de segurança
  if (freqA == 0 || freqB == 0) {
    Serial.println("Erro: frequência inválida detectada.");
    return;
  }

  digitalWrite(ledPin, HIGH);

  // Mensagem de início do ensaio
  Serial.print("TAR_COMPARACAO;");
  Serial.print("ensaio="); Serial.print(ensaio);
  Serial.print(";tipo="); Serial.print(tipo);
  Serial.print(";freqA="); Serial.print(freqA);
  Serial.print(";freqB="); Serial.println(freqB);

  unsigned long inicio = millis();
  int resposta = -1;

  // Vibração contínua até botão válido ser pressionado
  while (true) {
    digitalWrite(motorA, HIGH);
    digitalWrite(motorB, HIGH);
    delay(min(1000 / freqA, 1000 / freqB) / 2);
    digitalWrite(motorA, LOW);
    digitalWrite(motorB, LOW);
    delay(min(1000 / freqA, 1000 / freqB) / 2);

    int botao = lerBotaoComparacao(); // Retorna a PORTA do botão
    if (botao != -1) {
      resposta = botao;
      break;
    }
  }

  unsigned long tempoResposta = millis() - inicio;
  digitalWrite(ledPin, LOW);
  pararMotoresComparacao();

  // Avaliação da resposta
  String resultado = "INCORRETO";
  if ((tipo == 0 && resposta == botaoAMaior) ||
      (tipo == 1 && resposta == botaoBMaior) ||
      (tipo == 2 && resposta == botaoIgual)) {
    resultado = "CORRETO";
  }

  // Mensagem de resultado
  Serial.print("resposta="); Serial.print(resposta);
  Serial.print(";tempo="); Serial.print(tempoResposta);
  Serial.print("ms;acerto="); Serial.println(resultado);

  delay(1000); // Pausa entre ensaios
}





void setup() {
  Serial.begin(115200);
  pinMode(motorAjuste, OUTPUT);
  pinMode(motorA, OUTPUT);
  pinMode(motorB, OUTPUT);
  pinMode(ledPin, OUTPUT);
  for (int i = 0; i < 8; i++) pinMode(botaoPortas[i], INPUT_PULLUP);
  gerarParesComparacao();
  Serial.println("INICIO DO EXPERIMENTO");
}

void loop() {
  for (int i = 0; i < numEnsaiosTarefaA; i++) executarTarefaAjusteCrescente(i);
  for (int i = 0; i < numEnsaiosTarefaB; i++) executarTarefaAjusteDecrescente(i);
  for (int i = 0; i < numEnsaiosTarefaC; i++) executarTarefaAjusteMaximo(i);
  for (int i = 0; i < 30; i++) executarTarefaComparacaoPareada(i);
  pararMotorAjuste();
  pararMotoresComparacao();
  Serial.println("FIM DO EXPERIMENTO");
  while (true);
}