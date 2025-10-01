// GPIOs atualizadas dos botões
const int botaoPins[8] = {15, 13, 23, 22, 19, 18, 5, 17};
bool botaoEstadoAnterior[8] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};

void setup() {
  Serial.begin(115200);

  // Configura os pinos como entrada com resistor pull-up
  for (int i = 0; i < 8; i++) {
    pinMode(botaoPins[i], INPUT_PULLUP);
  }

  Serial.println("Testador de botões iniciado.");
  Serial.println("Pressione qualquer botão para ver o número correspondente.");
}

void loop() {
  for (int i = 0; i < 8; i++) {
    int leitura = digitalRead(botaoPins[i]);

    // Detecta botão pressionado
    if (leitura == LOW && botaoEstadoAnterior[i] == HIGH) {
      Serial.print("Botão ");
      Serial.print(i);
      Serial.println(" pressionado");
    }

    botaoEstadoAnterior[i] = leitura;
  }

  delay(20); // debounce básico
}

