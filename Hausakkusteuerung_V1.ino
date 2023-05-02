#include <Arduino.h>
#include <Arduino_JSON.h>
#include <ArduinoHttpClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <Adafruit_ADS1X15.h>
#include <DigiPotX9Cxxx.h>


//******************************************************************************
// Init
//******************************************************************************

// AD Converter
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */

// Servers
ESP8266WebServer server(80);

// Clients
WiFiClient espClient;
PubSubClient client(espClient);

// WIFI
#ifndef STASSID
#define STASSID "Fritzbox"
#define STAPSK "Asternstrasse21"
#endif
// host name the ESP8266 will show in WLAN
#define HOSTNAME                     "Hausakkusteuerung"

//Static IP address configuration
IPAddress staticIP(192, 168, 0, 25); //ESP static ip
IPAddress gateway(192, 168, 0, 100);   //IP Address of your WiFi Router (Gateway)
IPAddress subnet(255, 255, 255, 0);  //Subnet mask
IPAddress dns(8, 8, 8, 8);  //DNS

#define SERIAL_SPEED 115200

// WIFI Logindaten
const char* ssid = STASSID;
const char* password = STAPSK;
const char* deviceName = HOSTNAME;

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.0.123";

// Pins for Potcontrol
DigiPot pot(D7,D6,D8);  // Format: INC; U/D; CS means D7=>INC; D6=>U/D; D8=>CS

// Setup der Variablen
int16_t BatSpg = 5;
int16_t BatStr = 6;
unsigned long previousMillis = millis(); // Zeitmessung, um die Wh messen zu können
const long interval = 1000; // Intervall zwischen 2 Messungen
long Spanne;
float BatLeistung = 253;
String Akkuleistung = "Bezug";
float WhImported = 3760;
float WhExported = 5860;
float TeilSpg = 440;
float TeilStr = 349;
float BatSpannung = 5;
float BatStrom = 5;
float BatKap = 14784000;
float BatSOC = 2956800;
float SOC = 20;
long lastMsg = 0;
char msg[50];
int value = 0;
String messageTemp = "0";
float Leistung = 0;
int Mittelwert = 0;
int Potiint = 0;
int Potiwert = 0;
String postForms = "Noch nix";
const int ledPin = D4;
String Lader = "Nix";
String WR = "Nix";
float volts0, volts1, volts2, volts3;
byte shelly2 = 0;


void handleRoot();              // function prototypes for HTTP handlers
void handleNotFound();

//******************************************************************************
// Setup()
//******************************************************************************

void setup() {
  // put your setup code here, to run once:
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
// init serial port
  Serial.begin(SERIAL_SPEED);
  while (!Serial);
  delay(1000);

// Start WiFi and services
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("");
  delay(100);
  WiFi.disconnect();  //Prevent connecting to wifi based on previous configuration
  delay(100);
  WiFi.hostname(deviceName);      // DHCP Hostname (useful for finding device for static lease)
  delay(1000);
  WiFi.config(staticIP, subnet, gateway, dns);
  delay(1000);
  WiFi.begin(ssid, password);

  WiFi.mode(WIFI_STA); //WiFi mode station (connect to wifi router only
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP


  delay(1000);
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Hausakkusteuerung");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  delay(1000);
  ArduinoOTA.begin();

// Start the mDNS Responder to find the ESP in our homenet
  if (MDNS.begin("hausakkusteuerung")) {              // Start the mDNS responder for hausakkusteuerung.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

// Setup MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  Serial.println("MQTT setup finished");
  
// Start the webserver
  server.on("/", handleRoot);               // Call the 'handleRoot' function when a client requests URI "/"
  server.onNotFound(handleNotFound);        // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"

  server.begin();                           // Actually start the server
  Serial.println("HTTP server started");
  
  // Set Potentiometer Wiper to 0
  for (int i = 0; i < 100; i++) {
  pot.decrease(1);
      delay(20);  
  }
  Potiwert = 0;
  
  // The ADC input range (or gain) can be changed via the following
  // functions, but be careful never to exceed VDD +0.3V max, or to
  // exceed the upper and lower limits if you adjust the input range!
  // Setting these values incorrectly may destroy your ADC!
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
  
  // Initializing AD converter
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1);
  }

}

 // callback routine if an MQTT topic has been transmitted 
