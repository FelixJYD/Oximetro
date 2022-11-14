#define __DEBUG__
 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DFRobot_MAX30102.h>

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>

#define WIFI_SSID "tesalia"
#define WIFI_PASSWORD "admin2022&"

#define MQTT_HOST IPAddress(192, 168, 1, 150)
#define MQTT_PORT 1883

#define MQTT_PUB_OX "oximetro/sensor/oxigenacion"
#define MQTT_PUB_RC "oximetro/sensor/ritmocardiaco"

float movimiento;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

unsigned long previousMillis = 0; 
const long interval = 10000;

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); 
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}


DFRobot_MAX30102 particleSensor;

/*
Macro definition options in sensor configuration
sampleAverage: SAMPLEAVG_1 SAMPLEAVG_2 SAMPLEAVG_4
               SAMPLEAVG_8 SAMPLEAVG_16 SAMPLEAVG_32
ledMode:       MODE_REDONLY  MODE_RED_IR  MODE_MULTILED
sampleRate:    PULSEWIDTH_69 PULSEWIDTH_118 PULSEWIDTH_215 PULSEWIDTH_411
pulseWidth:    SAMPLERATE_50 SAMPLERATE_100 SAMPLERATE_200 SAMPLERATE_400
               SAMPLERATE_800 SAMPLERATE_1000 SAMPLERATE_1600 SAMPLERATE_3200
adcRange:      ADCRANGE_2048 ADCRANGE_4096 ADCRANGE_8192 ADCRANGE_16384
*/
 
// Definir constantes
#define ANCHO_PANTALLA 128 // ancho pantalla OLED
#define ALTO_PANTALLA 64 // alto pantalla OLED
 
// Objeto de la clase Adafruit_SSD1306
Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);
 
void setup() {
  Serial.begin(115200);
  Serial.println();

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  
 mqttClient.onConnect(onMqttConnect);
 mqttClient.onDisconnect(onMqttDisconnect);
 mqttClient.onPublish(onMqttPublish);
 mqttClient.setServer(MQTT_HOST, MQTT_PORT);
 connectToWifi();

  #ifdef __DEBUG__
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("Iniciando pantalla OLED");
  #endif

  while (!particleSensor.begin()) {
    Serial.println("MAX30102 was not found");
    delay(1000);
  }
  
  particleSensor.sensorConfiguration(/*ledBrightness=*/50, /*sampleAverage=*/SAMPLEAVG_4, \
                        /*ledMode=*/MODE_MULTILED, /*sampleRate=*/SAMPLERATE_100, \
                        /*pulseWidth=*/PULSEWIDTH_411, /*adcRange=*/ADCRANGE_16384);
            
  
  // Iniciar pantalla OLED en la dirección 0x3C
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    #ifdef __DEBUG__
      Serial.println("No se encuentra la pantalla OLED");
    #endif
    while (true);
  }

  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  display.setCursor(0,0);
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.print("SPO2: ");
  display.setCursor(0,16);
  display.setTextSize(3); 
  display.print("99 %");
  display.display();
 
}
 
int32_t SPO2; //SPO2
int8_t SPO2Valid; //Flag to display if SPO2 calculation is valid
int32_t heartRate; //Heart-rate
int8_t heartRateValid; //Flag to display if heart-rate calculation is valid 

void loop()
{
  Serial.println(F("Wait about four seconds"));
  particleSensor.heartrateAndOxygenSaturation(/**SPO2=*/&SPO2, /**SPO2Valid=*/&SPO2Valid, /**heartRate=*/&heartRate, /**heartRateValid=*/&heartRateValid);

  //Print result 

  uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_RC, 1, true, String(heartRate).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i", MQTT_PUB_RC, packetIdPub1);
    Serial.print(F("heartRate="));
    Serial.print(heartRate, DEC);
    Serial.print(F(", heartRateValid="));
    Serial.print(heartRateValid, DEC);
 uint16_t packetIdPub2 = mqttClient.publish(MQTT_PUB_OX, 1, true, String(SPO2).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i", MQTT_PUB_OX, packetIdPub1);
    Serial.print(F("; SPO2="));
    Serial.print(SPO2, DEC);
    Serial.print(F(", SPO2Valid="));
    Serial.println(SPO2Valid, DEC);

  display.clearDisplay();

  display.setCursor(0,0);
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.print("SPO2: ");
  display.setCursor(0,16);
  display.setTextSize(3); 
  display.print(SPO2);
  display.print(" %");
  display.display();
  
}