#include <Arduino.h>
#include <Arduino_JSON.h>
#include <ArduinoHttpClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <NTPClient.h>
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
#define STASSID "SSID"
#define STAPSK "PSK"
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
int16_t Nullpunkt = 18800;
unsigned long previousMillis = millis(); // Zeitmessung, um die Wh messen zu können
const long interval = 1000; // Intervall zwischen 2 Messungen
float Spanne;
float BatLeistung = 0.00;
String Akkuleistung = "Bezug";
float WhImported = 0.00;
float WhExported = 0.00;
float TeilSpg = 439.00;  // adjustment to voltage divider needed
float TeilStr = 320.00;  // adjustment to current Sensor needed. For the used Allegro and Gain 1, we have 40mV/A for the Allegro, and 0.125mV/Bit for the ADS1115, this leads to a factor of 40/0.125 = 320
float BatSpannung = 5.0;
float BatStrom = 5.0;
float BatKap = 14784.00;
float SOC = 60.00;
float BatSOC = BatKap * (SOC/100);
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
boolean bLader = false;
boolean bWR = false;
String Lader = "Nix";
String WR = "Nix";
String Test = "Anfang";
boolean Wartungsladung = true;
float maxSOC = 100.00;

// NTP Client variables
const long utcOffsetInSeconds = 3600;
unsigned long EpochTime;
unsigned long LetzteLadung = 0;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);


void handleRoot();              // function prototypes for HTTP handlers
void handleNotFound();

//******************************************************************************
// Setup()
//******************************************************************************

