#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// const char* WIFI_SSID = "ㅤ";
// const char* password = "113333555555";

const char* WIFI_SSID = "AFN_AquaSync";
const char* password = "Be_Smart";

//Database Secret's
#define API_KEY      "AIzaSyDMYlfnmK92oIdOqsFbwJIio_kOoAMeWJA"
#define DATABASE_URL "https://smartedge-iot-app-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData stream;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String listenerPath = "AFN_AquaSync/Irrigating";

//Pin Definition
const int Light_rain_indicator = 0;
const int Heavy_rain_indicator = 2;
const int WaterPump            = 3;  //RX

int moistureLevel = 11;

//Status Checker
unsigned long lastPacketTime      = 0;
unsigned long lastConnectionTime  = 0;
unsigned long lastSentDataTime    = 0;
unsigned long lastMoistIncresed   = 0;

struct struct_message 
{
  float temperature;
  float humidity;
  uint8_t soilMoisture;
};

struct_message myData;

void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) 
{
  Serial.println("Entered the ESP-NOW callback");
  memcpy(&myData, incomingData, sizeof(myData));
  lastPacketTime = millis();
}

int32_t getWiFiChannel(const char *ssid) 
{
  if (int32_t n = WiFi.scanNetworks()) 
  {
    for (uint8_t i = 0; i < n; i++) 
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
  Serial.begin(115200);
  Serial.println("Device Started");

  // Initialize the pins as inputs
  pinMode(Heavy_rain_indicator, OUTPUT);
  pinMode(Light_rain_indicator , OUTPUT);
  pinMode(WaterPump, OUTPUT);

  digitalWrite(Heavy_rain_indicator, HIGH);
  digitalWrite(Light_rain_indicator , HIGH);
  digitalWrite(WaterPump, LOW);

  WiFi.mode(WIFI_AP_STA);

  //Get and set the WiFi channel
  int32_t channel = getWiFiChannel(WIFI_SSID);
  wifi_set_channel(channel);

  WiFi.begin(WIFI_SSID, password, channel);

  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
    // digitalWrite(Quarter, HIGH);
  }
  Serial.println(WiFi.localIP());
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    // digitalWrite(Quarter, LOW);
  }

  if (esp_now_init() != 0) 
  {
    //Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onDataRecv);
  
  //Detect double reset
  // if (drd.detectDoubleReset())
  // {
  //     WiFiManager wm;
  //     wm.resetSettings();

  //     if (wm.autoConnect("AFN DRD AquaSmart"))
  //     {
  //       drd.stop();
  //     }
  // }

  // else
  // {
  //   WiFiManager wm;
  //   wm.autoConnect("AFN AquaSmart");
  // }

  config.api_key      = API_KEY;
  config.database_url = DATABASE_URL;

  // Firebase.signUp(&config, &auth, "admin@aquafarmnexus.com", "admin");
  auth.user.email = "admin@aquafarmnexus.com";
  auth.user.password = "afn-admin";

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(false);

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  //Firebase Stream
 if (!Firebase.RTDB.beginStream(&stream, listenerPath.c_str()))
 {
  Serial.printf("stream begin error, %s\n\n", stream.errorReason().c_str());
 }

  // Assign a calback function to run when it detects changes on the database
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
}

// Callback function that runs on database changes
void streamCallback(FirebaseStream data) {
  Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
  data.streamPath().c_str(),
  data.dataPath().c_str(),
  data.dataType().c_str(),
  data.eventType().c_str());
  printResult(data); //see addons/RTDBHelper.h
  Serial.println();

  // Get the path that triggered the function
  String streamPath = String(data.dataPath());

  // if the data returned is an integer, there was a change on the GPIO state on the following path /{gpio_number}
  if (data.dataTypeEnum() == fb_esp_rtdb_data_type_integer) {
    String gpio = streamPath;
    int state = data.intData();
    Serial.print("GPIO: ");
    Serial.println(gpio);
    Serial.print("STATE: ");
    Serial.println(state);
    // digitalWrite(gpio.toInt(), state);
    if (gpio == "/") {
      digitalWrite(WaterPump, state);
    }
  }

  /* When it first runs, it is triggered on the root (/) path and returns a JSON with all keys
  and values of that path. So, we can get all values from the database and updated the GPIO states*/
  if (data.dataTypeEnum() == fb_esp_rtdb_data_type_json) {
    FirebaseJson json = data.to<FirebaseJson>();

    // To iterate all values in Json object
    size_t count = json.iteratorBegin();
    for (size_t i = 0; i < count; i++) {
      if (i == 2) {
        continue;
      }
      FirebaseJson::IteratorValue value = json.valueAt(i);
      String gpio = value.key;
      int state = value.value.toInt();
      // digitalWrite(gpio, state);
      if (gpio == "/") {
      digitalWrite(WaterPump, state);
      }
    }
    json.iteratorEnd();  // required for free the used memory in iteration (node data collection)
  }
}

void streamTimeoutCallback(bool timeout) 
{
  if (timeout) {
  }
  if (!stream.httpConnected()) {
  }
}

void tokenRefresher() 
{
  if (Firebase.isTokenExpired()) 
  {
    Firebase.refreshToken(&config);
    Serial.println("Refresh token");
  }
}

void sendInt(int value)
{
  if (Firebase.RTDB.setInt(&fbdo, "AFN_AquaSync/Moisture_Level", value))
  {
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
    
  else 
  {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}

void sendFloat(float value, String path)
{
  // Write an Float number on the database path test/float
    if (Firebase.RTDB.setFloat(&fbdo, "AFN_AquaSync/" + path, value))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }

    else 
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
}

void loop() {
  //Serial.println("Entered the loop part.");
  tokenRefresher();

  if (Firebase.ready() && millis() - lastSentDataTime > 3000 || lastSentDataTime == 0) 
  {
    lastSentDataTime = millis();
    sendInt(moistureLevel);
    sendFloat(myData.temperature, "Temperature");
    sendFloat(myData.humidity, "Humidity");
  }

  if (millis() - lastPacketTime < 2000) {
    Serial.println("Esp_now Connected receiving data");
    // Print the pin states to the //Serial Monitor

    if (myData.soilMoisture == 1) {
      // digitalWrite(Light_rain_indicator , LOW);
      Serial.println("Soil moisture content is low");
    }

    if (myData.soilMoisture == 0) {
      digitalWrite(WaterPump, HIGH);
      if (millis() - lastMoistIncresed > 1000)
      {
        lastMoistIncresed = millis();
        moistureLevel += 10;
      }
      Serial.println(moistureLevel);
      Serial.println("Soil moisture content is high");
    }
  }

  else 
  {
    unsigned long prevTime = 0;
    if (millis() - prevTime > 2000)
    {
      Serial.println("Esp_now Not-Connected");
    }
  }
}