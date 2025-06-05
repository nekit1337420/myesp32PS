#include <WiFi.h>
#include <WebSocketsServer.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <time.h>

const char* ssid = "Boga";
const char* password = "bogdan2003";

#define DHTPIN 4
#define DHTTYPE DHT22
#define BUTTON_PIN 2
#define LED_PIN 7

DHT dht(DHTPIN, DHTTYPE);
WebSocketsServer webSocket = WebSocketsServer(81);

int floodCount = 0;
bool ledState = false;          // Overall LED state
bool buttonTriggered = false;   // Flag for button-triggered state
unsigned long ledOnTime = 0;    // Time when LED was turned on

int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
const unsigned long ledOnDuration = 5000; // 5 seconds

const char* ntfyTopic = "myesp32flood";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 10800; // EEST is UTC+3
const int   daylightOffset_sec = 0;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("Client %u disconnected\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("Client %u connected\n", num);
      break;
    case WStype_TEXT:
      Serial.printf("Received from client %u: %s\n", num, payload);
      if (length > 0) {
        char message[64];
        strncpy(message, (char *)payload, length);
        message[length] = '\0';
        if (strcmp(message, "led_on") == 0) {
          ledState = true;
          buttonTriggered = false; // WebSocket control, no timeout
          digitalWrite(LED_PIN, HIGH);
          Serial.println("LED turned ON via WebSocket (persistent)");
        } else if (strcmp(message, "led_off") == 0) {
          ledState = false;
          buttonTriggered = false;
          digitalWrite(LED_PIN, LOW);
          Serial.println("LED turned OFF via WebSocket");
        }
      }
      break;
  }
}

void sendNotification() {
  if (WiFi.status() == WL_CONNECTED) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d at %H:%M:%S", &timeinfo);

    HTTPClient http;
    String url = "https://ntfy.sh/" + String(ntfyTopic);
    http.begin(url);
    http.addHeader("Content-Type", "text/plain");
    String message = "⚠️ ALERTĂ INUNDAȚIE detected on " + String(timeStr);
    int httpCode = http.POST(message);
    if (httpCode > 0) {
      Serial.println("Notification sent via ntfy");
    } else {
      Serial.println("Notification send failed, HTTP code: " + String(httpCode));
      delay(1000);
      httpCode = http.POST(message);
      if (httpCode > 0) Serial.println("Retry succeeded");
      else Serial.println("Retry failed");
    }
    http.end();
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  dht.begin();
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    delay(1000);
    Serial.print("Connecting to WiFi... Status: ");
    Serial.println(WiFi.status());
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.println("Time synchronized with NTP server");
      char timeStr[32];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
      Serial.println(timeStr);
    } else {
      Serial.println("Failed to synchronize time");
    }
  } else {
    Serial.println("Failed to connect to WiFi");
  }
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

void loop() {
  webSocket.loop();
  if (WiFi.status() == WL_CONNECTED) {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) {
      Serial.print("Temperature: ");
      Serial.print(temp);
      Serial.print("°C | Humidity: ");
      Serial.print(hum);
      Serial.println("%");
    } else {
      Serial.println("Failed to read from DHT sensor!");
    }

    int buttonState = digitalRead(BUTTON_PIN);
    if (buttonState != lastButtonState) {
      lastDebounceTime = millis();
    }
    if (millis() - lastDebounceTime > debounceDelay && buttonState == HIGH) {
      Serial.println("Button pressed detected");
      if (floodCount < 10) {
        floodCount++;
        ledState = true;
        buttonTriggered = true; // Mark as button-triggered
        ledOnTime = millis();
        digitalWrite(LED_PIN, HIGH);
        Serial.print("ALERTA_INUNDAȚIE - Count: ");
        Serial.println(floodCount);
        sendNotification();
      }
    }
    // Turn off LED after 5 seconds only if triggered by button
    if (buttonTriggered && ledState && millis() - ledOnTime >= ledOnDuration) {
      ledState = false;
      buttonTriggered = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED turned OFF automatically after 5 seconds");
    }

    lastButtonState = buttonState;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "{\"temperature\":%.1f,\"humidity\":%.1f,\"ledState\":\"%s\",\"floodCount\":%d}",
             temp, hum, ledState ? "ON" : "OFF", floodCount);
    webSocket.broadcastTXT(buffer);
  } else {
    Serial.println("WiFi not connected");
  }

  delay(1000);
}