void setup() {
  // put your setup code here, to run once:
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  pinMode(D3, OUTPUT); // Select Pin D3 for switching Charger
  digitalWrite(D3, HIGH); // Switch Charger off
  pinMode(D5, OUTPUT); // Select Pin D5 for switching Inverter
  digitalWrite(D5, HIGH); // Switch Inverter off
  
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
timeClient.begin();

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
    Mittelwert = (49 * Mittelwert + Leistung) / 50; // floating mean value to decide whether charger or inverter to switch on
    Serial.print("Mittelwert = ");
    Serial.println(String(Mittelwert));
    Potiint = Leistung / 40;  // potentiometer value to set new value for digital poti
    
    Serial.print("Potiint = ");
    Serial.println(String(Potiint));
    
    if (Mittelwert < -100)
     bWR = true;
    if (Mittelwert > 150)
     bLader = true;
    if (Mittelwert < -50)
     bLader = false;
    if (Mittelwert > 15)
     bWR = false;
    
    if (bWR)
    {
      digitalWrite (D3, HIGH);  // switch Charger off
      digitalWrite (D5, LOW);  // switch Inverter on
      Lader = "Aus";
      if (SOC >= 20.00) {
      WR = "Ein";
      }      
      Test = "WR entlädt";
    }
    
    if (bLader)
    {
      digitalWrite (D3, LOW);  // switch Charger on
      digitalWrite (D5, HIGH);  // switch Inverter off
      if (SOC <= maxSOC) {
      Lader = "Ein";
      }
      WR = "Aus";     
    }
    
    if (!bLader && !bWR) {
      digitalWrite (D3, HIGH);  // switch Charger off
      digitalWrite (D5, HIGH);  // switch Inverter off
      Lader = "Aus";
      WR = "Aus";
      if (abs(Nullpunkt - BatStr) > 0,2){
        //delay(5000);
        Nullpunkt = BatStr; // correct temperature drift of Allegro at zero current
      }
    }
    }
    
   
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
      Potiwert = " + String(Potiwert) + "<br>\
      Potiint = " + String(Potiint) + "<br>\
      Mittelwert = " + String(Mittelwert) + "<br>\
      AD-Wert Spannung = " + String(BatSpg) + "<br>\
      AD-Wert Strom = " + String(BatStr) + "<br>\
      Nullpunkt = " + String(Nullpunkt) + "<br>\
      Batteriespannung = " + String(BatSpannung) + "<br>\
      Batteriestrom = " + String(BatStrom) + "<br>\
      Batterieleistung = " + String(BatLeistung) + "<br>\
      Ladezustand [Wh] = " + String(BatSOC) + "<br>\
      State of Charge = " + String(SOC) + "<br>\
      Wattstunden exportiert = " + String(WhExported) + "<br>\
      Wattstunden Importiert = " + String(WhImported) + "<br>\
      Spanne = " + String(Spanne) + "<br>\
      Lader = " + Lader + "<br>\
      Wechselrichter = " + WR + "<br></p>\
      Test = " + Test + "<br></p>\
      Epoch Time = " + String(EpochTime) + "<br></p>\
      Letzte Ladung = " + String(LetzteLadung) + "<br></p>\
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
      client.subscribe("openWB/lp/1/W");
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
  if (currentMillis - previousMillis < 0) previousMillis = currentMillis;
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
    BatStrom = (BatStr - Nullpunkt) / TeilStr; // offset added as the current sensor (Allegro ACS758LCB-050B-PFF-T) is bidirectional, so the zero point is in "the middle"
    
    // Suppress AD converter noise
    if (BatStrom < 0.2 && BatStrom > -0.2) {
      BatStrom = 0;
     }

    // calculate battery power. As we have DC, direct multiplication is possible
    BatLeistung = BatSpannung * BatStrom;
    
    // is Battery full?
    if (BatSpannung > 57.5) {
      BatSOC = BatKap;
      Test = "Akku voll";
      Wartungsladung = false;
      maxSOC = 80;
      digitalWrite (RX, LOW);  // switch Charger off
      bLader = false;
      Lader = "Aus";
      timeClient.update();
      LetzteLadung = timeClient.getEpochTime();
    }

    if (bLader) {  // if charger is on, set potentiometer value (X9C103s digital Poti)
      if(Potiint > 0){  // more charging current!
        pot.increase(1); 
        Potiwert++; 
      }
    
      if(Potiint < 0)  // less charging current!
      {
        pot.decrease(1);
        Potiwert--;
      }
  }
  else {
    Potiwert = 0;  // if charger is off, set potentiometer to 0
    for (int i = 0; i < 100; i++) {
      pot.decrease(1);  
    }
  } 
  
  // align Potiwert to real X9C103S adjustment
  if (Potiwert < 0) { 
    Potiwert = 0;
  }
  if (Potiwert > 100) {   // X9C103S has only 100 steps
    Potiwert = 100;
  }
  
  // Calculate SOC of Battery
  BatSOC = BatSOC + ((BatLeistung / 3600.00) * (Spanne / 1000.00));
  
    
  if (BatLeistung < 0)
  {
    WhExported = WhExported - ((BatLeistung / 3600.00) * (Spanne / 1000.00));  // Watthours exported for OpenWB Wallbox
  }
  else
  {
    WhImported = WhImported + ((BatLeistung / 3600.00) * (Spanne / 1000.00));  // Watthours imported for OpenWB Wallbox
  }
  SOC = (BatSOC / BatKap) * 100.00;
  
  // Publish Battery data to OpenWB
    char msg_out[20];
    int intLeistung = (int)BatLeistung;
    sprintf(msg_out, "%d",intLeistung);
    client.publish("openWB/set/houseBattery/W", msg_out);
    dtostrf(WhImported,20,2,msg_out);
    client.publish("openWB/set/houseBattery/WhImported", msg_out);
    dtostrf(WhExported,20,2,msg_out);
    client.publish("openWB/set/houseBattery/WhExported", msg_out);
    int intSOC = (int)SOC;
    sprintf(msg_out, "%d",intSOC);
    client.publish("openWB/set/houseBattery/%Soc", msg_out);

    timeClient.update();
    EpochTime = timeClient.getEpochTime();

    if (!Wartungsladung && EpochTime - LetzteLadung > 3024000) {
      Wartungsladung = true;
      maxSOC = 100.00;
    }
  
  }
}

void handleRoot() {
  server.send(200, "text/html", postForms);
  digitalWrite(ledPin, 0);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
