#include "src/ArduinoJson/ArduinoJson.h"
#include "src/pubsubclient/src/PubSubClient.h"
#include "src/Adafruit-Fingerprint/Adafruit_Fingerprint.h"
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <ArduinoOTA.h>

// Wifi Settings
#define SSID                          "**YOUR SSID HERE**"
#define PASSWORD                      "**YOUR WI-FI PASSWORD HERE"
//Static IP setup
IPAddress local_IP(192, 168, 0, 150);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); 

// MQTT Settings
#define HOSTNAME                      "fingerprint"
#define MQTT_SERVER                   "192.168.0.125"					  //IP of MQTT server												
#define STATE_TOPIC                   "fingerprint_sensor/1/state"        //Subscribe here to receive state updates
#define REQUEST_TOPIC                 "fingerprint_sensor/1/request"      //Publish here to make learn and delete requests
#define REPLY_TOPIC                   "fingerprint_sensor/1/reply"        //Publish here to display your action on OLED
#define AVAILABILITY_TOPIC            "fingerprint_sensor/1/available"
#define mqtt_username                 "**YOUR MQTT USERNAME**"
#define mqtt_password                 "**YOUR MQTT PASSWORD**"

// Fingerprint Sensor
#define SENSOR_TX 13                  //GPIO Pin for WEMOS RX, SENSOR TX
#define SENSOR_RX 12                  //GPIO Pin for WEMOS TX, SENSOR RX
#define TOUCH_PIN 14				  //GPIO Pin to detect placed finger
#define learnTimeout 10000            //10s learn timeout

SoftwareSerial mySerial(SENSOR_TX, SENSOR_RX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

//Buzzer
#define buzzerRelayPin  4             //GPIO Pin pro rele D2
#define buzzerInterval  2000          //Time in ms for opening door

WiFiClient wifiClient;                // Initiate WiFi library
PubSubClient client(wifiClient);      // Initiate PubSubClient library

int id = 0;                       //Stores the current fingerprint ID
int lastID = 0;                   //Stores the last matched ID
int lastConfidenceScore = 0;      //Stores the last matched confidence score

int i;                                // misc for loop use
int reconnectCounter = 0;

//Declare JSON variables
DynamicJsonDocument mqttMessage(200);
char mqttBuffer[200];

void setup(){

  Serial.begin(57600);

  pinMode(14, INPUT); // Connect D5 to T-Out (pin 5 on reader), T-3v to 3v
  pinMode(buzzerRelayPin, OUTPUT);
  digitalWrite(buzzerRelayPin, LOW);

  Serial.println("Looking for sensor...");
  
  finger.begin(57600);
  delay(5);
  while (!finger.verifyPassword()) { // Try to connect to sensor
    Serial.println("Sensor not found!");
    delay(2000);
  }
  Serial.println("Sensor Found!");

  // prep wifi
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet, primaryDNS);
  WiFi.begin(SSID, PASSWORD);
  Serial.println("Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) {       // Wait till Wifi connected
    delay(1000);
    Serial.println(WiFi.status());
  }
  Serial.print("IP is ");
  Serial.println(WiFi.localIP());
  delay(500);

  // connect to mqtt server
  Serial.println("Connecting MQTT...");
  client.setServer(MQTT_SERVER, 1883);                // Set MQTT server and port number
  client.setCallback(callback);
  while (!client.connected()) {       // Loop until connected to MQTT server
    reconnect(); 					  //Connect to MQTT server
	delay(1000);
  }
  Serial.println("Connected");
  // publish initial state
  mqttMessage["state"] = "Idle";
  mqttMessage["id"] = 0;
  mqttMessage["success"] = false;
  mqttMessage["Wi-Fi signal"] = WiFi.RSSI();
  size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  delay(500);

  // enable OTA updates
  ArduinoOTA.setHostname("Fingerprint");
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  // done
  Serial.println("BOOT SUCCESSFUL!");
  
  delay(1000);
}




