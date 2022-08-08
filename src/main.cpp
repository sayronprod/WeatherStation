#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Display
#define TFT_CS D3
#define TFT_RST D0
#define TFT_DC D8

#define SENSOR_TIMEOUT 4000
#define SEND_TIMEOUT 60000

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_BMP280 bmp;

const char *ssid = "Mister Sirion";
const char *password = "agatakristy";

const char *mqtt_server = "mqttserver.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastSensorReadTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastReconnectAttempt = 0;

float lastTemperature = 0.0f;
float lastPressure = 0.0f;

void drawTemperature()
{
  String text = "Temperature " + String(lastTemperature);
  tft.setCursor(30, 50);
  tft.setTextColor(ST77XX_BLUE, ST77XX_BLACK);
  tft.print(text);
}

void drawPressure()
{
  String text = "Pressure " + String(lastPressure);
  tft.setCursor(30, 70);
  tft.setTextColor(ST77XX_BLUE, ST77XX_BLACK);
  tft.print(text);
}

void MQTTcallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message received in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  String message;
  for (unsigned int i = 0; i < length; i++)
  {
    message = message + (char)payload[i];
  }
  Serial.println(message);
}

boolean reconnect()
{
  if (client.connect("ESP8266"))
  {
    client.subscribe("esp/command");
  }
  return client.connected();
}

void setup(void)
{
  Serial.begin(921600);

  if (!bmp.begin(0x76))
  {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                     "try a different address!"));
    while (1)
      delay(10);
  }

  tft.initR(INITR_BLACKTAB);
  tft.setSPISpeed(16000000);
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(3);
  tft.setTextSize(1);

  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_4000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.print("Connected to WiFi :");
  Serial.println(WiFi.SSID());

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(MQTTcallback);
}

void loop()
{
  if (!client.connected())
  {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;
      if (reconnect())
      {
        lastReconnectAttempt = 0;
      }
    }
  }
  else
  {
    client.loop();
  }

  if (millis() - lastSensorReadTime > SENSOR_TIMEOUT)
  {
    bmp.takeForcedMeasurement();
    lastTemperature = bmp.readTemperature();
    lastPressure = bmp.readPressure() / 134.0f;

    lastSensorReadTime = millis();

    drawTemperature();
    drawPressure();
  }

  if (millis() - lastSendTime > SEND_TIMEOUT && client.connected())
  {
    String message = "{\"Temperature\":" + String(lastTemperature) + ",\"Pressure\":" + String(lastPressure) + "}";
    client.publish("esp/weather", message.c_str(), true);

    lastSendTime = millis();
  }
}