void callback(char* topic, byte* message, unsigned int length) {
  digitalWrite(ledPin, LOW);
  messageTemp = "";
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
    
  // copy payload to messageTemp
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

    // If a message is received on the topic openWB/evu/W, we calculate the potentiometer value. 
    if (String(topic) == "openWB/evu/W") {
    Serial.println("Message = ");
    Serial.print(messageTemp);
    Serial.print("Changing output to ");
    Leistung = -messageTemp.toInt();
    Mittelwert = (9 * Mittelwert + Leistung) / 10; // floating mean value to decide whether charger or inverter to switch on
    Serial.print("Mittelwert = ");
    Serial.println(String(Mittelwert));
    Potiint = Leistung / 40;  // potentiometer value to set new value for digital poti
    
    Serial.print("Potiint = ");
    Serial.println(String(Potiint));
    if (Mittelwert < -100)
    {
      digitalWrite (RX, LOW);  // switch Charger off
      digitalWrite (TX, HIGH);  // switch Inverter on
      Lader = "Aus";
      WR = "Ein";
    }
    else if (Mittelwert > 150)
    {
      digitalWrite (RX, HIGH);  // switch Charger on
      digitalWrite (TX, LOW);  // switch Inverter off
      Lader = "Ein";
      WR = "Aus";
    }
    else {
      digitalWrite (RX, LOW);  // switch Charger off
      digitalWrite (TX, LOW);  // switch Inverter off
      Lader = "Aus";
      WR = "Aus";
    }
    }
    
    // If a message is received on the topic shellies/shellyplug-s-3CE90EC7E630/relay/0, we store this binary value in a variable. 
    if (String(topic) == "shellies/shellyplug-s-3CE90EC7E630/relay/0") {
    if (messageTemp == "on") {
    shelly2 = 1;
    }
    else
    {
      shelly2 = 0;
    }}
    
    // Changes the potentiometer value according to the message and post it via serial
    Serial.println(String(Leistung));
    Serial.print("Potiwert = ");
    Serial.println(String(Potiwert));
    
    // decide whether we draw power from the net or if we supply power to the net
    if (Leistung < 0) {
      Akkuleistung = "Bezug";
    }
    else {
      Akkuleistung = "Einspeisung";
    }
    
    // render the HTML website with helpful info for debugging
    postForms = "<html>\
    <head>\
      <title>AD and Poti values and PV-values</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
      </style>\
      <meta http-equiv=\"refresh\" content=\"2\">\
    </head>\
    <body>\
      <p><br>Message arrived on topic: " + String(topic) + ". Message: " + messageTemp + "<br>\
      <br>Wir haben " + String(Leistung) + " Watt " + Akkuleistung +"<br>\
      Negativer Wert = Netzbezug<br>\
      ShellyPlug2 = " + String(shelly2) + "<br>\
      Potiwert = " + String(Potiwert) + "<br>\
      Potiint = " + String(Potiint) + "<br>\
      Mittelwert = " + String(Mittelwert) + "<br>\
      AD-Wert Spannung = " + String(BatSpg) + "<br>\
      AD-Wert Strom = " + String(BatStr) + "<br>\
      Batteriespannung = " + String(BatSpannung) + "<br>\
      Batteriestrom = " + String(BatStrom) + "<br>\
      Batterieleistung = " + String(BatLeistung) + "<br>\
      Ladezustand [Wh] = " + String(BatSOC) + "<br>\
      State of Charge = " + String(SOC) + "<br>\
      Wattstunden exportiert = " + String(WhExported) + "<br>\
      Wattstunden Importiert = " + String(WhImported) + "<br>\
      Lader = " + Lader + "<br>\
      Wechselrichter = " + WR + "<br></p>\
    </body>\
    </html>";
}

// test MQTT connection
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe topics needed
      client.subscribe("openWB/evu/W");
      client.subscribe("shellies/shellyplug-s-3CE90EC7E630/relay/0");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  // Run the following code only once in a second
unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) { 
    Spanne = currentMillis - previousMillis;
    previousMillis = currentMillis; 
    
    // OTA connection
    ArduinoOTA.begin();
    ArduinoOTA.handle();

// Server connection
    server.handleClient();                    // Listen for HTTP requests from clients

     
// MQTT connection
    if (!client.connected()) {
    reconnect();
    } 

    client.loop();
    
    // read Battery current and Battery voltage AD values (ADS1115)
    BatSpg = ads.readADC_SingleEnded(0);
    BatStr = ads.readADC_SingleEnded(1);
    Serial.println(BatStr);
    Serial.println(BatSpg);

    // calculate Battery voltage and current and then Battery power
    BatSpannung = BatSpg / TeilSpg;  // battery voltage due to voltage divider
    BatStrom = (BatStr - 17666) / TeilStr; // offset added as the current sensor (Allegro ACS758LCB-050B-PFF-T) is bidirectional, so the zero point is in "the middle"
    
    // Suppress AD converter noise
    if (BatStrom < 0.1 && BatStrom > -0.1) {
      BatStrom = 0;
    }
    
    BatLeistung = BatSpannung * BatStrom;
    
    // is Battery full?
    if (BatSpannung > 57.5) {
      BatSOC = BatKap;
    }

    // Calculate SOC of Battery
    BatSOC = BatSOC + (BatLeistung / (3600 * Spanne));
    SOC = BatSOC / (BatKap / 100);

  if (shelly2 == 1) {  // if charger is on, set potentiometer value (X9C103s digital Poti)
    if(Potiint > 0){  // more charging current!
      pot.increase(1);
      delay(200);  
      Potiwert++; 
    }
    
    if(Potiint < 0)  // less charging current!
    {
      pot.decrease(1);
      delay(200);
      Potiwert--;
    }
  }
  else {
    Potiwert = 0;  // if charger is off, set potentiometer to 0
    for (int i = 0; i < 100; i++) {
      pot.decrease(1);
      delay(20);  
    }
  } 
  }
  
  // align Potiwert to real X9C103S adjustment
  if (Potiwert < 0) { 
    Potiwert = 0;
  }
  if (Potiwert > 100) {   // X9C103S has only 100 steps
    Potiwert = 100;
  }
  
  
  if (BatLeistung < 0)
  {
    WhExported = WhExported - (BatLeistung / (3600 * Spanne));  // Watthours exported for OpenWB Wallbox
  }
  else
  {
    WhImported = WhImported + (BatLeistung / (3600 * Spanne));  // Watthours imported for OpenWB Wallbox
  }
  }

void handleRoot() {
  server.send(200, "text/html", postForms);
  digitalWrite(ledPin, 0);
}

void handleNotFound(){
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
