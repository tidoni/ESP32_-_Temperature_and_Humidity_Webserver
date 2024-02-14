/**
 * 
 * Date: 2023-02-14
 * Created by: Niklas Mueller
 * Inspired by: Rui Santos @ https://randomnerdtutorials.com/esp8266-dht11dht22-temperature-and-humidity-web-server-with-arduino-ide/
 * 
*/

// Import required libraries
#ifdef ESP32
#include <WiFi.h>

// For ESP32-PoE
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_PHY_POWER 12
#include <ETH.h>
static bool eth_connected = false;

#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <DHT.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h> // ESP8266 can be found at: https://github.com/vshymanskyy/Preferences



// ################################################################
// ### Change the following Settings to match your environment  ###
// ################################################################

// Replace with your network credentials

const char* ssid = "<ssid>";
const char* password = "<password>";

String logoURL = ".../logo.svg";   // Should be Landscape format
String faviconURL = "/favicon.svg";

// #define DHTPIN 32     // Digital pin connected to the DHT sensor | ESP32 (G32 -> 32)
#define DHTPIN 4     // Digital pin connected to the DHT sensor | ESP8266 (Number 4 is the D2 PIN)

// Uncomment the type of sensor in use:
// #define DHTTYPE    DHT11     // DHT 11
#define DHTTYPE    DHT22     // DHT 22 (AM2302)
// #define DHTTYPE    DHT21     // DHT 21 (AM2301)

// ################################################################
// ### Change the Settings above acordingly to your environment ###
// ################################################################


String string_location;
String string_settings;
String string_humidity;
String string_temperature;
String string_initial_password;
String string_new_location;
String string_update_location;


String location_of_device;
String admin_password;

Preferences preferences;
DHT dht(DHTPIN, DHTTYPE);

// current temperature & humidity, updated in loop()
float t = 0.0;
float tFahrenheit = 0.0;
float h = 0.0;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;    // will store last time DHT was updated
unsigned long previousMillisReboot = 0;    // will store last time DHT was updated

// Updates DHT readings every 10 seconds
const long interval = 10000;
// Reboot ESP every xxx Milli-Seconds
const long intervalReboot = 3600000;

void loadSettings() {
  preferences.begin("base-settings", false);
  location_of_device = preferences.getString("location", "No Location found");
  admin_password = preferences.getString("password", "No Password set");
  preferences.end();
}


#ifdef ESP32
void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}
#endif

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  dht.begin();
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);

  #ifdef ESP32
  WiFi.onEvent(WiFiEvent);
  ETH.begin();
  #endif
    
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println(""); // Line Break if Connection has been established
  
  // Print IP and MAC Address
  Serial.print("Wifi-MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Wifi-IP: ");
  Serial.println(WiFi.localIP());

  #ifdef ESP32
  Serial.print("Eth-MAC: ");
  Serial.println(ETH.macAddress());
  Serial.print("Eth-IP: ");
  Serial.println(ETH.localIP());
  #endif

  preferences.begin("base-settings", false);

  Serial.print("hasBooted: ");
  Serial.println(preferences.getBool("hasBooted", false));

  if (! preferences.getBool("hasBooted", false)) {
    // At first run, set Default Values (Location, Language, ...)
    Serial.println("Running first Setup...");
    preferences.clear();
    preferences.putBool("hasBooted", true);
    preferences.putString("location", "Default Location");
    preferences.putString("password", "Admin");
  }
  
  Serial.print("hasBooted: ");
  Serial.println(preferences.getBool("hasBooted", false));
  Serial.print("location: ");
  Serial.println(preferences.getString("location", "No Location found"));
  Serial.print("password: ");
  Serial.println(preferences.getString("password", "No Password set"));
  
  preferences.end();

  loadSettings();
  
  // Defaults to english; Change String here for other Languages
  string_location = "Location";
  string_settings = "Settings";
  string_humidity = "Humidity";
  string_temperature = "Temperature";
  string_initial_password = "Initial Password";
  string_new_location = "New Location";
  string_update_location = "Update Location";

  /*
  // German
  string_location = "Standort";
  string_settings = "Einstellungen";
  string_humidity = "Luftfeuchtigkeit";
  string_temperature = "Temperatur";
  string_initial_password = "Initial Passwort";
  string_new_location = "Neuer Standort";
  string_update_location = "Standort Aktualisieren";
  */


  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    
   char index_html[] = R"rawliteral(
<!doctype html>

