#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

const char *ssid = "Utility_WIFI";
const char *password = "39277756";
String hostname = "HeatingOTA";

WebServer server(80);
IPAddress mqttserver(192, 168, 101, 117);
WiFiClient wificlient;
PubSubClient client(wificlient);

#define LED 02
#define ON HIGH
#define OFF LOW

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived, i.e. set an output
    // Print the topic
  Serial.print("Topic: ");
  Serial.println(topic);

  String message = "";
  // Print the message
  Serial.print("Message: ");
  for(int i = 0; i < length; i ++)
  {
    // Serial.print(char(payload[i]));
    message += (char(payload[i]));
  }

  Serial.print(message);
  
  // Print a newline
  Serial.println("");

  if (message == String("OFF")) {
    digitalWrite(LED,OFF);
    Serial.println("hello light is off");
  }
  if (message == String("ON")) {
    digitalWrite(LED,ON);
    Serial.println("hello light is on");
  }
}

long lastReconnectAttempt = 0;

boolean reconnect() {
  if (client.connect("TestClientFinn")) {
    // Once connected, publish an announcement...
    client.publish("outTopic","hello world, heating control here");
    // ... and resubscribe
    client.subscribe("heating");
  }
  return client.connected();
}



#define LIVING 04
#define DINING 35
#define CHILD1 34
#define OFFICE 36
#define KITCHEN 39

#define CONSERVATORY 14
#define BEDROOM 27
#define BATHROOM 16
#define HALL 25
#define STUDIO 17
#define CHILD3 26

#define CIRCULATE 18
#define HEATPUMP 19
#define HALLACTU 23
#define SPARE 5

#define START_INTERVAL 120000
#define STOP_INTERVAL  300000

#define SENSORARRAY 11
String sensorNames[SENSORARRAY] = { "Living room", "Dining room", "Child 1 bedroom", "Office", "Kitchen", "Conservatory", "Guest bedroom", "Bathroom", "Hall", "Studio","Child3 bedroom" };
int sensorChannels[SENSORARRAY] = { LIVING, DINING, CHILD1, OFFICE, KITCHEN, CONSERVATORY, BEDROOM, BATHROOM, HALL, STUDIO, CHILD3 };

void displaySensors() {
  
  String out = "";
  char temp[56];
  
  for (int i = 0; i < SENSORARRAY; i++) {
        sprintf(temp, "%s sensor is %d<br>\n", sensorNames[i].c_str(), digitalRead(sensorChannels[i]));
        out += temp;
  }
  server.send(200, "text/html", out);
}

#define CONTROLARRAY 4
String controlNames[CONTROLARRAY] = { "Circulation pump", "Heatpump", "Hall ACU", "Spare"};
int controlChannels[CONTROLARRAY] = { CIRCULATE, HEATPUMP, HALLACTU, SPARE };

void displayControls() {
  
  String out = "";
  char temp[56];
  
  for (int i = 0; i < CONTROLARRAY; i++) {
        sprintf(temp, "%s control is %d<br>\n", controlNames[i].c_str(), digitalRead(controlChannels[i]));
        out += temp;
  }
  server.send(200, "text/html", out);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void handleRoot() {
  char temp[1024];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 1024,

           "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>Heating Control OTA Version 1.0</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Heating Control OTA</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>Thermostats</p>\
    <iframe src=\"/sensors.html\" width=\"300\" height=\"400\" border:none;></iframe>\
    <p>Controls</p>\
    <iframe src=\"/controls.html\" width=\"300\" height=\"200\" border:none;></iframe>\
  </body>\
</html>",

           hr, min % 60, sec % 60
          );
  server.send(200, "text/html", temp);
}

