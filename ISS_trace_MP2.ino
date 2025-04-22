#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* ssids[] = {"", "DJOG-cursisten", "ThirdSSID"}; // Array van SSID's
const char* passwords[] = {"", "DJOG1402", "thirdPassword"}; // Array van bijbehorende wachtwoorden

// API endpoints
const char* ipApiUrl = "http://ip-api.com/json/";
const char* issApiUrl = "http://api.open-notify.org/iss-now.json";

// LED pins
const int ledPins[] = {14, 12, 13, 15}; // D5, D6, D7, D8

// Locatiegegevens
String latitude = "53.126480", longitude = "6.405418";
ESP8266WebServer server(80); // Webserver op poort 80

#define SCREEN_WIDTH 128 // OLED display width, in pixels
//#define SCREEN_HEIGHT 64 // OLED display height, in pixels   //0.96
#define SCREEN_HEIGHT 32 // OLED display height, in pixels   //0.91

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
    Serial.begin(115200);
    for (int i = 0; i < 4; i++) {
        pinMode(ledPins[i], OUTPUT); // Initialiseer LED-pinnen
    }

    // initialize with the I2C addr 0x3C
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  

    // Clear the buffer.
    display.clearDisplay();
    // Display Inverted Text
    display.setTextColor(WHITE); // 'inverted' text
    display.setTextSize(2);
    display.setCursor(0,0);
    display.println("ISS Trace MP2");
    display.display();
    delay(2000);
    display.clearDisplay();

    connectToWiFi(); // Verbinden met Wi-Fi
    fetchLocation(); // Ophalen van locatiegegevens

    // Start de webserver routes
    server.on("/", handleRoot);
    server.on("/iss", handleISSLocation);
    server.begin(); // Start de webserver
}

void loop() {
    checkISS(); // Check de ISS positie
    server.handleClient(); // Behandel binnenkomende clienten
    delay(10000); // Controleer elke 10 seconden
}

void connectToWiFi() {
    for (int i = 0; i < 3; i++) {
        WiFi.begin(ssids[i], passwords[i]);
        Serial.print("Probeer verbinding met: ");
        Serial.println(ssids[i]);
        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            Serial.println("Verbonden met WiFi");
            return; // Verbinding geslaagd
        }
    }
    Serial.println("Verbinding met alle netwerken is mislukt.");
}

void fetchLocation() {
    WiFiClient client;
    if (client.connect("ip-api.com", 80)) {
        client.print(String("GET ") + ipApiUrl + " HTTP/1.1\r\n" +
                     "Host: ip-api.com\r\n" +
                     "Connection: close\r\n\r\n");
       
        String line;
        while (client.connected() && !client.available()) {
            delay(100); // Wacht tot er gegevens beschikbaar zijn
        }
       
        // Ontvang en parse JSON-gegevens
        while (client.available()) {
            line = client.readStringUntil('\n');
            if (line.startsWith("{")) {
                Serial.println(line); // Print JSON-respons voor debugging
                parseLocationData(line);
                break; // Breek na het verwerken van de gegevens
            }
        }
    } else {
        Serial.println("Verbinding mislukt");
    }
}

void parseLocationData(String jsonResponse) {
    int latIndex = jsonResponse.indexOf("lat");
    int lonIndex = jsonResponse.indexOf("lon");

    if (latIndex != -1) {
        latitude = jsonResponse.substring(latIndex + 5, jsonResponse.indexOf(",", latIndex));
    }
    if (lonIndex != -1) {
        longitude = jsonResponse.substring(lonIndex + 5, jsonResponse.indexOf(",", lonIndex));
    }
}

void checkISS() {
    WiFiClient client;
    if (client.connect("api.open-notify.org", 80)) {
        digitalWrite(ledPins[3], HIGH); // Zet de LED aan bij succesvolle verbinding

        client.print(String("GET ") + issApiUrl + " HTTP/1.1\r\n" +
                     "Host: api.open-notify.org\r\n" +
                     "Connection: close\r\n\r\n");
       
        String line;
        while (client.connected() && !client.available()) {
            delay(100); // Wacht tot er gegevens beschikbaar zijn
        }
       
        // Ontvang en parse ISS-gegevens
        while (client.available()) {
            line = client.readStringUntil('\n');
            if (line.startsWith("{")) {
                Serial.print("JSON-respons");
                Serial.println(line); // Print JSON-respons voor debugging
                float issLat = parseFloatFromJson(line, "latitude");
                float issLon = parseFloatFromJson(line, "longitude");
                Serial.print("latitude");
                Serial.println(issLat);
                // Bereken de afstand
                float distance = calculateDistance(issLat, issLon);
                handleDistance(distance);
                break; // Breek na het verwerken van de gegevens
            }
        }

        client.stop();
    } else {
        Serial.println("ISS Verbinding mislukt");
        digitalWrite(ledPins[3], LOW); // Zet de LED uit bij verbindingsproblemen
    }
}

