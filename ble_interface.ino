/*
 *  This sketch demonstrates how to scan WiFi networks.
 *  The API is almost the same as with the WiFi Shield library,
 *  the most obvious difference being the different file you need to include:
 */
#include "WiFi.h"
#include <ArduinoJson.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <U8x8lib.h>

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

char* ssid;
char* wifiPassword;

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);


void findAvailableWifiNetworksAndSendToBLE()
{
    
//  need about 90 per SSID
    StaticJsonDocument<500> doc;
    doc["payload"] = "WIFI_NETWORKS";

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    u8x8.print("WIFI Setup done");
    Serial.println("WIFI Setup done");

    int n = WiFi.scanNetworks();
    u8x8.print("found ");
    u8x8.print(n);
    u8x8.print(" networks/n");

    JsonArray networks = doc.createNestedArray("networks");

    if (n == 0) {
        Serial.println("no networks found");
        u8x8.println("no networks found");
    } else {
        for (int i = 0; i < n && i < 5; ++i) {
          JsonObject network = networks.createNestedObject();
          network["SSID"] = WiFi.SSID(i);
          network["RSSI"] = WiFi.RSSI(i);
          network["auth"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?"OPEN":"CLOSED";
        }
    }
    u8x8.println("sending networks");
    std::string out;
    serializeJson(doc, out);
    pTxCharacteristic->setValue(out);
    pTxCharacteristic->notify();
    
//    serializeJsonPretty(doc, Serial);
//    TODO: write a Custom Writer to stream serializeJSON back over BLE: https://arduinojson.org/v6/api/json/serializejson/
}

void sendWifiStatusToBLE(){
 
  StaticJsonDocument<200> doc;

  int status = WiFi.status();
  doc["payload"] = "WIFI_STATUS";

  if(status == WL_CONNECTED){
    doc["status"] = "CONNECTED";
    doc["ssid"] = ssid;
  }else{
    doc["status"] = "NOT_CONNECTED";  
  }

  u8x8.println("sending status");
  std::string out;
  serializeJson(doc, out);
  pTxCharacteristic->setValue(out);
  pTxCharacteristic->notify();
  
}

void connectToWifiAndSendStatusToBLE(){
  WiFi.begin(ssid, wifiPassword);
  int status = WiFi.status(); 
  while(status != WL_CONNECTED) {
    delay(100);
    status = WiFi.status();  
  }
  sendWifiStatusToBLE();
}


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

    
      StaticJsonDocument<2000> doc;

      DeserializationError error = deserializeJson(doc, rxValue);

      // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }

      const char* command = doc["command"];

      serializeJsonPretty(doc, Serial);
      Serial.print("Received Command: ");
      Serial.println(command);
      u8x8.clear();
      u8x8.print("CMD: ");
      u8x8.println(command);

      if(strcmp(command, "WIFI_SEARCH") == 0){
        u8x8.println("SEARCHING");
        findAvailableWifiNetworksAndSendToBLE();
      }

      if(strcmp(command, "SAY_HI") == 0){
        u8x8.println("SAYING HI");
        pTxCharacteristic->setValue("HI");
        pTxCharacteristic->notify();
      }

      if(strcmp(command, "WIFI_STATUS") == 0){
        sendWifiStatusToBLE();
      }

      if(strcmp(command, "WIFI_CONNECT") == 0){
        ssid = strdup(doc["SSID"]);
        wifiPassword = strdup(doc["password"]);
        u8x8.println(ssid);
        u8x8.println(wifiPassword);
        Serial.println(ssid);
        Serial.println(wifiPassword);
        connectToWifiAndSendStatusToBLE();
      }
      
    }
};



void setup()
{
    Serial.begin(115200);
    u8x8.begin();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.clear();

    // Create the BLE Device
    BLEDevice::init("Heltec OTA Experiment 2");
  
    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
  
    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);
  
    // Create a BLE Characteristic
    pTxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                        
    pTxCharacteristic->addDescriptor(new BLE2902());
  
    BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                         CHARACTERISTIC_UUID_RX,
                        BLECharacteristic::PROPERTY_WRITE
                      );
  
    pRxCharacteristic->setCallbacks(new MyCallbacks());
  
    // Start the service
    pService->start();
  
    // Start advertising
    pServer->getAdvertising()->start();
    Serial.println("Waiting a client connection to notify...");
    
}

void loop()
{

//  For testing BLE Tx
//     if (deviceConnected) {
//        pTxCharacteristic->setValue(&txValue, 1);
//        pTxCharacteristic->notify();
//        txValue++;
//        delay(10); // bluetooth stack will go into congestion, if too many packets are sent
//    }

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }

}


