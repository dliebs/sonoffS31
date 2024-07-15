//
//
//  Sonoff S31 Firmware - Version 1.1.0
//    This version was deployed 2024.02.02
//
//  Sonoff S31 Generic ESP8266 Module Based
//    Power Monitoring + Reporting
//
//  Changes From Previous Version
//    Transitioned to <espHTTPServer.h>
//
//

#include <espHTTPUtils.h>
#include <espWiFiUtils.h>
#include <espHTTPServer.h>

#include <CSE7766.h>
CSE7766 theCSE7766;

/*-------- User-Defined Variables ----------*/

// WiFi Credentials
#ifndef STASSID
#define STASSID "Your-WiFi-SSID"  // SSID
#define STAPSK  "Your-WiFi-Pass"  // Password
#endif
#define WiFiHostname "S31A"

// Webpage User Settings
#define BGCOLOR "000"
#define TABBGCOLOR "111"
#define BUTTONCOLOR "222"
#define TEXTCOLOR "a40"
#define FONT "Helvetica"
#define TABHEIGHTEM "47"
#define REFRESHPAGE true
#define PORT 80

// espHTTPServer Object
espHTTPServer httpServer( WiFiHostname, BGCOLOR, TABBGCOLOR, BUTTONCOLOR, TEXTCOLOR, FONT, TABHEIGHTEM, REFRESHPAGE, PORT );

// Cost of energy - $0.16/kWh
float costOfEnergy = 0.16;

// Toggle for ISR
bool buttonState = false;


/*--------     GPIO - DO NOT CHANGE    ----------*/

#define RELAY_PIN       12
#define LED             13
#define BUTTON           0
#define CSE7766TX        1
#define CSE7766RX        3


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
  httpServer.server.handleClient();

  //Arduino OTA
  ArduinoOTA.handle();

  // Let the ESP8266 do its thing
  yield();
}


/*-------- Button Interrupt ----------*/

ICACHE_RAM_ATTR void buttonISR() {
  buttonState = !buttonState;
}


/*--------    Server Functions    --------*/

void toggle() {
  digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
  httpServer.redirect();
}

void on() {
  digitalWrite(RELAY_PIN, HIGH);
  httpServer.redirect();
}

void off() {
  digitalWrite(RELAY_PIN, LOW);
  httpServer.redirect();
}

void setCost() {
  costOfEnergy = httpServer.server.arg("cost").toFloat();
  httpServer.redirect();
}

void status() {
  String statusOf = httpServer.server.arg("of");
  if ( statusOf == "state" ) { httpServer.server.send(200, "text/html", (String)digitalRead(RELAY_PIN)); }
  else if ( statusOf == "voltageV" ) { httpServer.server.send(200, "text/html", (String)theCSE7766.getVoltage()); }
  else if ( statusOf == "currentA" ) { httpServer.server.send(200, "text/html", (String)theCSE7766.getCurrent()); }
  else if ( statusOf == "activePowerW" ) { httpServer.server.send(200, "text/html", (String)theCSE7766.getActivePower()); }
  else if ( statusOf == "apparentPowerVA" ) { httpServer.server.send(200, "text/html", (String)theCSE7766.getApparentPower()); }
  else if ( statusOf == "reactivePowerVAR" ) { httpServer.server.send(200, "text/html", (String)theCSE7766.getReactivePower()); }
  else if ( statusOf == "powerFactorPC" ) { httpServer.server.send(200, "text/html", (String)theCSE7766.getPowerFactor()); }
  else if ( statusOf == "energyWs" ) { httpServer.server.send(200, "text/html", (String)(theCSE7766.getEnergy())); }
  else if ( statusOf == "energyKWH" ) { httpServer.server.send(200, "text/html", (String)(theCSE7766.getEnergy()/3600000)); }
  else if ( statusOf == "cost" ) { httpServer.server.send(200, "text/html", (String)(costOfEnergy * (theCSE7766.getEnergy()/3600000))); }
  else { handleNotFound(); }
}


/*--------     HTTP Server     ----------*/

void serverSetup() {
  httpServer.server.on("/", handleRoot);
  httpServer.server.on("/toggle", HTTP_GET, toggle);
  httpServer.server.on("/on", HTTP_GET, on);
  httpServer.server.on("/off", HTTP_GET, off);
  httpServer.server.on("/setCost", HTTP_GET, setCost);
  httpServer.server.on("/status", HTTP_GET, status);
  httpServer.server.onNotFound(handleNotFound);
  httpServer.server.begin();
}

String body = "<div class=\"container\">\n"
                "<div class=\"centered-element\">\n"
                  "<form action=\"/toggle\" method=\"GET\"><input type=\"submit\" value=\"Turn %toggleStub%\" class=\"simpleButton\" style=\"width: 100%;\"></form>\n"
                  "<form action=\"/setCost\" style=\"display: flex;\" method=\"GET\">\n"
                    "<input type=\"text\" class=\"textInput\" name=\"cost\" value=\"%costOfEnergyStub%\" style=\"width: 65%;\">\n"
                    "<input type=\"submit\" class=\"textInput\" style=\"width: 35%;\" value=\"$/kWh\">\n"
                  "</form>\n"
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
  String deliveredHTML = httpServer.assembleHTML(body);

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

  // Energy Costs
  deliveredHTML.replace("%costOfEnergyStub%", (String)costOfEnergy);
  deliveredHTML.replace("%costStub%", (String)(energyStub * costOfEnergy));

  httpServer.server.send(200, "text/html", deliveredHTML);
}

// Simple call back to espHTTPServer object for reasons
void handleNotFound() {
  httpServer.handleNotFound();
}
