String linhaRecebida = "";

void setup() {
  Serial.begin(9600);     // Monitor Serial
  Serial1.begin(9600);    // Recebe do LoRa A
  Serial2.begin(9600);    // Envia para o LoRa C

  // Configura endereço deste LoRa (B = 1002)
  delay(1000);
  Serial1.print("AT+ADDRESS=1002\r\n");
  Serial2.print("AT+ADDRESS=1003\r\n");
  delay(100);

  Serial.println("LoRa B - Repetidor inicializado");
}

void loop() {
  // Lê dados do LoRa A
  while (Serial1.available() > 0) {
    char incomingByte = Serial1.read();
    linhaRecebida += incomingByte;

    // Se chegar o fim da linha, processa
    if (incomingByte == '\n') {
      linhaRecebida.trim(); // remove espaços e quebras extras

      // Filtra apenas mensagens destinadas a B (prefixo 1002,)
      if (linhaRecebida.startsWith("1002,")) {
        String conteudo = linhaRecebida.substring(5); // remove prefixo

        // Reenvia para C com prefixo 1004
        Serial2.print("1004,");
        Serial2.print(conteudo);
        Serial2.println();

        Serial.print("Recebido de A e enviado para C: ");
        Serial.println(conteudo);
      }

      linhaRecebida = ""; // limpa buffer
    }
  }
}
