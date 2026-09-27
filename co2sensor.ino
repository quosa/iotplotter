#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <HardwareSerial.h>
#include <HTTPClient.h>
#include <WiFi.h>


#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;
HardwareSerial MySerial(1);

uint8_t cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
uint8_t res[9] = {};
uint8_t idx = 0;
bool flag = false;
uint16_t co2 = 0;

float temperature = 0;
float pressure = 0;
float humidity = 0;

const char* ssid     = "WIFI_NETWORK_NAME";
const char* password = "WIFI_PASSWORD";


void setup() {
  bool status;

  Serial.begin(115200); // towards computer
  MySerial.begin(9600, SERIAL_8N1, 16, 17); // sensor
  delay(100);

  status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  delay(4000);

  Serial.print("Connecting to WiFi..");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { //Check for the connection
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to the WiFi network ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void loop() {
  printMHValues();
  printBMEValues();
  sendResults();
  delay(60 * 1000);
}

void printMHValues() {
  // Serial.println("starting");
  MySerial.write(cmd, 9);
  // Serial.println("read command sent");

  while (MySerial.available() > 0)
  {
    res[idx++] = MySerial.read();
    // Serial.print(res[idx -1], HEX);
    flag = true;
  }
  idx = 0;
  if (flag)
  {
    flag = false;
    co2 = 0;
    co2 += (uint16_t)res[2] << 8;
    co2 += res[3];
  }
  // rudimentary sanity check for after reboot
  if (co2 < 0 || co2 > 10000) {
    Serial.print("OUT-OF-BOUNDS CO2 VALUE ");
    Serial.print(co2);
    Serial.println(" - SETTING TO 0");
    co2 = 0;
  }
  // Serial.println("");
  Serial.print("CO2 = ");
  Serial.print(co2);
  Serial.println(" ppm");
}

void printBMEValues() {
  temperature = bme.readTemperature();
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" *C");

  // Convert temperature to Fahrenheit
  /*Serial.print("Temperature = ");
    Serial.print(1.8 * bme.readTemperature() + 32);
    Serial.println(" *F");*/

  pressure = bme.readPressure() / 100.0F;
  Serial.print("Pressure = ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  humidity = bme.readHumidity();
  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.println();
}

void sendResults() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error in WiFi connection");
    return;
  }

  HTTPClient http;

  http.begin("http://iotplotter.com/api/v2/feed/157026536425251150");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("api-key", "8c29e24f9ef569f609790bf8ed6de47a13c9756020");

  String post_json = "{\"data\":{\"IndoorAirQuality\": [{\"value\":";
  post_json += String(co2);
  post_json += "}],\"IndoorTemperature\": [{\"value\":";
  post_json += String(temperature);
  post_json += "}],\"IndoorAirPressure\": [{\"value\":";
  post_json += String(pressure);
  post_json += "}],\"IndoorHumidity\": [{\"value\":";
  post_json += String(humidity);
  post_json += "}]}}";
  Serial.println(post_json);
  int httpResponseCode = http.POST(post_json);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
    if (  WiFi.status() != WL_CONNECTED) {
      Serial.println("Lost WiFi conectivity - rebooting in 5s...");
      delay(5000);
      ESP.restart();
    }
  }
  http.end();
}
