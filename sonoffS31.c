//
// Custom Sonoff S31 Firmware v6
//
// Changes from v5
//   Added cost estimates, changed to kWh
//
// This version HNBD
//
// Credit to:
// https://github.com/dervomsee/CSE7766
//

#include <espHTTPUtils.h>
#include <espWiFiUtils.h>
#include <CSE7766.h>
#include "espHTTPServerUtils.h"

CSE7766 theCSE7766;

/*-------- User-Defined Variables ----------*/

// WiFi Credentials
#ifndef STASSID
#define STASSID "Your-WiFi-SSID"  // SSID
#define STAPSK  "Your-WiFi-Pass"  // Password
#endif

#define WiFiHostname "S31A"

// Define BASICPAGE or TABBEDPAGE
#define BASICPAGE

// Webpage Hex Colors
#define BGCOLOR "000"
#define TABBGCOLOR "111"
#define BUTTONCOLOR "222"
#define TEXTCOLOR "a40"
#define FONT "Helvetica"
#define TABHEIGHTEM "47"

/*-------- Program Variables ----------*/

// GPIO
#define RELAY_PIN       12
#define LED             13
#define BUTTON           0
#define CSE7766TX        1
#define CSE7766RX        3

bool buttonState = false;

/*-------- Main Functions ----------*/

void setup() {
  // Initialize
  theCSE7766.setRX(1);
  theCSE7766.begin(); // will initialize serial to 4800 bps

  // Bring relay pin LOW to turn off load
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Activate the button as an interrupt - ESP WILL FAIL BOOT IF SET TO INPUT_PULLUP
  pinMode(BUTTON, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON), buttonISR, HIGH);

  // Connect to WiFi, start OTA
  connectWiFi(STASSID, STAPSK, WiFiHostname, LED);
  initializeOTA(WiFiHostname, STAPSK);

  // Start HTML Server
  serverSetup();
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

ICACHE_RAM_ATTR void buttonISR() {
  buttonState = !buttonState;
}

/*-------- Server Calls ----------*/

void serverSetup() {
  server.on("/", handleRoot);
  server.on("/toggle", HTTP_GET, toggle);
  server.on("/on", HTTP_GET, on);
  server.on("/off", HTTP_GET, off);
  server.onNotFound(handleNotFound);
  server.begin();
}

String body = "<div class=\"container\">\n"
                "<div class=\"centered-element\">\n"
                  "<form action=\"/toggle\" method=\"GET\"><input type=\"submit\" value=\"Turn %toggleStub%\" class=\"simpleButton\"></form>\n"
                  "<p align=\"center\">Voltage: %voltageStub% V<br>\n"
                  "Current: %currentStub% A<br>\n"
                  "Active Power: %apowerStub% W<br>\n"
                  "Apparent Power: %appowerStub% VA<br>\n"
                  "Reactive Power: %rpowerStub% VAR<br>\n"
                  "Power Factor: %pfactorStub% %<br>\n"
                  "Energy: %energyStub% kWh<br>\n"
                  "Cost: &#36;%costStub%</p>\n"
                "</div>\n"
              "</div>\n";

void handleRoot() {
  String deliveredHTML = assembleHTML(body);

  if (digitalRead(RELAY_PIN) == HIGH) { deliveredHTML.replace("%toggleStub%", "Off"); }
                                 else { deliveredHTML.replace("%toggleStub%", "On"); }

  // Read the CSE7766
  theCSE7766.handle();

  deliveredHTML.replace("%voltageStub%", (String)theCSE7766.getVoltage());
  deliveredHTML.replace("%currentStub%", (String)theCSE7766.getCurrent());
  deliveredHTML.replace("%apowerStub%", (String)theCSE7766.getActivePower());
  deliveredHTML.replace("%appowerStub%", (String)theCSE7766.getApparentPower());
  deliveredHTML.replace("%rpowerStub%", (String)theCSE7766.getReactivePower());
  deliveredHTML.replace("%pfactorStub%", (String)theCSE7766.getPowerFactor());

  // Convert Watt-hours to kWh
  float energyStub = theCSE7766.getEnergy()/3600000;
  deliveredHTML.replace("%energyStub%", (String)energyStub);

  // Assume $0.16/kWh
  deliveredHTML.replace("%costStub%", (String)(energyStub*.16));
  server.send(200, "text/html", deliveredHTML);
}

void toggle() {
  digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
  redirect();
}

void on() {
  digitalWrite(RELAY_PIN, HIGH);
  redirect();
}

void off() {
  digitalWrite(RELAY_PIN, LOW);
  redirect();
}
