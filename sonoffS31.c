/*-----      Custom Sonoff S31 Firmware v0      -----*/
/*-----  https://github.com/ingeniuske/CSE7766  -----*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

/*-------- User-Defined Variables ----------*/

// WiFi Credentials
#ifndef STASSID
#define STASSID "Your-WiFi-SSID"  // SSID
#define STAPSK  "Your-WiFi-Pass"  // Password
#endif

#define WiFiHostname "S31"

// GPIO
#define RELAY_PIN       12
#define LED             13
#define BUTTON			 0
#define CSE7766TX		 1
#define CSE7766RX		 3

/*-------- Program Variables ----------*/

const char * ssid = STASSID;
const char * pass = STAPSK;
ESP8266WebServer server(80);

/*-------- Main Functions ----------*/

void setup() {
  // Connect to WiFi
  connectWiFi();

  // Start HTML Server
  Serial.print("HTTP server starting... ");
  serverSetup();
  Serial.println("Done.");
  
  // Close the relay to switch on the load
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  // Initialise digital pin LED_SONOFF as an output.
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
}

void loop() {
  // Webserver
  server.handleClient();

  // Let the ESP8266 do its thing
  yield();
}

/*-------- WiFi Code ----------*/

void connectWiFi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(WiFiHostname);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/*-------- Server Calls ----------*/

void serverSetup() {
  server.on("/", handleRoot);
  server.on("/toggle", HTTP_POST, toggle);
  server.onNotFound(handleNotFound);
  server.begin();
}

String webpage =   ""
                   "<!DOCTYPE html>"
                   "<html>"
                   "<head>"
                   "<title>Sonoff S31</title>"
                   "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
                   "<meta name=\"viewport\" content=\"width=device-width\" />"
                   "<style>"
                   "body {background-color: #000; color: #a40; font-family: Helvetica;}"
                   "input.colorButton { width: 100%; height: 2.5em; padding: 0; font-size: 2em; background-color: #222; border-color: #222; color: #a40; font-family: Helvetica;} "
                   "input[type=range] { outline: 0; -webkit-appearance: none; width: 100%; height: 2.5em; margin: 0; background: linear-gradient(to right, #f00 0%, #ff8000 8.3%, #ff0 16.6%, #80ff00 25%, #0f0 33.3%, #00ff80 41.6%, #0ff 50%, #007fff 58.3%, #00f 66.6%, #7f00ff 75%, #f0f 83.3%, #ff0080 91.6%, #f00 100%); }"
                   "</style>"
                   "</head>"
                   "<body>"
                   "<form action=\"/toggle\" method=\"POST\"><input type=\"submit\" value=\"Turn %toggleStub%\" class=\"colorButton\"></form>"
                   "</body>"
                   "</html>";

void handleRoot() {
  String deliveredHTML = webpage;
  
  if (digitalRead(RELAY_PIN) == HIGH) {
    deliveredHTML.replace("%toggleStub%", "Off");
  }
  else {
    deliveredHTML.replace("%toggleStub%", "On");
  }
  
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
  // Close the relay to switch on the load
  digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
  digitalWrite(LED, !digitalRead(LED));
  redirect();
}

void redirect() {
  server.sendHeader("Location", "/");
  server.send(303);
}