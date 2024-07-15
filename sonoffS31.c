/*-----     Custom Sonoff S31 Firmware v4      -----*/
/*-----             Unified Device             -----*/
/*-----  https://github.com/dervomsee/CSE7766  -----*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <CSE7766.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

CSE7766 theCSE7766;

/*-------- User-Defined Variables ----------*/

// WiFi Credentials
#ifndef STASSID
#define STASSID "Your-WiFi-SSID"  // SSID
#define STAPSK  "Your-WiFi-Pass"  // Password
#endif

#define WiFiHostname "S31B"

/*-------- Program Variables ----------*/

// GPIO
#define RELAY_PIN       12
#define LED             13
#define BUTTON           0
#define CSE7766TX        1
#define CSE7766RX        3

const char * ssid = STASSID;
const char * pass = STAPSK;
ESP8266WebServer server(80);
bool buttonState = false;

/*-------- Main Functions ----------*/

void setup() {
  // Initialize
  theCSE7766.setRX(1);
  theCSE7766.begin(); // will initialize serial to 4800 bps

  // Bring relay pin LOW to turn off load
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Bring LED pin LOW to turn on
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  // Activate the button as an interrupt - ESP WILL FAIL BOOT IF SET TO INPUT_PULLUP
  pinMode(BUTTON, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON), buttonWatcher, HIGH);

  // Connect to WiFi
  connectWiFi();

  // Start HTML Server
  serverSetup();

  //Initialize Arduino OTA
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
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
  ArduinoOTA.setHostname(WiFiHostname);
  ArduinoOTA.setPassword(STAPSK);
  ArduinoOTA.begin();
}

void loop() {
  // Safety Shutoff at 15A
  theCSE7766.handle();
  if (theCSE7766.getCurrent() > 15) {
  	digitalWrite(RELAY_PIN, LOW);
  }

  // Button Trigger
  if (buttonState) {
    toggle();
    buttonState = false;
  }

  // Webserver
  server.handleClient();

  //Arduino OTA
  ArduinoOTA.handle();

  // Let the ESP8266 do its thing
  yield();
}

/*-------- Button Interrupt ----------*/

ICACHE_RAM_ATTR void buttonWatcher() {
  buttonState = !buttonState;
}

/*-------- WiFi Code ----------*/

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WiFiHostname);
  WiFi.begin(ssid, pass);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) <= 10000) // Timeout WiFi connection attempt after 10 sec
  {
    delay(500);
    digitalWrite(LED, !digitalRead(LED));
  }
  digitalWrite(LED, HIGH);
}

/*-------- Server Calls ----------*/

void serverSetup() {
  server.on("/", handleRoot);
  server.on("/toggle", HTTP_GET, toggle);
  server.on("/turnOn", HTTP_GET, turnOn);
  server.on("/turnOff", HTTP_GET, turnOff);
  server.onNotFound(handleNotFound);
  server.begin();
}

String webpage =   ""
                   "<!DOCTYPE html>"
                   "<html>"
                   "<head>"
                     "<title>Sonoff %deviceName%</title>"
                     "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
                     "<meta name=\"viewport\" content=\"width=device-width\" />"
                     "<meta http-equiv=\"refresh\" content=\"10\" />"
                     "<style>"
                       "body {background-color: #000; color: #a40; font-family: Helvetica;}"
                       "input.colorButton { width: 100%; height: 2.5em; padding: 0; font-size: 2em; background-color: #222; border-color: #222; color: #a40; font-family: Helvetica;} "
                     "</style>"
                   "</head>"
                   "<body>"
                     "<form action=\"/toggle\" method=\"GET\"><input type=\"submit\" value=\"Turn %toggleStub%\" class=\"colorButton\"></form>"
                     "<p align=\"center\">Voltage: %voltageStub% V<br>"
                     "Current: %currentStub% A<br>"
                     "Active Power: %apowerStub% W<br>"
                     "Apparent Power: %appowerStub% VA<br>"
                     "Reactive Power: %rpowerStub% VAR<br>"
                     "Power Factor: %pfactorStub% %<br>"
                     "Energy: %energyStub% Ws</p>"
                   "</body>"
                   "</html>";

void handleRoot() {
  String deliveredHTML = webpage;

  deliveredHTML.replace("%deviceName%", WiFiHostname);

  if (digitalRead(RELAY_PIN) == HIGH) {
    deliveredHTML.replace("%toggleStub%", "Off");
  }
  else {
    deliveredHTML.replace("%toggleStub%", "On");
  }

  // Read the CSE7766
  theCSE7766.handle();

  deliveredHTML.replace("%voltageStub%", (String)theCSE7766.getVoltage());
  deliveredHTML.replace("%currentStub%", (String)theCSE7766.getCurrent());
  deliveredHTML.replace("%apowerStub%", (String)theCSE7766.getActivePower());
  deliveredHTML.replace("%appowerStub%", (String)theCSE7766.getApparentPower());
  deliveredHTML.replace("%rpowerStub%", (String)theCSE7766.getReactivePower());
  deliveredHTML.replace("%pfactorStub%", (String)theCSE7766.getPowerFactor());
  deliveredHTML.replace("%energyStub%", (String)theCSE7766.getEnergy());
  server.send(200, "text/html", deliveredHTML);
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

void toggle() {
  digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
  redirect();
}

void turnOn() {
  digitalWrite(RELAY_PIN, HIGH);
  redirect();
}

void turnOff() {
  digitalWrite(RELAY_PIN, LOW);
  redirect();
}

void redirect() {
  yield();
  server.sendHeader("Location", "/");
  server.send(303);
}
