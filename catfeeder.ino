
#define _ASYNC_MQTT_LOGLEVEL_               1

#define WIFI_SSID         "YOUR WIFI SSD"
#define WIFI_PASSWORD     "YOUR WIFI PASSWORD"
#define RFID_SAAR         "XXXXXXXX03012001"  
#define RFID_PUCK         "XXXXXXXX03012001"
#define BARCODE_HOME      "KLK"
#define BARCODE_SAAR      "LKK"
#define BARCODE_PUCK      "LKL"
#define MQTT_HOST         IPAddress(192, 168, 1, 23)  //YOUR mqtt server
#define MQTT_PORT         1883                        //mqtt port
#define SCHEDULE_SAAR     "YES"						  //Cat needs timeslot
#define SCHEDULE_PUCK     "NO"						  //Cat needs timeslot



#include <WiFi.h>
#include <NTPClient.h>

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMQTT_ESP32.h>


// *** motor
const int dirPin = 27;                                      // motor direction pin
const int stepPin = 16;                                     // motor step pin
int speed = 1500;                                           // duration between steps
int dir = 0;                                                // 1 = counterclockwise
double speedfactor = 0;                                     // depending on speed, change intervals with speedfactor

// *** mqtt ****
const char *PubTopic  = "catfeeder.mqtt.publish";        	// Topic to publish
const char *ListenTopic = "catfeeder.mqtt.instruction"; 	 // Topic to listen to
String mqttMessage = "";                                    // mqtt message to send
String mqttPayload = "";                                    // mqtt message received
AsyncMqttClient mqttClient;                                 // asynchronous mqqtClient
TimerHandle_t mqttReconnectTimer;                           // asynchronous mqqtClient
TimerHandle_t wifiReconnectTimer;                           // asynchronous mqqtClient

// *** RFID Reader ***
int ByteReceived = 0;                                       // INT for received serial data (RFID)
int Reader_reset = 13;                                      // RFID reset pin
int Detected = 0;                                           // Cat in sight of reader
int RFID_Count = 0;                                         // Number of times cat is detected while eating
String strCode = "";                                        // Current RFID code read
unsigned long lastResetReaderTime = 0;                      // Time loop: Read-interval RFID Reader
const unsigned long resetReaderInterval = 1000;             // Time loop: Read-interval RFID Reader

// *** Infrared Reader ***
unsigned long oldTimeWhite = millis();
unsigned long oldTimeBlack = millis();
int IRStatus = 1;                                           // infrared white detection
int oldIRStatus = 1;                                        // trigger/status for state change IRStatus
int IR_sensor = 14;                                         // IR Sensor pin
int duration = 0;                                           // length (in time) black stripe
int durationWhite = 0;                                      // length (in time) white part

// *** General ***
unsigned long lastResetTime = 0;                            // Time loop: Time for cat to eat without being detected
const unsigned long resetInterval = 12000;                  // TIme loop: Time for cat to eat without being detected
unsigned long lastResetMotorTime = 0;                       // Time loop: If barcode can't be found, stop motor after stopInterval milliseconds
unsigned long resetMotorInterval = 20000;                   // Time loop: If barcode can't be found, stop motor after stopInterval milliseconds
int whiteFound = 0;                                         // trigger/status for detection white       
int blackFound = 0;                                         // trigger/status for detection black
String Cat = "";                                            // current Cat
String barcode = "";                                        // current barcode reading
String disclocation = "";                                   // current location disc
String CatBarcode = "";                                     // LKK = Saar, LKL = Puck, KLK = Home. L = Long, K = Short
String mqttDuration = "";                                   // String with durations current move (example: "Duration:KLK;11;55;12;12;66;18")

// *** timeclient, used for timewindow  ***
// Define the NTP Server
const char *ntpServer = "pool.ntp.org";
// Define the time zone
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);


int eatingAllowed = 1;                                      // timeslot for NOT eating
int eatingWindowStartHour = 12;
int eatingWindowStartMinute = 5;
int eatingWindowEndHour = 17;
int eatingWindowEndMinute = 55;


void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
#if USING_CORE_ESP32_CORE_V200_PLUS

    case ARDUINO_EVENT_WIFI_READY:
      Serial.println("WiFi ready");
      break;

    case ARDUINO_EVENT_WIFI_STA_START:
      Serial.println("WiFi STA starting");
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("WiFi STA connected");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.println("WiFi lost IP");
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
#else
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
#endif

    default:
      break;
  }
}

