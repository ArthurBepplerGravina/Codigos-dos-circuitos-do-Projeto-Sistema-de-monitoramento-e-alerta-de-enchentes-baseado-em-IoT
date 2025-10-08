#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Configurações WiFi
const char* ssid = "";
const char* password = "";

// Configurações Firebase
const char* firebaseUrl = "";

// Configurações Google Sheets
const char* googleScriptUrl = "";

// Variáveis para LoRa
String loraBuffer = "";
unsigned long lastReceiveTime = 0;
const unsigned int receiveTimeout = 1000;

// NTP Client para obter hora atual
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000); // UTC-3 (Brasília)

WiFiClientSecure espClient;

// Variáveis para armazenar status dos envios
String fbStatus = "";
String gsStatus = "";

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

  // Inicializa NTP Client
  timeClient.begin();
  timeClient.update();
}

// Função para obter data e hora formatadas (YYYY-MM-DD HH:MM:SS)
String getFormattedDateTime() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&epochTime);

  int year = ptm->tm_year + 1900;
  int month = ptm->tm_mon + 1;
  int day = ptm->tm_mday;
  int hour = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int seconds = timeClient.getSeconds();

  char datetime[20];
  snprintf(datetime, sizeof(datetime), "%04d-%02d-%02d %02d:%02d:%02d",
           year, month, day, hour, minutes, seconds);

  return String(datetime);
}

// Função para enviar dados para Google Sheets
void sendToGoogleSheets(String valorPuro, String timestamp, String originalPayload) {
  if (WiFi.status() != WL_CONNECTED) {
    gsStatus = "[GS] WiFi desconectado";
    return;
  }

  HTTPClient http;
  espClient.setInsecure();
  espClient.setTimeout(15000);

  String url = String(googleScriptUrl) +
               "?timestamp=" + URLEncode(timestamp) +
               "&nivel=" + URLEncode(valorPuro) +
               "&payload=" + URLEncode(originalPayload);

  if (http.begin(espClient, url)) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      gsStatus = "[GS] OK (" + String(httpResponseCode) + ")";
    } else {
      gsStatus = "[GS] Erro: " + String(httpResponseCode);
    }

    http.end();
  } else {
    gsStatus = "[GS] Falha na conexão";
  }
}

// Função para enviar para Firebase
void sendToFirebase(String valorPuro, String timestamp, String originalPayload) {
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
    firebaseDoc["timestamp"] = timestamp;
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

// Função para codificar URL
String URLEncode(String input) {
  String encoded = "";
  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      encoded += '%';
      if (c < 16) encoded += '0';
      encoded += String(c, HEX);
    }
  }
  return encoded;
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
  String timestamp = getFormattedDateTime();

  // Mostra no Serial com horário
  Serial.print("[");
  Serial.print(timestamp);
  Serial.print("] Recebido via B: ");
  Serial.println(conteudo);

  String valorPuro = extractNumericValue(conteudo);
  if (valorPuro.length() == 0) {
    Serial.println("[LoRa] Sem valor numérico");
    return;
  }

  Serial.print("[Valor] ");
  Serial.println(valorPuro);

  // Reseta os status
  fbStatus = "";
  gsStatus = "";

  // Envia para Firebase
  sendToFirebase(valorPuro, timestamp, conteudo);

  // Envia para Google Sheets
  sendToGoogleSheets(valorPuro, timestamp, conteudo);

  Serial.println(fbStatus + " " + gsStatus);
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

  // Atualiza o horário periodicamente
  timeClient.update();
}
