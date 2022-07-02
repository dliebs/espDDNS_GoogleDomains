/** Google Domains Dynamic DNS Updater V1.0
	Checks for current IP from https://domains.google.com/checkip
	Makes an HTTPS API Request https://support.google.com/domains/answer/6147083#zippy=%2Cuse-the-api-to-update-your-dynamic-dns-record
	Based on https://github.com/ayushsharma82/EasyDDNS
	Added support for HTTPS https://arduino-esp8266.readthedocs.io/en/2.4.0/esp8266wifi/client-secure-examples.html
	Also thanks to https://randomnerdtutorials.com/esp8266-nodemcu-http-get-post-arduino/
	
	Future Plans:
	  - Add other DDNS services
	  - HTTPS Server Verification (Oops kinda skipped that... Do I need to check the server identity for it to work? No. Should I? I guessssss)
**/


/**  Libraries  **/

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>


/**  User Variables  **/

// WiFi Credentials and Desired Hostname
const char * wifi_ssid = "YOUR_WIFI_SSID_HERE";
const char * wifi_password = "YOUR_WIFI_PASS_HERE";
const char * wifi_hostname = "espDDNS";

// Subdomain and Generated Credentials
const char* ddns_subdomain = "YOURSUBDOMAIN.YOURDOMAIN.COM";
String ddns_username = "DNS_API_USERNAME";
String ddns_password = "DNS_API_PASSWORD";

// Update Interval
const int intervalMins = 15;


/**  Program Variables  **/

// DDNS Info
const char* ddns_server = "domains.google.com";
String update_url = "https://" + ddns_username + ":" + ddns_password + "@" + ddns_server + "/nic/update?hostname=" + ddns_subdomain + "&myip=";  // Update URL for Google, modify this string for other services
bool needsUpdate = false;  // Do not update by default
String ddns_ip = ""; // Placeholder for IP address

// Timekeeping
unsigned long previousMillis = 0;
const long interval = 1000 * 60 * intervalMins;


/**  Main Program  **/

void setup() {
  //Serial.begin(115200);                 // Serial can be disabled
  connectWiFi();
  readDDNSIP();
  getNewIP();
  ddnsUpdate();
  Serial.println("DDNS Updater will run again in " + (String)intervalMins + " minutes\r\n");
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      readDDNSIP();
      getNewIP();
      ddnsUpdate();
    }
    else {
      Serial.println("WiFi disconnected, attempting to reconnect");
      connectWiFi();
      readDDNSIP();
      getNewIP();
      ddnsUpdate();
    }

    Serial.println("DDNS Updater will run again in " + (String)intervalMins + " minutes\r\n");
  }
  // Let the ESP8266 do its thing
  yield();
}

// Connect to WiFi, 10 second timeout
void connectWiFi() {
  Serial.print("\r\n\r\nConnecting to " + (String)wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(wifi_hostname);
  WiFi.begin(wifi_ssid, wifi_password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) <= 10000) // Timeout WiFi connection attempt after 10 sec
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  
  // Report WiFi connection status
  if (WiFi.status() == WL_CONNECTED) {
	Serial.println("Connected to WiFi network with IP Address: " + WiFi.localIP().toString() + "\r\n");
  }
  else {
	Serial.println("WiFi failed to connect.");
  }
}

// Reads the IP address of ddns_subdomain and writes the value to ddns_ip as a String
void readDDNSIP () {
  Serial.println("\r\n\r\nReading current IP of " + (String)ddns_subdomain + "");
  IPAddress result;
  int err = WiFi.hostByName(ddns_subdomain, result);
  if (err == 1) {
    Serial.print("Current IP address for " + (String)ddns_subdomain + " is ");
    Serial.println(result);
    ddns_ip = result.toString();
  }
  else {
    Serial.print("Error Code: ");
    Serial.println(err);
    Serial.print("Result: ");
    Serial.println(result);
  }
  Serial.println("");
}

// Reads the current IP address from Googles resolver and compares it to ddns_ip
// If it does not match, sets the needsUpdate flag to true and writes the new IP to ddns_ip
void getNewIP() {
  Serial.println("getNewIP() - Retrieving external IP from Google");
  WiFiClientSecure client;
  client.setInsecure(); // Lol who needs to verify server identities
  HTTPClient http;
  http.begin(client, "https://domains.google.com/checkip");
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String newIP = http.getString();
      Serial.println("External IP: " + newIP);
      Serial.println("Current DDNS IP: " + ddns_ip);
      if (newIP == ddns_ip) {
        Serial.println("External IP matches Current IP for " + (String)ddns_subdomain + ", no update needed");
        needsUpdate = false;
      }
      else {
        Serial.println("External IP does not match Current IP for " + (String)ddns_subdomain + ", update needed");
        ddns_ip = newIP;
        needsUpdate = true;
      }
    }
  }
  else {
    Serial.println("HTTP Error Code: " + httpCode);
  }
  http.end();
  Serial.println("");
}

// The update code
void ddnsUpdate() {
  if (needsUpdate == true) {
    Serial.println("Attempting to update " + (String)ddns_subdomain + " to " + ddns_ip);

    WiFiClientSecure client;
    HTTPClient http;

    update_url = update_url + ddns_ip; // Add IP address to the end of the API URL
    Serial.println("Update URL: " + update_url);

    Serial.println("Connecting to Server");
    client.setInsecure();
    client.connect(ddns_server, 443);

    // Your Domain name with URL path or IP address with path
    Serial.println("Updating IP");
    http.begin(client, update_url);

    // Send HTTP GET request
    int httpCode = http.GET();

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String ddnsOutput = http.getString();
        Serial.println("Success!");
        Serial.print("HTTP Response Code: ");
        Serial.println(httpCode);
        Serial.println("Server Output: " + ddnsOutput);
        Serial.println("");
      }
    }
    else {
      Serial.println("Failure to update DDNS IP :(");
      Serial.println("HTTP Error Code: " + httpCode);
      Serial.println("");
    }
    // Free resources
    http.end();
    Serial.println("Update Completed Successfully!");
  }
  else {
    Serial.println("No update needed, not updating :)");
  }
}
