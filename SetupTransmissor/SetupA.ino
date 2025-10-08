void setup() {
  Serial.begin(9600);    // Monitor Serial
  Serial1.begin(9600);   // LoRa
  Serial2.begin(115200); // Radar

  while (!Serial) {
    delay(100);
  }

  // Hex string de teste inicial
  String hex_to_send = "01 06 03 E7 55 AA 86 96";
  sendHexData(hex_to_send);
}

void loop() {
  static String linha = "";

  // Lê dados do radar continuamente
  while (Serial2.available() > 0) {
    char incomingByte = Serial2.read();
    linha += incomingByte;
    Serial.print(incomingByte); // mostra no monitor

    // Quando chegar o fim da linha, envia para o LoRa
    if (incomingByte == '\n') {
      Serial1.print("1002,");   // prefixo de destino
      Serial1.print(linha);     // envia linha completa
      Serial1.println();        // finaliza mensagem
      linha = "";               // limpa para próxima linha

      delay(10000); // espera 5 minutos (300.000 ms) antes de processar a próxima linha
    }
  }
}

void sendHexData(String hexString) {
  int hexStringLength = hexString.length();
  byte hexBytes[hexStringLength / 2];
  for (int i = 0; i < hexStringLength; i += 2) {
    hexBytes[i / 2] = strtoul(hexString.substring(i, i + 2).c_str(), NULL, 16);
  }

  // Envia para o radar
  Serial2.write(hexBytes, sizeof(hexBytes));
}
