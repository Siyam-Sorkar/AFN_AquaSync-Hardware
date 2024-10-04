#include <ESP8266WiFi.h>
#include <espnow.h>
#include <DHT.h>

#define DHTPIN 2      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11 // DHT 11

DHT dht(DHTPIN, DHTTYPE);

//Pin declarations
int dhtVcc  = 1;
int dhtGnd  = 0;
int moistSensPin = 3;

// MAC Address of the Slave module
uint8_t receiverMAC[] =  {0x48, 0x55, 0x19, 0xC8, 0x5A, 0x4C};

//New ESP-01s
// uint8_t receiverMAC[] =  {0xAC, 0x0B, 0xFB, 0xDB, 0x9B, 0xDA};

constexpr char WIFI_SSID[] = "AFN_AquaSync";
// constexpr char WIFI_SSID[] = "ㅤ";

// Structure to send the state of the pins
struct struct_message 
{ 
  float temperature;
  float humidity;
  uint8_t soilMoisture;
};

struct_message myData;

//Function to get the WiFi channel
int32_t getWiFiChannel(const char *ssid) 
{
  if (int32_t n = WiFi.scanNetworks()) 
  {
      for (uint8_t i=0; i<n; i++) 
      {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) 
          {
            return WiFi.channel(i);
          }
      }
  }
  return 0;
}


void setup() 
{
  // Initialize the pins as inputs
  pinMode(dhtVcc,  OUTPUT);
  pinMode(dhtGnd,  OUTPUT);
  pinMode(moistSensPin, INPUT);

  digitalWrite(dhtVcc, HIGH);
  digitalWrite(dhtGnd, LOW);
  
  WiFi.mode(WIFI_STA);

  //Get and set the WiFi channel
  int32_t channel = getWiFiChannel(WIFI_SSID);
  wifi_set_channel(channel);

  if (esp_now_init() != 0) 
  {
    //Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(receiverMAC, ESP_NOW_ROLE_SLAVE, channel, NULL, 0);

  dht.begin();
}

void loop() 
{
  // Read the sensor values
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity(); 

  myData.temperature  = temperature;
  myData.humidity     = humidity;
  myData.soilMoisture = digitalRead(moistSensPin);

  // Send the struct with the pin states
  esp_now_send(receiverMAC, (uint8_t *)&myData, sizeof(myData));


  delay(1000);
}