void handleDistance(float distance) {
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.setTextSize(2);

    if (distance <= 500.0) {
        digitalWrite(ledPins[0], HIGH); // LED1 aan
        digitalWrite(ledPins[1], LOW); // LED2 uit
        digitalWrite(ledPins[2], LOW); // LED3 uit
        Serial.println("ISS is vlakbij! (minder dan 500 km)");
        display.clearDisplay();
        display.print("D < 500");
        display.setCursor(0,16);
        display.println(distance);
        display.display();
    } else if (distance <= 1000.0) {
        digitalWrite(ledPins[0], LOW); // LED1 uit
        digitalWrite(ledPins[1], HIGH); // LED2 aan
        digitalWrite(ledPins[2], LOW); // LED3 uit
        Serial.println("ISS is dichtbij! (tot 1000 km)");
        display.clearDisplay();
        display.print("D <= 1000");
        display.setCursor(0,16);
        display.println(distance);
        display.display();
    } else if (distance <= 5000.0) {
        digitalWrite(ledPins[0], LOW); // LED1 uit
        digitalWrite(ledPins[1], LOW); // LED2 uit
        digitalWrite(ledPins[2], HIGH); // LED3 aan
        Serial.println("ISS is dichtbij! (tot 5000 km)");
        display.clearDisplay();
        display.print("D <= 5000");
        display.setCursor(0,16);
        display.println(distance);
        display.display();
    } else {
        // Zet alle LEDs uit als we verder weg zijn
        digitalWrite(ledPins[0], LOW);
        digitalWrite(ledPins[1], LOW);
        digitalWrite(ledPins[2], LOW); // Zet de LED's uit als ISS niet in de buurt is

        // Bereken de tijd totdat de ISS weer in bereik is
        float timeToReach = calculateTimeToReachISS(distance - 500.0); // Stelt 500 km als de drempel
        if (timeToReach > 0) {
            Serial.print("ISS zal weer binnen bereik zijn in ongeveer ");
            Serial.print(timeToReach);
            Serial.println(" seconden.");
            Serial.print("Afstand tot CSS ");
            Serial.print(distance);
            Serial.println(" km.");
            display.setTextColor(WHITE , BLACK);
            display.setCursor(0,0);
            display.setTextSize(2);
            display.println("Time2reach");
            display.setCursor(0,16);
            display.setTextSize(2);
            display.println("                ");
            display.setCursor(0,16);
            display.println(timeToReach);
            display.display();
        }

    }
}

// Functie voor het berekenen van de tijd voordat de ISS weer in bereik is
float calculateTimeToReachISS(float distance) {
    const float ISS_SPEED_KM_S = 7.66; // Snelheid van de ISS in km/s
    if (distance <= 0) return 0; // Geen tijd nodig als de afstand niet positief is
    return distance / ISS_SPEED_KM_S; // Return de tijd in seconden
}

float parseFloatFromJson(String jsonResponse, String key) {
    int index = jsonResponse.indexOf(key);
    if (index != -1) {
        return jsonResponse.substring(index + 13, jsonResponse.indexOf(",", index + 13)).toFloat();
    }
    return 0.0;
}

float calculateDistance(float issLat, float issLon) {
    const float EARTH_RADIUS_KM = 6371.0;

    // Converteer breedte- en lengtegraad van graden naar radialen
    float lat1 = latitude.toFloat();
    float lon1 = longitude.toFloat();

    float dLat = (issLat - lat1) * (M_PI / 180.0);
    float dLon = (issLon - lon1) * (M_PI / 180.0);

    float a = sin(dLat / 2) * sin(dLat / 2) +
              cos(lat1 * (M_PI / 180.0)) * cos(issLat * (M_PI / 180.0)) *
              sin(dLon / 2) * sin(dLon / 2);
    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS_KM * c; // Teruggeven van de afstand in kilometers
}

// Webpagina's
void handleRoot() {
    String html = "<html><head><style>"
                  "body { background-color: black; color: white; font-family: Arial, sans-serif; }"
                  "a { color: lightblue; }"
                  "footer { color: lightgray; font-size: small; text-align: center; margin-top: 20px; }"
                  "</style></head><body>"
                  "<h1>Welkom bij de ISS Tracker!</h1>"
                  "<p><a href=\"/iss\">Bekijk ISS Locatie</a></p>"
                  "<p><a href=\"https://deepai.org/\">Hulp van DeepAI</a></p>"
                  "<p><a href=\"http://wsn.spaceflight.esa.int/iss/index_portal.php\">Bekijk de Satelliet</a></p>"
                  "<footer>FORK VAN: Idee van Secupatch Embedded</footer>"
                  "</body></html>";
    server.send(200, "text/html", html);
}

void handleISSLocation() {
    WiFiClient client;
    String issData = "";

    if (client.connect("api.open-notify.org", 80)) {
        client.print(String("GET ") + issApiUrl + " HTTP/1.1\r\n" +
                     "Host: api.open-notify.org\r\n" +
                     "Connection: close\r\n\r\n");

        // Wacht op antwoord
        while (client.connected() && !client.available()) {
            delay(100);
        }
       
        while (client.available()) {
            issData = client.readStringUntil('\n');
            if (issData.startsWith("{")) {
                break;
            }
        }
    }

    String html = "<html><head><style>"
                  "body { background-color: black; color: white; font-family: Arial, sans-serif; }"
                  "a { color: lightblue; }"
                  "footer { color: lightgray; font-size: small; text-align: center; margin-top: 20px; }"
                  "</style></head><body>"
                  "<h1>Huidige ISS Locatie</h1>"
                  "<p>" + issData + "</p>"
                  "<p><a href=\"/\">Terug</a></p>"
                  "<p><a href=\"https://deepai.org/\">Hulp van DeepAI</a></p>"
                  "<p><a href=\"http://wsn.spaceflight.esa.int/iss/index_portal.php\">Bekijk de Satelliet</a></p>"
                  "<footer>FORK VAN: Idee van Secupatch Embedded</footer>"
                  "</body></html>";
    server.send(200, "text/html", html);
}
