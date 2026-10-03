/*
 * SETU-V2 - ESP32-S3 Dashboard Host Node
 * 
 * Runs LoRa mesh + Wi-Fi AP dashboard simultaneously.
 * This is the "gateway" node that hosts the web dashboard
 * and also participates in the mesh as a full peer.
 * 
 * Hardware: ESP32-S3 + SX1278 (Ra-02) + OLED I2C + 2 buttons + 2 LEDs + buzzer
 */

#include <RadioLib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <SPI.h>

// ===== Node Identity =====
#define NODE_ID 1

// ===== LoRa Pin Definitions (ESP32-S3) =====
#define LORA_NSS 10
#define LORA_RST 9
#define LORA_DIO0 8
#define LORA_MOSI 11
#define LORA_MISO 13
#define LORA_SCK 12

// ===== OLED Pin Definitions (ESP32-S3) =====
#define OLED_SDA 1
#define OLED_SCL 2
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// ===== Button, LED, Buzzer Pins =====
#define BUTTON_SCROLL 0
#define BUTTON_SEND 3
#define LED_RED_PIN 5
#define LED_YELLOW_PIN 6
#define BUZZER 4

enum LedColor { LED_RED_SIGNAL, LED_YELLOW_SIGNAL };

// ===== Mode + Signal Structure =====
const char* modeNames[] = {"RESCUE", "WAREHOUSE", "FIELD_OPS"};
const int numModes = 3;

const char* rescueSignals[]    = {"SOS", "FIRE", "MEDICAL", "TRAPPED", "SAFE", "WATER"};
const char* warehouseSignals[] = {"NEED_HELP", "LOW_STOCK", "DAMAGE", "BREAK_TIME", "TASK_DONE", "EMERGENCY"};
const char* fieldOpsSignals[]  = {"ARRIVED", "DELAYED", "ALL_CLEAR", "NEED_BACKUP", "MOVING", "STANDBY"};
const int numSignalsPerMode = 6;

const char** signalSets[] = {rescueSignals, warehouseSignals, fieldOpsSignals};

// ===== Navigation State =====
enum ScreenState { SCREEN_MODE_SELECT, SCREEN_SIGNAL_SELECT };
ScreenState currentScreen = SCREEN_MODE_SELECT;
int selectedModeIndex = 0;
int selectedSignalIndex = 0;

// ===== SPI + LoRa Instance =====
SPIClass loraSPI(FSPI);
SX1278 lora = new Module(LORA_NSS, LORA_DIO0, LORA_RST, RADIOLIB_NC, loraSPI);

// ===== Wi-Fi AP Settings =====
const char* ap_ssid = "RescueMesh_Dashboard";
const char* ap_password = "rescue123";
WebServer server(80);

// ===== Alert Storage =====
struct Alert {
  int nodeId;
  String mode;
  String signal;
  unsigned long timestamp;
  float rssi;
};
Alert alerts[30];
int alertCount = 0;

// ===== Interrupt flag =====
volatile bool receivedFlag = false;

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  receivedFlag = true;
}

// ===== Button timing tracking =====
unsigned long lastScrollPress = 0;
unsigned long lastSendPress = 0;
unsigned long scrollPressStart = 0;
bool scrollHeld = false;
const unsigned long debounceDelay = 250;
const unsigned long longPressThreshold = 1000;

// ===== Function Prototypes =====
void handleRoot();
void sendSignal(int nodeId, String mode, String signal);
void displayModeSelect();
void displaySignalSelect();
void displayReceivedAlert(int nodeId, String mode, String signal, float rssi);
void flashLED(LedColor color);
void beepBuzzer();

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_SCROLL, INPUT_PULLUP);
  pinMode(BUTTON_SEND, INPUT_PULLUP);

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_YELLOW_PIN, LOW);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Multi-Mode Node"));
  display.println(F("Initializing..."));
  display.display();

  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  Serial.print(F("[LoRa] Initializing... "));
  int state = lora.begin(433.0);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    display.println(F("LoRa: FAILED"));
    display.display();
    for (;;);
  }

  lora.setPacketReceivedAction(setFlag);
  lora.startReceive();

  WiFi.softAP(ap_ssid, ap_password);
  Serial.print(F("[WiFi] AP IP address: "));
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.begin();
  Serial.println(F("[HTTP] Server started"));

  flashLED(LED_YELLOW_SIGNAL);
  displayModeSelect();
}