int checkHeat() {
  int startHeat=0;
  int static timerStarted=0;
  int static timerStopped=0;
  int static heatStarted = 0;
  unsigned long static previousMillis = millis();
  unsigned long currentMillis = 0;

  startHeat=checkThermostats();
  
  if (!timerStarted&&startHeat) {
    timerStarted = 1;
    previousMillis = millis();
  }
  if (timerStarted&&!startHeat) {
    timerStarted = 0;
  }
  
  currentMillis = millis();
  
  if (timerStarted && (currentMillis - previousMillis > START_INTERVAL)) {
    // set the time that we started the heat
    heatStarted = 1;
    timerStarted = 0;

    // Turn on the circulation pump
    digitalWrite(CIRCULATE, ON);
    digitalWrite(HEATPUMP, ON);
  }

  if (heatStarted && !startHeat) {
    heatStarted = 0;
    timerStopped = 1;
    previousMillis = millis();

    digitalWrite(HEATPUMP, OFF);    
    // give the pump time to turn off
    delay(10);  }

  if (timerStopped && (currentMillis - previousMillis > STOP_INTERVAL)) {
    heatStarted = 0;
    timerStopped = 0;

    digitalWrite(CIRCULATE, OFF);
    // give the pump time to turn off
    delay(10);
  }
}

int checkThermostats() {
  static int startHeating;
  int Living = digitalRead(LIVING);
  int Dining = digitalRead(DINING);
  int Child1 = digitalRead(CHILD1);
  int Office = digitalRead(OFFICE);
  int Kitchen = digitalRead(KITCHEN);
  int Conservatory = digitalRead(CONSERVATORY);
  int Bedroom = digitalRead(BEDROOM);
  int Bathroom = digitalRead(BATHROOM);
  int Hall = digitalRead(HALL);
  int Studio = digitalRead(STUDIO);
  int Child3 = digitalRead(CHILD3);
  
  if (!Hall) {

    // Turn on Hall Actuator
    digitalWrite(HALLACTU, ON);    
  }

  if (Hall) {

    // Turn off Hall Actuator
    digitalWrite(HALLACTU, OFF);    
  }
  
  if (Living&&Dining&&Child1&&Office&&Kitchen&&Conservatory&&Bedroom&&Bathroom&&Hall&&Studio&&Child3) {
    startHeating = 0;
  }
  if (!Living||!Dining||!Child1||!Office||!Kitchen||!Conservatory||!Bedroom||!Bathroom||!Hall||!Studio||!Child3) {
    startHeating = 1;
  }
  return startHeating;
}

void setup(void) {
  Serial.begin(115200);
  WiFi.setHostname(hostname.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    Serial.println("Started MDNS responder");
  }

  server.on("/", handleRoot);
  server.on("/sensors.html", displaySensors);
  server.on("/controls.html", displayControls);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Started HTTP server");

  client.setServer(mqttserver, 1883);
  client.setCallback(callback);

  pinMode(LIVING,INPUT_PULLUP);
  pinMode(DINING,INPUT_PULLUP);
  pinMode(CHILD1,INPUT_PULLUP);
  pinMode(OFFICE,INPUT_PULLUP);
  pinMode(KITCHEN,INPUT_PULLUP);
  pinMode(CONSERVATORY,INPUT_PULLUP);
  pinMode(BEDROOM,INPUT_PULLUP);  
  pinMode(BATHROOM,INPUT_PULLUP);
  pinMode(HALL,INPUT_PULLUP); 
  pinMode(STUDIO,INPUT_PULLUP);
  pinMode(CHILD3,INPUT_PULLUP);

  pinMode(LED,OUTPUT);
  digitalWrite(LED,OFF);
  pinMode(HALLACTU,OUTPUT);
  digitalWrite(HALLACTU,OFF);
  pinMode(CIRCULATE,OUTPUT);
  digitalWrite(CIRCULATE,OFF);
  pinMode(HEATPUMP,OUTPUT);
  digitalWrite(HEATPUMP,OFF);
  
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
      Update.printError(Serial);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      Update.printError(Serial);
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      Update.printError(Serial);
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  //digitalWrite(LED,ON);
  checkHeat();
  
  server.handleClient();
  delay(10);//allow the cpu to switch to other tasks
  ArduinoOTA.handle();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  //digitalWrite(LED,OFF);  
  delay(10);//allow the cpu to switch to other tasks
}