void onMqttConnect(bool sessionPresent)
{
  Serial.print("Connected to MQTT broker: ");
  uint16_t packetIdSub = mqttClient.subscribe(ListenTopic, 2);          //mqtt subscribe to listen
  mqttClient.publish(PubTopic, 0, true, "MQTT connection established"); //first mqtt message
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  (void) reason;
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(const uint16_t& packetId, const uint8_t& qos)
{
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(const uint16_t& packetId)
{
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties,const size_t& len, const size_t& index, const size_t& total)
{
  (void) payload;
  //Serial.println(payload);

  mqttPayload = "";
  for (int r = 0; r < len; r++) {
    mqttPayload = mqttPayload + payload[r];
  }  
  
  if (mqttPayload.startsWith("Bak Status")) {
    mqttBakStatus();
  }  

  if (mqttPayload.startsWith("Bak Saar")) {
    movePosition(BARCODE_SAAR);
  }

  if (mqttPayload.startsWith("Detect Saar")) {
    SaarDetected();
  }

  if (mqttPayload.startsWith("Detect Puck")) {
    PuckDetected();
  }

  if (mqttPayload.startsWith("Bak Puck")) {
    movePosition(BARCODE_PUCK);
  }

  if (mqttPayload.startsWith("Bak home")) {
    movePosition(BARCODE_HOME);
  }

  if (mqttPayload.startsWith("Steps:")) {
    speed = mqttPayload.substring(6).toInt();
  }

  if (mqttPayload.startsWith("eatingWindowStartHour:")) {
    eatingWindowStartHour = mqttPayload.substring(22).toInt();
  }

  if (mqttPayload.startsWith("eatingWindowEndHour:")) {
    eatingWindowEndHour = mqttPayload.substring(20).toInt();
  }

  if (mqttPayload.startsWith("eatingWindowStartMinute:")) {
    eatingWindowStartMinute = mqttPayload.substring(24).toInt();
  }

  if (mqttPayload.startsWith("eatingWindowEndMinute:")) {
    eatingWindowEndMinute = mqttPayload.substring(22).toInt();
  }

  if (mqttPayload.startsWith("Reset ESP")) {
    ESP.restart();
  }

}

float  GetValue(String pString){
  char vTempValue[10];
  pString.toCharArray(vTempValue,sizeof(vTempValue));
  return  atof(vTempValue);
}

void onMqttPublish(const uint16_t& packetId)
{
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void setup()
{
  Serial.begin(9600);
  while (!Serial && millis() < 5000);
  delay(500);

  //set pinouts
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode (Reader_reset, OUTPUT);
  digitalWrite (Reader_reset, HIGH);

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,
                                    reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,
                                    reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();
  
  //reset status cats, set disc to homeposition (KLK)
  mqttCatStatus("Puck", "gone");
  mqttCatStatus("Saar", "gone");
  movePosition (BARCODE_HOME);

  timeClient.begin();
}


int checkEatingAllowed (String Schedule) {
  timeClient.update();
  mqttAnyMessage(timeClient.getFormattedTime());

  if (Schedule == "YES") {
    // Get the current time
    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    String mymessage = "";
    mymessage = "Decline from:" + String(eatingWindowStartHour) + ":" + String(eatingWindowStartMinute) + " tot " + String(eatingWindowEndHour) + ":" + String(eatingWindowEndMinute);
    mqttAnyMessage (mymessage);

    // Check if the current time is between timewindow. If it is, NOT allowed to eat
    if ((hours == eatingWindowStartHour && minutes >= eatingWindowStartMinute) || (hours > eatingWindowStartHour && hours < eatingWindowEndHour) || (hours == eatingWindowEndHour && minutes <= eatingWindowEndMinute)) {
      // Do something if the current time falls between window
      return 0;
    } else {
      // Do something else if the current time does not fall between window
      return 1;
    }
  } else {
    return 1;
  }
}

void mqttCatStatus (String mqttCat, String catStatus){
  //send CatStatus (name cat, status) to mqtt broker
  mqttMessage = "";
  mqttMessage = mqttCat + " " + catStatus; 
  const char *char_mqttMessage = mqttMessage.c_str();
  Serial.println(char_mqttMessage);
  mqttClient.publish(PubTopic, 0, true, char_mqttMessage);
}

void mqttBakStatus (){
  mqttMessage = "";
  mqttMessage = "Location:" + disclocation; 
  const char *char_mqttMessage = mqttMessage.c_str();
  mqttClient.publish(PubTopic, 0, true, char_mqttMessage);
}

void mqttRFID_Count (){
  mqttMessage = "";
  mqttMessage = "RFID Count:" + String(RFID_Count); 
  const char *char_mqttMessage = mqttMessage.c_str();
  mqttClient.publish(PubTopic, 0, true, char_mqttMessage);
}

void mqttEatingAllowedStatus (String mqttCat, String mqttEatingAllowed){
  //send durationmeasurements to mqtt broker
  mqttMessage = "";
  mqttMessage = mqttCat + " " + mqttEatingAllowed;  
  const char *char_mqttMessage = mqttMessage.c_str();
  mqttClient.publish(PubTopic, 0, true, char_mqttMessage);
}

void mqttDurationStats (){
  //send durationmeasurements to mqtt broker
  mqttMessage = "";
  mqttMessage = mqttDuration; 
  const char *char_mqttMessage = mqttMessage.c_str();
  mqttClient.publish(PubTopic, 0, true, char_mqttMessage);
}

void SaarDetected (){      
  int result = 0;
  Detected = 1;
  if (Cat == "Puck"){
    mqttCatStatus("Puck", "gone"); 
    mqttRFID_Count();
    Cat = "";
  }

  if (Cat == "Saar"){
    RFID_Count++;
  }

  if (Cat == "") {
    Cat = "Saar";
    RFID_Count = 1;
    mqttCatStatus(Cat, "detected");
    result = checkEatingAllowed(SCHEDULE_SAAR);
    if (result==1) {
      mqttEatingAllowedStatus (Cat, "allowed");
      movePosition (BARCODE_SAAR);
    } else {
      mqttEatingAllowedStatus (Cat, "declined");
      if (disclocation != BARCODE_HOME) {
        movePosition (BARCODE_HOME);
      }
    }
  } 
}

void PuckDetected (){ 
  int result = 0;     
  Detected = 1;
  if (Cat == "Saar"){
    mqttCatStatus("Saar", "gone"); 
    mqttRFID_Count();
    Cat = "";
  }

  if (Cat == "Puck"){
    RFID_Count++;
  }

  if (Cat == ""){
    Cat = "Puck";
    RFID_Count = 1;
    mqttCatStatus(Cat, "detected");
    result = checkEatingAllowed(SCHEDULE_PUCK);
    if (result==1) {
      mqttEatingAllowedStatus (Cat, "allowed");
      movePosition (BARCODE_PUCK);
    } else {
      mqttEatingAllowedStatus (Cat, "declined");
      if (disclocation != BARCODE_HOME) {
        movePosition (BARCODE_HOME);
      }
    }
  }
}

void mqttAnyMessage (String message){
  mqttMessage = "";
  mqttMessage = message; 
  const char *char_mqttMessage = mqttMessage.c_str();
  mqttClient.publish(PubTopic, 0, true, char_mqttMessage);
}

void movePosition (String CatBarcode) {
  unsigned long currentTime = millis();
  lastResetMotorTime = currentTime;
  if (CatBarcode != disclocation){

    mqttDuration = "Duration:" + CatBarcode + ":";
    oldTimeWhite = millis();
    oldTimeBlack = millis();

    if (CatBarcode == BARCODE_SAAR) { //Saar
      dir = 1;
    }
    if (CatBarcode == BARCODE_PUCK) { //Puck
      dir =0;
    }

    if (CatBarcode == BARCODE_PUCK and disclocation == BARCODE_SAAR) { //Puck while at Saar
      dir =1;
    }
    
    if (CatBarcode == BARCODE_SAAR and disclocation == BARCODE_PUCK) { //Saar while at Puck
      dir =0;
    }

    if (CatBarcode == BARCODE_HOME and disclocation == BARCODE_SAAR) { //Home while at Saar
      dir = 0;
    }

    if (CatBarcode == BARCODE_HOME and disclocation == BARCODE_PUCK) { //Home while at Puck
      dir = 1;
    }
    
    // move motor, disclocation changes in OneStep, stop if barcode is not found
    while (disclocation != CatBarcode) {
      OneStep (dir,speed);
      unsigned long currentTime = millis();
      if ( currentTime - lastResetMotorTime >= resetMotorInterval) {
        //send errormessage to mqttbroker and create endless loop (motor stops)
        mqttMessage = "ERROR: Barcode niet gevonden, motor gestopt. Controleer kattenfeeder.";
        const char *char_mqttMessage = mqttMessage.c_str();
        mqttClient.publish(PubTopic, 0, true, char_mqttMessage);
        while (1){
          //endless loop
        };
      }
    }
      
    blackFound = 0;
    //move until next black and stop
    while (blackFound == 0) {
      OneStep (dir,speed);
    }
  }
  mqttDurationStats ();
  whiteFound = 0;
  blackFound = 0;
}

void OneStep(int dir, int speed)
{
  if (speed == 1000) {
    speedfactor = 1;
  }

  if (speed == 1500) {
    speedfactor = 1.7;
  }

  if (speed == 2000) {
    speedfactor = 2.3;
  }
  
  unsigned long currentTime = millis();
  digitalWrite(dirPin, dir);
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(speed);
  digitalWrite(stepPin, LOW);
  delayMicroseconds(speed);
  IRStatus = digitalRead(IR_sensor);
  if (IRStatus == 1 and IRStatus != oldIRStatus)
  {
    durationWhite = (currentTime - oldTimeWhite);
    oldTimeBlack = currentTime;
    oldIRStatus = IRStatus;
    //if (whiteFound != 1) {
      //set black found only if no large white was found before that - to be tested
    blackFound = 1;
    //}
  } else
  {
    if (IRStatus == 0 and IRStatus != oldIRStatus)
    {

      if (durationWhite > (500 * speedfactor ))
      {
        whiteFound = 1;
        barcode = "";
      } 
      oldTimeWhite = currentTime;
      duration = (currentTime - oldTimeBlack);
      mqttDuration = mqttDuration +';' + duration;
      oldIRStatus = IRStatus;
      if (whiteFound == 1) {
        if (duration >=(5 * speedfactor) and duration <= (22* speedfactor)) {
          barcode = barcode + "K";
        }
        if (duration >=(30 * speedfactor) and duration <= (55*speedfactor)) {
          barcode = barcode + "L";
        }
        if (duration >=(100 * speedfactor) and duration <= (150 * speedfactor)) {
          disclocation = "CAT";
          barcode = "";
          //disclocation = "";
        }
      }
      if (barcode.length() == 3) {
        disclocation = barcode;
        barcode = "";
      }
    }
  }
}

// ***********  main loop  ************** 
void loop()
{
  unsigned long currentTime = millis();
  //reset RFID Reader after 1 second to check if cat is still in sight - 18/2: asynchronous using resetReaderInterval
  if ( currentTime - lastResetReaderTime >= resetReaderInterval)
  { 
    if (Detected == 1){
      Detected = 0;
      digitalWrite (Reader_reset, LOW);
      delay(10);
      digitalWrite (Reader_reset, HIGH);
      delay(10);
      lastResetTime = currentTime;
    }
      lastResetReaderTime = currentTime;
  }

  // set cat = gone to mqtt broker after x seconds
  if ( currentTime - lastResetTime >= resetInterval and Detected == 0){
    if (Cat != ""){
      mqttCatStatus(Cat, "gone");
      mqttRFID_Count();
      Cat = "";
      movePosition (BARCODE_HOME);
      lastResetTime = currentTime;
    }
  }

//read rfid data
if (Serial.available() > 0) {
    // read byte of received data:
  
    ByteReceived = Serial.read();
    if (isalpha((char)ByteReceived) or isdigit((char)ByteReceived)) 
    {
      strCode = strCode + (char)ByteReceived;
    }
    else 
    {
      Serial.println(strCode);

      if (strCode.startsWith(RFID_SAAR) and Detected == 0) //Saar
      {
        SaarDetected();
      }
      if (strCode.startsWith(RFID_PUCK) and Detected == 0) //Puck
      {
        PuckDetected();
      }
      strCode = "";
    }      
  }
}