void loop() {
  unsigned long now = millis();

  bool scrollPinState = digitalRead(BUTTON_SCROLL);

  if (scrollPinState == LOW && !scrollHeld) {
    scrollHeld = true;
    scrollPressStart = now;
  }

  if (scrollPinState == HIGH && scrollHeld) {
    scrollHeld = false;
    unsigned long pressDuration = now - scrollPressStart;

    if (pressDuration < longPressThreshold && (now - lastScrollPress > debounceDelay)) {
      lastScrollPress = now;
      if (currentScreen == SCREEN_MODE_SELECT) {
        selectedModeIndex = (selectedModeIndex + 1) % numModes;
        displayModeSelect();
      } else {
        selectedSignalIndex = (selectedSignalIndex + 1) % numSignalsPerMode;
        displaySignalSelect();
      }
    }
  }

  if (scrollHeld && (now - scrollPressStart > longPressThreshold)) {
    if (currentScreen == SCREEN_SIGNAL_SELECT) {
      currentScreen = SCREEN_MODE_SELECT;
      selectedSignalIndex = 0;
      displayModeSelect();
      scrollHeld = false;
      flashLED(LED_YELLOW_SIGNAL);
    }
  }

  if (digitalRead(BUTTON_SEND) == LOW && (now - lastSendPress > debounceDelay)) {
    lastSendPress = now;

    if (currentScreen == SCREEN_MODE_SELECT) {
      currentScreen = SCREEN_SIGNAL_SELECT;
      selectedSignalIndex = 0;
      displaySignalSelect();
    } else {
      String mode = modeNames[selectedModeIndex];
      String signal = signalSets[selectedModeIndex][selectedSignalIndex];
      sendSignal(NODE_ID, mode, signal);
    }
  }

  if (receivedFlag) {
    receivedFlag = false;

    String receivedData;
    int state = lora.readData(receivedData);

    if (state == RADIOLIB_ERR_NONE) {
      float rssi = lora.getRSSI();

      Serial.print(F("[LoRa] Received: "));
      Serial.println(receivedData);

      int firstColon = receivedData.indexOf(':');
      int secondColon = receivedData.indexOf(':', firstColon + 1);

      if (firstColon > 0 && secondColon > firstColon) {
        int nodeId = receivedData.substring(0, firstColon).toInt();
        String mode = receivedData.substring(firstColon + 1, secondColon);
        String signal = receivedData.substring(secondColon + 1);

        displayReceivedAlert(nodeId, mode, signal, rssi);
        flashLED(LED_YELLOW_SIGNAL);

        if (alertCount < 30) {
          alerts[alertCount].nodeId = nodeId;
          alerts[alertCount].mode = mode;
          alerts[alertCount].signal = signal;
          alerts[alertCount].timestamp = millis();
          alerts[alertCount].rssi = rssi;
          alertCount++;
        }

        delay(2000);
        if (currentScreen == SCREEN_MODE_SELECT) {
          displayModeSelect();
        } else {
          displaySignalSelect();
        }
      }
    } else {
      Serial.print(F("readData failed, code "));
      Serial.println(state);
    }

    lora.startReceive();
  }

  server.handleClient();
}

// ===== Screen: Mode Selection =====
void displayModeSelect() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Node "));
  display.print(NODE_ID);
  display.println(F(" - SELECT MODE"));
  display.println(F("--------------------"));

  display.setTextSize(2);
  display.setCursor(5, 25);
  display.print(F("> "));
  display.println(modeNames[selectedModeIndex]);

  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(F("["));
  display.print(selectedModeIndex + 1);
  display.print(F("/"));
  display.print(numModes);
  display.println(F("] Scroll|Select"));

  display.display();
}

