#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// WiFi Credentials
const char* ssid = "Mr.Perfect";
const char* password = "SRINI0624";

// Supabase API Configuration
const char* supabaseUrl = "https://tbedstpoanoiovyexfac.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InRiZWRzdHBvYW5vaW92eWV4ZmFjIiwicm9sZSI6ImFub24iLCJpYXQiOjE3Mzk2ODc3NDMsImV4cCI6MjA1NTI2Mzc0M30.r1yVr9QzNUn9VYLnb7OgtTUwRMt5hMuX3sUQYh9UZyw";
const char* sensorDataEndpoint = "/rest/v1/sensor_data";
const char* controlSettingsEndpoint = "/rest/v1/control_settings";

// DHT Sensor Setup
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Relay Pin
#define RELAY_PIN 2

float temp_limit = 30.0;  // Default temperature limit
bool relay_status = false;
bool relay_manual = false;
bool relay_override = false;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  pinMode(RELAY_PIN, OUTPUT);
  dht.begin();

  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int airQuality = analogRead(15);

    Serial.print("Temp: "); Serial.print(temperature);
    Serial.print(" °C, Humidity: "); Serial.print(humidity);
    Serial.print(" %, Air Quality: "); Serial.println(airQuality);

    // Fetch control settings
    getControlSettings();

    // Determine relay status
    if (relay_manual) {
      // Manual mode - use override setting
      relay_status = relay_override;
    } else {
      // Automatic mode - use temperature limit
      relay_status = temperature > temp_limit;
    }

    digitalWrite(RELAY_PIN, relay_status ? HIGH : LOW);

    // Send sensor data to Supabase
    sendToSupabase(temperature, humidity, airQuality, relay_status);
  } else {
    Serial.println("WiFi Disconnected");
    WiFi.begin(ssid, password);
  }

  delay(5000);
}

void sendToSupabase(float temperature, float humidity, int airQuality, bool relayStatus) {
  HTTPClient http;
  
  String url = String(supabaseUrl) + String(sensorDataEndpoint);
  http.begin(url);
  
  // Set headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", "Bearer " + String(supabaseKey));
  
  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["air_quality"] = airQuality;
  doc["relay_status"] = relayStatus;

  String jsonData;
  serializeJson(doc, jsonData);
  
  int httpResponseCode = http.POST(jsonData);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Data sent successfully");
  } else {
    Serial.print("Error sending data. Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}

void getControlSettings() {
  HTTPClient http;
  
  String url = String(supabaseUrl) + String(controlSettingsEndpoint) + "?select=temp_limit,relay_manual,relay_override&order=created_at.desc&limit=1";
  http.begin(url);
  
  // Set headers
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", "Bearer " + String(supabaseKey));
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    String payload = http.getString();
    
    // Parse JSON array (Supabase returns an array)
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);
    
    // Get first (and only) object from array
    JsonObject obj = doc[0];
    
    temp_limit = obj["temp_limit"].as<float>();
    relay_manual = obj["relay_manual"].as<bool>();
    relay_override = obj["relay_override"].as<bool>();
    
    Serial.println("Control settings updated");
  } else {
    Serial.print("Error getting control settings. Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}