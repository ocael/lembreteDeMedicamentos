#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define LED_PIN 2
#define BUZZER_PIN 25
#define POT_PIN 34

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "test.mosquitto.org";

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000); // GMT-3

int horaMedicamento = 14;
int minutoMedicamento = 30;
int horaAnterior = -1;
int minutoAnterior = -1;

bool lembreteAtivo = false;
unsigned long lastBuzzerTime = 0;
bool buzzerState = false;
const int buzzerInterval = 500; // intervalo do som

unsigned long lastPublishTime = 0;
const unsigned long publishHeartbeat = 1000;

void setupWiFi() {
  Serial.print("Conectando-se à ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConectado ao Wi-Fi!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    if (client.connect("ESP32_MedReminder")) {
      Serial.println("Conectado!");
    } else {
      Serial.print("Falha, rc=");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

void publicar(const char* topico, const char* mensagem) {
  if (!client.connected()) reconnect();
  client.publish(topico, mensagem);
  Serial.print("Publicado em ");
  Serial.print(topico);
  Serial.print(": ");
  Serial.println(mensagem);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao inicializar display"));
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 25);
  display.println("Lembrete de Remedios");
  display.display();
  delay(2000);

  setupWiFi();
  client.setServer(mqtt_server, 1883);
  timeClient.begin();
  timeClient.update();
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
  timeClient.update();

  int horaAtual = timeClient.getHours();
  int minutoAtual = timeClient.getMinutes();

  int potValue = analogRead(POT_PIN);
  int totalMinutos = map(potValue, 0, 4095, 0, 1439);
  horaMedicamento = totalMinutos / 60;
  minutoMedicamento = totalMinutos % 60;

  // Publica horário do medicamento se mudou ou a cada segundo
  if (horaMedicamento != horaAnterior || minutoMedicamento != minutoAnterior) {
    horaAnterior = horaMedicamento;
    minutoAnterior = minutoMedicamento;
    char msg[10];
    sprintf(msg, "%02d:%02d", horaMedicamento, minutoMedicamento);
    publicar("medicamento/hora", msg);
  } else if (millis() - lastPublishTime >= publishHeartbeat) {
    char msg[10];
    sprintf(msg, "%02d:%02d", horaMedicamento, minutoMedicamento);
    publicar("medicamento/hora", msg);
    lastPublishTime = millis();
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Hora atual: %02d:%02d", horaAtual, minutoAtual);

  if (horaAtual == horaMedicamento && minutoAtual == minutoMedicamento) {
    digitalWrite(LED_PIN, HIGH);

    // Controle do buzzer intermitente e publicação do alerta
    if (millis() - lastBuzzerTime >= buzzerInterval) {
      buzzerState = !buzzerState;
      if (buzzerState) {
        tone(BUZZER_PIN, 1000);
        publicar("medicamento/alerta", "HORA DO MEDICAMENTO!!"); // PUBLICA ALERTA
      } else {
        noTone(BUZZER_PIN);
      }
      lastBuzzerTime = millis();
    }

    display.setCursor(0, 25);
    display.println("Hora do medicamento!");
    display.setCursor(0, 40);
    display.println("Tome seu remedio!");
  } else {
    lembreteAtivo = false;
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
    display.setCursor(0, 25);
    display.println("Proximo horario:");
    display.setCursor(0, 40);
    display.printf("%02d:%02d", horaMedicamento, minutoMedicamento);
  }

  display.display();
  delay(100);
}