// ===== Screen: Signal Selection =====
void displaySignalSelect() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Node "));
  display.print(NODE_ID);
  display.print(F(" ["));
  display.print(modeNames[selectedModeIndex]);
  display.println(F("]"));
  display.println(F("--------------------"));

  display.setTextSize(2);
  display.setCursor(0, 25);
  display.print(F("> "));
  display.println(signalSets[selectedModeIndex][selectedSignalIndex]);

  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(F("["));
  display.print(selectedSignalIndex + 1);
  display.print(F("/"));
  display.print(numSignalsPerMode);
  display.println(F("] Hold=Back"));

  display.display();
}

// ===== Send Signal via LoRa =====
void sendSignal(int nodeId, String mode, String signal) {
  Serial.print(F("[LoRa] Sending: "));
  Serial.print(nodeId);
  Serial.print(F(":"));
  Serial.print(mode);
  Serial.print(F(":"));
  Serial.println(signal);

  String packet = String(nodeId) + ":" + mode + ":" + signal;
  int state = lora.transmit(packet);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("SENDING..."));
  display.setTextSize(2);
  display.setCursor(0, 25);
  display.println(signal);
  display.display();

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Transmission successful!"));
    flashLED(LED_RED_SIGNAL);
    beepBuzzer();
  } else {
    Serial.print(F("Transmission failed, code "));
    Serial.println(state);
  }

  delay(1000);
  displaySignalSelect();

  lora.startReceive();
}

// ===== Display a Received Alert =====
void displayReceivedAlert(int nodeId, String mode, String signal, float rssi) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print(F("INCOMING ["));
  display.print(mode);
  display.println(F("]"));
  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print(F("Node "));
  display.println(nodeId);
  display.println(signal);
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(F("RSSI: "));
  display.print(rssi);
  display.println(F(" dBm"));
  display.display();
}

// ===== Flash LED =====
void flashLED(LedColor color) {
  if (color == LED_RED_SIGNAL) {
    digitalWrite(LED_RED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_RED_PIN, LOW);
  } else {
    digitalWrite(LED_YELLOW_PIN, HIGH);
    delay(100);
    digitalWrite(LED_YELLOW_PIN, LOW);
  }
}

// ===== Beep Buzzer =====
void beepBuzzer() {
  digitalWrite(BUZZER, HIGH);
  delay(150);
  digitalWrite(BUZZER, LOW);
}

// ===== Web Server Handler =====
void handleRoot() {
  String html = F("<!DOCTYPE html><html><head><title>Multi-Mode Mesh Dashboard</title>");
  html += F("<meta http-equiv='refresh' content='5'>");
  html += F("<style>body{font-family:Arial;margin:20px;} table{border-collapse:collapse;width:100%;}");
  html += F("th,td{border:1px solid #ddd;padding:8px;text-align:left;} th{background-color:#333;color:white;}");
  html += F(".RESCUE{background-color:#ffcccc;} .WAREHOUSE{background-color:#cce5ff;} .FIELD_OPS{background-color:#d9f2d9;}");
  html += F("</style></head><body>");
  html += F("<h1>Multi-Mode Mesh Dashboard</h1>");
  html += F("<h2>Activity Log</h2><table><tr><th>Node ID</th><th>Mode</th><th>Signal</th><th>RSSI</th><th>Time</th></tr>");

  for (int i = 0; i < alertCount; i++) {
    html += F("<tr class='");
    html += alerts[i].mode;
    html += F("'><td>");
    html += alerts[i].nodeId;
    html += F("</td><td>");
    html += alerts[i].mode;
    html += F("</td><td>");
    html += alerts[i].signal;
    html += F("</td><td>");
    html += alerts[i].rssi;
    html += F(" dBm</td><td>");
    html += (millis() - alerts[i].timestamp) / 1000;
    html += F("s ago</td></tr>");
  }

  html += F("</table></body></html>");
  server.send(200, "text/html", html);
}