void loop() {

  if (digitalRead(TOUCH_PIN) == HIGH) { // Read T-Out, normally HIGH (when no finger)
    finger.CloseLED();
  } else {
    Serial.println("SCANNING");
    delay(250);            //give finger time to land and settle
    uint8_t result = getFingerprintID();
    if (result == FINGERPRINT_OK) {
	  if (client.connected()) {
		mqttMessage["state"] = "Matched";
	    mqttMessage["id"] = lastID;
	    size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
	    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);               
	  }
      finger.CloseLED();
      digitalWrite(buzzerRelayPin, HIGH);
      delay(buzzerInterval);
      digitalWrite(buzzerRelayPin, LOW);
    } else if (result == FINGERPRINT_NOTFOUND) {
	  if (client.connected()) {
		mqttMessage["state"] = "Not matched";
        mqttMessage["id"] = 0;
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);            
	  }
      blinkLEDFast();
    } else {
      // do nothing
    }
	if (client.connected()) {
	  mqttMessage["state"] = "Idle";
      mqttMessage["id"] = 0;
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);          
	}
  }

  if (client.connected()) {
	client.loop();
	client.publish(AVAILABILITY_TOPIC, "Online");
	reconnectCounter = 0;
  } else {
	reconnectCounter++;
	if (reconnectCounter == 1) {
	  reconnect();
	} else if (reconnectCounter > 25) {
	  reconnectCounter = 0;
    }	  
  }

  ArduinoOTA.handle();
  delay(200);            //don't need to run this at full speed.
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("IMAGED");
      break;
    default:
      lastID = 0;
      Serial.println("TRY AGAIN");
      return p;
  }
  // image taken
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("PROCESSED");
      break;
    default:
      lastID = 0;
      int i;
      Serial.println("TRY AGAIN");
      return p;
  }
  // converted
  p = finger.fingerFastSearch();
  switch (p) {
    case FINGERPRINT_OK:
      lastID = finger.fingerID;
      lastConfidenceScore = finger.confidence;
      Serial.println("MATCHED");
      Serial.print("ID #");
      Serial.println(finger.fingerID);
      Serial.print("CONFIDENCE ");
      Serial.println(finger.confidence);
      delay(100);
      return p;
    case FINGERPRINT_NOTFOUND:
      lastID = 0;
      Serial.println("NO MATCH");
      return p;
    default:
      lastID = 0;
      Serial.println("TRY AGAIN");
      return p;
  }
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  Serial.println("PLACE FINGER...");
  delay(250);
  long startTime = millis();
  // start routine
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("IMAGED");
        delay(250);
        break;
      case FINGERPRINT_NOFINGER:
        // finger hasn't been placed yet, do nothing
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        // communication error, do nothing
        break;
      case FINGERPRINT_IMAGEFAIL:
        // imaging error, do nothing
        break;
      default:
        // unknown error, do nothing
        break;
    }
	if (millis()-startTime > learnTimeout) {
      Serial.println("LEARN TIMEOUT");
      return true;
    }
  }
  //
  // got fingerprint_ok, continue
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("CONVERTED");
      blinkLEDSlow();
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("IMAGE MESSY");
      blinkLEDFast();
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("COMM ERROR");
      blinkLEDFast();
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("BAD IMAGE");
      blinkLEDFast();
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("BAD IMAGE");
      blinkLEDFast();
      return p;
    default:
      Serial.println("UNKNOWN ERROR");
      blinkLEDFast();
      return p;
  }
    
  Serial.println("REMOVE FINGER");
  
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  p = -1;

  Serial.println("PLACE SAME FINGER");
  
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("IMAGED");
        delay(250);
        break;
      case FINGERPRINT_NOFINGER:
        //Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        //Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        //Serial.println("Imaging error");
        break;
      default:
        //Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("CONVERTED");
      delay(250);
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("IMAGE MESSY");
      blinkLEDFast();
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("COMM ERROR");
      blinkLEDFast();
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("BAD IMAGE");
      blinkLEDFast();
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("BAD IMAGE");
      blinkLEDFast();
      return p;
    default:
      Serial.println("UNKNOWN ERROR");
      blinkLEDFast();
      return p;
  }

  // OK converted!
  Serial.println("CREATING MODEL");
  delay(250);
  
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("PRINTS MATCH");
    delay(250);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("COMM ERROR");
    blinkLEDFast();
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("PRINTS DONT MATCH");
    blinkLEDFast();
    return p;
  } else {
    Serial.println("UNKNOWN ERROR");
    blinkLEDFast();
    return p;
  }
  delay(250);
  
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("SUCCESS");
    blinkLEDSlow();
    mqttMessage["success"] = true;
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("COMM ERROR");
    blinkLEDFast();
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("LOCATION ERROR");
    blinkLEDFast();
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("FLASH ERROR");
    blinkLEDFast();
    return p;
  } else {
    Serial.println("UNKNOWN ERROR");
    blinkLEDFast();
    return p;
  }
}


