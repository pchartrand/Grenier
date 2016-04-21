
#include <SPI.h>
#include <Adafruit_CC3000.h>
#include "utility/debug.h"
#include "utility/socket.h"
// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11

/* digital outputs */
#define SOLAR_1_OUT 6
#define SOLAR_2_OUT 7
#define FAN_1_OUT 8  //set to high if temp > high
#define FAN_2_OUT 9 //set to high if temp > high
#define FAN_3_OUT 4 //set to high if solar is strong 

/* analog inputs */
#define TEMP_1_IN 0
#define TEMP_2_IN 1
#define VOLTAGE_1_IN 2
#define VOLTAGE_2_IN 3

Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed

#define WLAN_SSID       "beaconsfield"   // cannot be longer than 32 characters!
#define WLAN_PASS       "fafabebe1a1a"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define LISTEN_PORT           80
#define MAX_ACTION            10      // Maximum length of the HTTP action that can be parsed.

#define MAX_PATH              64      // Maximum length of the HTTP request path that can be parsed.
                                      // There isn't much memory available so keep this short!

#define BUFFER_SIZE           MAX_ACTION + MAX_PATH + 20  // Size of buffer for incoming request data.
                                                          // Since only the first line is parsed this
                                                          // needs to be as large as the maximum action
                                                          // and path plus a little for whitespace and
                                                          // HTTP version.

#define TIMEOUT_MS            500    // Amount of time in milliseconds to wait for
                                     // an incoming request to finish.  Don't set this
                                     // too high or your server could be slow to respond.


/* reference voltages */
const int referenceVolts = 5;
const float charged = 12.0;

/* resistor divider network for voltage measurement */
const float voltageDivider = 10.0;

/* timing constants */
const int measurementTime = 100; 
const unsigned long sleepTime = 60000;

/* threshold temperatures */
const float hot = 24.0;
const float extra_hot = 30.0;

/* NTC polynomial coefficients for voltage to temperature conversion */
const float a3 = 3.296;
const float b2 = -22.378;
const float c1 =  70.951;
const float d = -49.382;

/* global variables */
unsigned long start_time;
unsigned long time;
unsigned long leap_time;
unsigned long elapsed = 0;
int source_one_status;
int source_two_status;
int fan_status;
int extra_fan_status;

float temp1;
float temp2;
float volt1;
float volt2;

/* program start */
Adafruit_CC3000_Server httpServer(LISTEN_PORT);
uint8_t buffer[BUFFER_SIZE+1];
int bufindex = 0;
char action[MAX_ACTION+1];
char path[MAX_PATH+1];