<html lang="de">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">

        <title>ESP Sensor | </title>
        <meta property="og:title" content="ESP Sensor | ">
        <meta property="og:type" content="website">
        
        <link rel="icon" href="" type="image/svg+xml">
        <link rel="stylesheet" href="/style.css">

        <script src="/scripts.js"></script>
    </head>

    <body>
        <div id="main-content-div">
            <div id="menu-div">
                <span id="menu-span"><a target="_blank" href="/api">Simple-JSON-API</a> | PRTG (<a target="_blank" href="/prtg-json">JSON</a> / <a target="_blank" href="/prtg-xml">XML</a>)</span>
            </div>
            <div id="control-div">
                <div id="logo-div"><img id="main_logo" src="" alt="logo"></div>
                <div id="info-update-div"><input type="text" id="new-location" placeholder="New Location"><button id="update-location" onclick="updateLocation()">Update Location</button><br><br></div>
                <div id="info-div"></div>
            </div>
            <code>
                <div id="history-div"></div>
            </code>
        </div>
            </body>
  </html>
    )rawliteral";
    
    request->send(200, "text/html; charset=UTF-8", index_html);
  });

  
  // Route for Stylesheet
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    String style_css = "";
    // style_css += "    div{        border: 1px solid yellowgreen;    }";
    style_css += "#main-content-div {\n\twidth: 70%;\n\tmargin: 0 auto;\n\tdisplay: block;\n}\n";
    style_css += "#menu-div {\n\twidth: 100%;\n\ttext-align: right;\n\tpadding-top: 1em;\n\tpadding-bottom: 1em;\n}\n";
    style_css += "#logo-div {\n\twidth: 100%;\n\tmax-width: 100%;\n\tpadding-top: 1em;\n\tpadding-bottom: 2em;\n\tmargin-bottom: 1em;\n}\n";
    style_css += "#logo-div img {\n\twidth: 100%;\n\tmax-width: 100%;\n}\n";
    style_css += "#control-div {\n\twidth: 40%;\n\tfloat: left;\n\toverflow: hidden;\n\tdisplay: block;\n}\n";
    style_css += "#history-div {\n\twidth: 60%;\n\theight: 300px;\n\toverflow: scroll;\n\tfloat: right;\n\tbackground: lightgoldenrodyellow;\n}\n";

    request->send(200, "text/css", style_css);
  });
  
  // Route for Javascript
  server.on("/scripts.js", HTTP_GET, [](AsyncWebServerRequest *request){
    char script_js[] = R"rawliteral(
      console.debug('ESP Sensor aktiv...');

      function update_values () {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            apiResponse = JSON.parse(this.responseText);
            console.debug(apiResponse);

            document.title = "ESP Sensor | " + apiResponse["Location"];
            document.getElementById("info-div").innerHTML = "Location: " + apiResponse["Location"];
            document.getElementById("info-div").innerHTML += "<br><br>Temperature: " + apiResponse["Temperature"] + " &deg;C | Humidity: " + apiResponse["Humidity"];
            document.getElementById("history-div").innerHTML = "<br>" + new Date().toString() + " | " + apiResponse["Temperature"] + " &deg;C | Humidity: " + apiResponse["Humidity"] + document.getElementById("history-div").innerHTML;
          }
        };
        xhttp.open("GET", "/api", true);
        xhttp.send();
      }
      
      document.addEventListener("DOMContentLoaded", function() {
        setInterval(update_values, 10000);
      });

      document.addEventListener("DOMContentLoaded", function() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            apiResponse = JSON.parse(this.responseText)["result"];
            console.debug(apiResponse);

            var link = document.querySelector("link[rel~='icon']");
            link.href = apiResponse["favicon_url"];
            
            var logo = document.getElementById("main_logo");
            logo.src = apiResponse["logo_url"];
          }
        };
        xhttp.open("GET", "/settings-backend?function=getSettings", true);
        xhttp.send();
        update_values();
      });
      

      function updateLocation() {
        var url = "/settings-backend";
        var params = "password=Admin&location=" + document.getElementById("new-location").value;
        var xhr = new XMLHttpRequest();
        xhr.open("POST", url, true);
        
        //Send the proper header information along with the request
        xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
        
        xhr.send(params);
        update_values();
      }

    )rawliteral";

    request->send(200, "text/javascript; charset=UTF-8", script_js);
  });
  
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(t).c_str());
  });

  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(h).c_str());
  });

  server.on("/settings-backend", HTTP_GET, [](AsyncWebServerRequest *request){
    String tmp;
    preferences.begin("base-settings", false);
    if (request->hasParam("function", false)) {
      if (request->getParam("function")->value() == "getSettings") {
        Serial.println("Send Setting Informations");
        tmp = "{\"code\": 200, \"result\": {\"location\": \""+preferences.getString("location", "No Location found")+"\", \"favicon_url\": \""+faviconURL+"\", \"logo_url\": \""+logoURL+"\"}}";
      } else {
        Serial.println("Function not found");
        tmp = "{\"code\": 500,\"message\": \"Function not found\"}";
      }
    } else {
      Serial.println("NO parameter received");
      tmp = "{\"code\": 400,\"message\": \"No Parameters given\"}";
    }
    preferences.end();
    request->send(200, "application/json", tmp);
  });


  server.on("/settings-backend", HTTP_POST, [](AsyncWebServerRequest *request){
    String tmp;
    
    if (request->hasParam("password", true)) {
      
      String password = request->getParam("password", true)->value();
      Serial.println("password");
      Serial.println(password);
      Serial.println("admin_password");
      Serial.println(admin_password);

      if (strcmp(password.c_str(), admin_password.c_str()) == 0) {
      
        preferences.begin("base-settings", false);
        if (request->hasParam("location", true)) {
          preferences.putString("location", request->getParam("location", true)->value());
        }
        if (request->hasParam("new-password", true)) {
          preferences.putString("password", request->getParam("new-password", true)->value());
        }
        preferences.end();
        
        Serial.println("Settings updated");
        tmp = "{\"code\": 200,\"message\": \"Settings updated\"}";
      } else {
        Serial.println("Password not correct");
        tmp = "{\"code\": 500,\"message\": \"Password not correct\"}";
      }
    } else {
      Serial.println("Settings not updated");
      tmp = "{\"code\": 400,\"message\": \"No Parameters given\"}";
    }

    loadSettings();
    request->send(200, "application/json", tmp);
  });

  
  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request){

    String tmp = "{\"" + string_location + "\": \"" + location_of_device + "\", \"" + string_temperature + "\": ";
    tmp += String(t).c_str();
    tmp += ", \"" + string_humidity + "\": ";
    tmp += String(h).c_str();
    tmp += "}";
    
    request->send(200, "application/json", tmp);
  });

  server.on("/prtg-json", HTTP_GET, [](AsyncWebServerRequest *request){
    String tmp = "";

    tmp += "{\"prtg\": {\"result\": [";
    
    tmp += "{\"channel\": \"" + string_temperature + "\",";
    tmp += "\"value\": ";
    tmp += String(t).c_str();
    tmp += "},";
    
    tmp += "{\"channel\": \"" + string_humidity + "\",";
    tmp += "\"value\": ";
    tmp += String(h).c_str();
    tmp += "}";
        
    tmp += "]}}";

    request->send(200, "application/json", tmp);
  });

  
  server.on("/prtg-xml", HTTP_GET, [](AsyncWebServerRequest *request){
    String tmp = "";
    tmp += "<prtg>";

    tmp += "<result>";
    tmp += "<channel>" + string_temperature + "</channel>";
    tmp += "<float>1</float>";
    tmp += "<unit>Temperature</unit>";
    tmp += "<value>";
    tmp += String(t).c_str();
    tmp += "</value>";
    tmp += "</result>";

    tmp += "<result>";
    tmp += "<channel>" + string_humidity + "</channel>";
    tmp += "<float>1</float>";
    tmp += "<unit>Percent</unit>";
    tmp += "<value>";
    tmp += String(h).c_str();
    tmp += "</value>";
    tmp += "</result>";

    tmp += "</prtg>";

    request->send(200, "text/xml", tmp);
  });

  
  server.on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest *request){
    String tmp = "";
    
    tmp += "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' fill='currentColor' class='bi bi-activity' viewBox='0 0 16 16'>";
    tmp += "<path fill-rule='evenodd' d='M6 2a.5.5 0 0 1 .47.33L10 12.036l1.53-4.208A.5.5 0 0 1 12 7.5h3.5a.5.5 0 0 1 0 1h-3.15l-1.88 5.17a.5.5 0 0 1-.94 0L6 3.964 4.47 8.171A.5.5 0 0 1 4 8.5H.5a.5.5 0 0 1 0-1h3.15l1.88-5.17A.5.5 0 0 1 6 2Z'/>";
    tmp += "</svg>";

    request->send(200, "image/svg+xml", tmp);
  });

  
  // Start server
  server.begin();
}



 
void loop(){  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you updated the DHT values
    previousMillis = currentMillis;
    // Read temperature as Celsius (the default)
    float newT = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float newT = dht.readTemperature(true);
    // if temperature read failed, don't change t value
    if (isnan(newT)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      t = newT;
      Serial.print("t: ");
      Serial.println(t);
      //   °F = (°C × 9/5) + 32 
      // tFahrenheit = (newT * 9 / 5) + 32;
    }
    // Read Humidity
    float newH = dht.readHumidity();
    // if humidity read failed, don't change h value 
    if (isnan(newH)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      h = newH;
      Serial.print("h: ");
      Serial.println(h);
    }
  }

  if (currentMillis - previousMillisReboot >= intervalReboot) {
    ESP.restart();
  }
}