uint8_t deleteFingerprint() {
  uint8_t p = -1;

  p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("DELETED");
    mqttMessage["success"] = true;
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("PACKET ERROR");
    mqttMessage["success"] = false;
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("BAD LOCATION");
    mqttMessage["success"] = false;
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("FLASH ERROR");
    mqttMessage["success"] = false;
    return p;
  } else {
    Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
    mqttMessage["success"] = false;
    return p;
  }
}

void blinkLEDFast() {
  for (int j=0; j<5; j++) {
    finger.CloseLED();
    delay(200);
    finger.OpenLED();
    delay(200);
  }
}

void blinkLEDSlow() {
  finger.CloseLED();
  delay(500);
  finger.OpenLED();
  delay(1000);
  finger.CloseLED();
  delay(500);
  finger.OpenLED();
}

void reconnect() {
  Serial.print("Reconnection routine, MQTT = "); Serial.print(client.state()); Serial.print(", Wi-Fi = "); Serial.println(WiFi.status());
  
  if (WiFi.status() != WL_CONNECTED) {       // If disconnected from Wi-Fi
    WiFi.reconnect();
  }
  
  if (client.connect(HOSTNAME, mqtt_username, mqtt_password, AVAILABILITY_TOPIC, 1, true, "Offline")) {       //Connect to MQTT server
    client.publish(AVAILABILITY_TOPIC, "Online");         // Once connected, publish online to the availability topic
    client.subscribe(REQUEST_TOPIC);
    Serial.println("Reconnect success!!!");
  } else {
    Serial.print("Reconnect unsuccessful, code "); Serial.println(client.state());
  }

}

void callback(char* topic, byte* payload, unsigned int length) {          //The MQTT callback which listens for incoming messages on the subscribed topics
  
  //check incoming topic
  if (strcmp(topic, REQUEST_TOPIC) == 0){
    // first convert payload to char array
    char payloadChar[length+1];
    for (int i = 0; i < length; i++) {
      payloadChar[i] = payload[i];
    }
    // second deserialize json payload and save variables
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payloadChar);
    const char* requestValue = doc["request"];
    id = atoi(doc["id"]);
    //if learning...
    if (strcmp(requestValue, "learn") == 0) {
      Serial.println("LEARNING MODE ACTIVE");
      delay(250);
      if (id > 0 && id < 128) {
        Serial.print("ID SELECTED: ");
        Serial.println(id);
        delay(250);
        // MQTT
        mqttMessage["state"] = "Learning";
        mqttMessage["id"] = id;
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        while (!getFingerprintEnroll());
        if (!client.connected()) {
          reconnect();                //Just incase we get disconnected from MQTT server
        }
        mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        Serial.println("EXITING LEARN MODE");
        delay(3000);
        id = 0;
      } else {
        Serial.println("INVALID ID");
        id = 0;
      }
    }
    // if deleting...
    if (strcmp(requestValue, "delete") == 0) {
      Serial.println("DELETE MODE ACTIVE");
      delay(250);
      if (id > 0 && id < 128) {
        Serial.print("ID SELECTED: ");
        Serial.println(id);
        delay(250);
        mqttMessage["state"] = "Deleting";
        mqttMessage["id"] = id;
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        while (! deleteFingerprint());
        mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        Serial.println("EXITING DELETE MODE");
        delay(3000);
        id = 0;
      }
    }
    // if opening from HA...
    if (strcmp(requestValue, "open") == 0) {
      Serial.println("Opening from HA");
      mqttMessage["state"] = "Opening";
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      digitalWrite(buzzerRelayPin, HIGH);
      delay(buzzerInterval);
      digitalWrite(buzzerRelayPin, LOW);
    }
  }
  // display the reply from the server
  if (strcmp(topic, REPLY_TOPIC) == 0){
    // first convert payload to char array
    char payloadChar[length+1];
    for (int i = 0; i < length; i++) {
      payloadChar[i] = payload[i];
    }
    // second deserialize json payload and save variables
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payloadChar);
    const char* line1Val = doc["line1"];
    const char* line2Val = doc["line2"];
    //then display it
    Serial.println(line1Val);
    Serial.println(line2Val);
    delay(1000);
  }
  
  // wrap up
  mqttMessage["state"] = "Idle";
  mqttMessage["id"] = 0;
  mqttMessage["success"] = false;
  mqttMessage["Wi-Fi signal"] = WiFi.RSSI();										
  size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);  
}