void setup(){
  start_time = millis();
  pinMode(SOLAR_1_OUT, OUTPUT);
  pinMode(SOLAR_2_OUT, OUTPUT);  
  pinMode(FAN_1_OUT, OUTPUT);
  pinMode(FAN_2_OUT, OUTPUT);
  pinMode(FAN_3_OUT, OUTPUT);
  
  Serial.begin(115200);
  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  Serial.print(F("\nAttempting to connect to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
   
  Serial.println(F("Connected!"));
  
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

  // Display the IP address DNS, Gateway, etc.
  while (! displayConnectionDetails()) {
    delay(1000);
  }
  
  httpServer.begin();
  Serial.println(F("Listening for connections..."));
  time = millis();
}

void loop(){
  temp1 = readTemp(TEMP_1_IN);
  temp2 = readTemp(TEMP_2_IN);
  volt1 = readVoltage(VOLTAGE_1_IN);
  volt2 = readVoltage(VOLTAGE_2_IN);
  leap_time = millis();
  if (leap_time > time + sleepTime){
    source_one_status = setPowerSource(volt1, SOLAR_1_OUT);
    source_two_status = setPowerSource(volt2, SOLAR_2_OUT);
    fan_status = controlFans(temp1, temp2);
    extra_fan_status = controlExtraFan(temp1, temp2);
    time = leap_time;
  }
  elapsed = leap_time - start_time;
  Serial.print(F("Free RAM: ")); Serial.println(getFreeRam(), DEC);
  Adafruit_CC3000_ClientRef client = httpServer.available();
  if(client){
    Serial.println(F("Client connected."));
    // Process this request until it completes or times out.
    // Note that this is explicitly limited to handling one request at a time!

    // Clear the incoming data buffer and point to the beginning of it.
    bufindex = 0;
    memset(&buffer, 0, sizeof(buffer));
    
    // Clear action and path strings.
    memset(&action, 0, sizeof(action));
    memset(&path,   0, sizeof(path));

    // Set a timeout for reading all the incoming data.
    unsigned long endtime = millis() + TIMEOUT_MS;
    
    // Read all the incoming data until it can be parsed or the timeout expires.
    bool parsed = false;
    while (!parsed && (millis() < endtime) && (bufindex < BUFFER_SIZE)) {
      if (client.available()) {
        buffer[bufindex++] = client.read();
      }
      parsed = parseRequest(buffer, bufindex, action, path);
    }
    // Handle the request if it was parsed.
    if (parsed) {
      Serial.println(F("Processing request"));
      Serial.print(F("Action: ")); Serial.println(action);
      Serial.print(F("Path: ")); Serial.println(path);
      // Check the action to see if it was a GET request.
      if (strcmp(action, "GET") == 0) {
        client.fastrprintln(F("HTTP/1.1 200 OK"));
        printStatus(client);
      }
      else {
        client.fastrprintln(F("HTTP/1.1 405 Method Not Allowed"));
        client.fastrprintln(F(""));
      }
    }

    // Wait a short period to make sure the response had time to send before
    // the connection is closed (the CC3000 sends data asyncronously).
    delay(100);

    // Close the connection when done.
    Serial.println(F("Client disconnected"));
    client.close();
  }
}
/* functions */

float valToVolts(int val){
  return (val / 1023.0) * referenceVolts * voltageDivider;
}

float getTemp(int in){
    float volts = (analogRead(in) / 1023.0) * referenceVolts;
    return a3*volts*volts*volts + b2*volts*volts + c1*volts + d;
}

float readTemp(int in){
  float temp = getTemp(in);
  delay(measurementTime);
  return temp;
}

float readVoltage(int in){
  float volts = valToVolts(analogRead(in));
  delay(measurementTime);
  return volts;
}

int setPowerSource(float volts, int out){
  int power_level = LOW;
  if (volts > charged) {
    power_level = HIGH;
  }
  digitalWrite(out, power_level);
  return power_level;
}

int controlFans(float temp1, float temp2){
  int fan = LOW;
  if (temp2 > hot and temp2 > temp1){
    fan = HIGH;
  }
  digitalWrite(FAN_1_OUT, fan);
  digitalWrite(FAN_2_OUT, fan);
  return fan;
}

int controlExtraFan(float temp1, float temp2){
  int extra_fan = LOW;
  if (temp1 > extra_hot or temp2 > extra_hot){
    extra_fan = HIGH;
  }
  digitalWrite(FAN_3_OUT, extra_fan);
  return extra_fan;
}

bool parseRequest(uint8_t* buf, int bufSize, char* action, char* path) {
  // Check if the request ends with \r\n to signal end of first line.
  if (bufSize < 2)
    return false;
  if (buf[bufSize-2] == '\r' && buf[bufSize-1] == '\n') {
    parseFirstLine((char*)buf, action, path);
    return true;
  }
  return false;
}

void parseFirstLine(char* line, char* action, char* path) {
  // Parse first word up to whitespace as action.
  char* lineaction = strtok(line, " ");
  if (lineaction != NULL)
    strncpy(action, lineaction, MAX_ACTION);
  // Parse second word up to whitespace as path.
  char* linepath = strtok(NULL, " ");
  if (linepath != NULL)
    strncpy(path, linepath, MAX_PATH);
}

String booleanToText(int value){
  if(value == 1){
    return "oui";
  }else{
    return "non";
  }
}

String secondsToDays(long timestamp){
    int secondes = timestamp % 60;
    long ttl_minutes = (timestamp - secondes) / 60;
    int minutes = ttl_minutes % 60;
    long ttl_heures = (ttl_minutes - minutes) / 60;
    int heures = ttl_heures % 24;
    long ttl_jours = (ttl_heures - heures) / 24;
    int jours = ttl_jours % 365;
    int annees = (ttl_jours - jours) / 365;
    int n;
    char buf[30];
    n = sprintf(buf, "%d années, %d jours, %02d:%02d:%02d", annees, jours, heures, minutes, secondes);
    return buf;
}

void printStatus(Adafruit_CC3000_ClientRef client){
    client.fastrprintln(F("Content-Type: text/html; charset=utf-8\r\nConnection: close\r\nRefresh: 60\r\n"));
    client.fastrprintln(F("<html>\n<head><title>Grenier</title></head>\n"));
    client.fastrprintln(F("<body><h1>Grenier</h1>\n"));
    client.fastrprintln(F("<p>température 1 :"));
    client.println(temp1);
    client.fastrprintln(F("C<br />température 2 :"));
    client.println(temp2);
    client.fastrprintln(F("C</p>\n<p>panneau solaire 1 :"));
    client.println(volt1);
    client.fastrprintln(F("V<br />panneau solaire 2 :"));
    client.println(volt2);
    client.fastrprintln(F("V</p>\n<p>ventilateurs 1 et 2 en fonction:"));
    client.println(booleanToText(fan_status));
    client.fastrprintln(F("<br />ventilateur supplémentaire en fonction:"));
    client.println(booleanToText(extra_fan_status));
    client.fastrprintln(F("</p>\n<p>arduino en fonction depuis :"));
    client.println(secondsToDays(elapsed / 1000));
    client.fastrprintln(F("secondes.</p></body></html>"));
}

bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}
