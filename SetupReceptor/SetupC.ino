#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Configurações WiFi
const char* ssid = "";
const char* password = "";

// Configurações Firebase
const char* firebaseUrl = "";

// Variáveis para LoRa
String loraBuffer = "";
unsigned long lastReceiveTime = 0;
const unsigned int receiveTimeout = 1000;

WiFiClientSecure espClient;

// Variável para armazenar status do envio
String fbStatus = "";

void setup_wifi() {
  Serial.begin(9600);
  Serial1.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // Espera até conectar
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println("\nWiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// Função para enviar para Firebase
void sendToFirebase(String valorPuro, String originalPayload) {
  if (WiFi.status() != WL_CONNECTED) {
    fbStatus = "[FB] WiFi desconectado";
    return;
  }

  HTTPClient http;
  espClient.setInsecure();
  espClient.setTimeout(15000);

  if (http.begin(espClient, firebaseUrl)) {
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);

    DynamicJsonDocument firebaseDoc(512);
    firebaseDoc["nivel"] = valorPuro;
    firebaseDoc["topic"] = "nivel-rio";
    firebaseDoc["originalPayload"] = originalPayload;

    String firebasePayload;
    serializeJson(firebaseDoc, firebasePayload);

    int httpResponseCode = http.POST(firebasePayload);

    if (httpResponseCode > 0) {
      fbStatus = "[FB] OK (" + String(httpResponseCode) + ")";
    } else {
      fbStatus = "[FB] Erro: " + String(httpResponseCode);
    }

    http.end();
  } else {
    fbStatus = "[FB] Falha na conexão";
  }
}

// Função para extrair valor numérico
String extractNumericValue(String data) {
  String result = "";
  bool hasDecimal = false;

  for (int i = 0; i < data.length(); i++) {
    char c = data.charAt(i);

    if (isdigit(c)) {
      result += c;
    } else if (c == '.' && !hasDecimal && result.length() > 0) {
      result += c;
      hasDecimal = true;
    } else if (result.length() > 0) {
      break;
    }
  }

  while (result.length() > 1 && result.charAt(0) == '0' && result.charAt(1) != '.') {
    result = result.substring(1);
  }

  return result;
}

void processLoraData(String data) {
  data.trim();

  // Só processa mensagens destinadas a este dispositivo (1004)
  if (!data.startsWith("1004,")) {
    return;
  }

  String conteudo = data.substring(5);

  // Mostra no Serial
  Serial.print("Recebido via B: ");
  Serial.println(conteudo);

  String valorPuro = extractNumericValue(conteudo);
  if (valorPuro.length() == 0) {
    Serial.println("[LoRa] Sem valor numérico");
    return;
  }

  Serial.print("[Valor] ");
  Serial.println(valorPuro);

  // Reseta o status
  fbStatus = "";

  // Envia para Firebase
  sendToFirebase(valorPuro, conteudo);

  Serial.println(fbStatus);
  Serial.println("----");
}

void setup() {
  setup_wifi();
  Serial.println("\n=== Sistema Iniciado ===");
  Serial.println("Aguardando dados LoRa...");
  Serial.println("----");
}

void loop() {
  while (Serial1.available() > 0) {
    char inChar = (char)Serial1.read();
    loraBuffer += inChar;
    lastReceiveTime = millis();

    if (inChar == '\n' || inChar == '\r') {
      processLoraData(loraBuffer);
      loraBuffer = "";
    }
  }

  if (loraBuffer.length() > 0 && (millis() - lastReceiveTime > receiveTimeout)) {
    processLoraData(loraBuffer);
    loraBuffer = "";
  }

  // Verificação de conexão WiFi
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 10000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Reconectando...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
    lastWifiCheck = millis();
  }
}
