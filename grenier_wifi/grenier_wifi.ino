#include <SoftwareSerial.h> 
#include <SparkFunESP8266WiFi.h>
const char mySSID[] = "beaconsfield";
const char myPSK[] = "fafabebe1a1a";


/* digital outputs */
//pins 8 and 9 are used for serial communication with the esp8266
#define SOLAR_1_OUT 3
#define SOLAR_2_OUT 10
#define FAN_1_OUT 11 //set to high if temp > high
#define FAN_2_OUT 12 //set to high if temp > high
#define FAN_3_OUT 2 //set to high if solar is strong 

/* analog inputs */
#define TEMP_1_IN 0
#define TEMP_2_IN 1
#define VOLTAGE_1_IN 2
#define VOLTAGE_2_IN 3

/* threshold temperatures */
const float hot = 23.0;
const float extraHot = 27.0;

/* reference voltages */
const int referenceVolts = 5;
const float charged = 15.0;

/* resistor divider network for voltage measurement */
const float voltageDivider = 10.0;

/* timing constants */
const int measurementTime = 100; 
const unsigned long sleepTime = 60000;

/* NTC polynomial coefficients for voltage to temperature conversion */
const float a3 = 3.296;
const float b2 = -22.378;
const float c1 =  70.951;
const float d = -49.382;

/* global variables */
unsigned long startTime;
unsigned long time;
unsigned long leapTime;
unsigned long elapsed = 0;

long fanOnTime;

int sourceOneStatus;
int sourceTwoStatus;
int fanStatus = LOW;
int extraFanStatus;

float temp1;
float temp2;
float volt1;
float volt2;

const String htmlHeader = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/html; charset=utf-8\r\n"
                          "Connection: close\r\n"
                          "Refresh: 60\r\n\r\n";
                          
/* program start */
ESP8266Server server = ESP8266Server(80);

void serverSetup()
{
  // begin initializes a ESP8266Server object. It will
  // start a server on the port specified in the object's
  // constructor (in global area)
  server.begin();
  Serial.print(F("Server started! Go to "));
  Serial.println(esp8266.localIP());
  Serial.println();
}

void setup(){
  startTime = millis();
  pinMode(SOLAR_1_OUT, OUTPUT);
  pinMode(SOLAR_2_OUT, OUTPUT);  
  pinMode(FAN_1_OUT, OUTPUT);
  pinMode(FAN_2_OUT, OUTPUT);
  pinMode(FAN_3_OUT, OUTPUT);
  Serial.begin(9600);  
  initializeESP8266();
  connectESP8266();
  serverSetup();
  time = millis();
}

void loop(){
  temp1 = readTemp(TEMP_1_IN);
  temp2 = readTemp(TEMP_2_IN);
  volt1 = readVoltage(VOLTAGE_1_IN);
  volt2 = readVoltage(VOLTAGE_2_IN);
  leapTime = millis();
  if (leapTime > time + sleepTime){
    sourceOneStatus = setPowerSource(volt1, SOLAR_1_OUT);
    sourceTwoStatus = setPowerSource(volt2, SOLAR_2_OUT);
    if (fanStatus == HIGH){
         fanOnTime += (leapTime - time);
    }
    fanStatus = controlFans(temp1, temp2);
    extraFanStatus = controlExtraFan(temp1, temp2);
    time = leapTime;
  }
  elapsed = leapTime - startTime;
  ESP8266Client client = server.available(500);
  if (client) 
  {
    Serial.println(F("Client Connected!"));
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) 
    {
      if (client.available()) 
      {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) 
        {
          Serial.println(F("Sending HTML page"));
          // send a standard http response header:
          client.print(htmlHeader);
          String htmlBody;
          htmlBody  = F("<html>\n<head><title>Grenier</title></head>\n<body><h1>Grenier</h1>\n<p>Température intérieure :");
          htmlBody += temp1;
          htmlBody += F("C\n<br />Température extérieure :");
          htmlBody += temp2;
          htmlBody += F("C\n</p>\n<p>Tension sur panneau solaire 1 : ");
          htmlBody += volt1;
          htmlBody += F("V\n<br />Tension sur panneau solaire 2 : ");
          htmlBody += volt2;
          htmlBody += F("V</p>\n");
          client.print(htmlBody);
          
          htmlBody  = F("<p>Ventilateur 1 sur solaire : ");
          htmlBody += booleanToText(sourceOneStatus);
          htmlBody += F("<br />\nVentilateur 2 sur solaire : ");
          htmlBody += booleanToText(sourceTwoStatus);
          htmlBody += F("</p>\n<p>ventilateurs 1 et 2 en fonction : ");
          htmlBody += booleanToText(fanStatus);
          htmlBody += F("<br />\nVentilateur supplémentaire en fonction : ");
          htmlBody += booleanToText(extraFanStatus);
          htmlBody += F("</p>\n");
          client.print(htmlBody);
          
          htmlBody  = F("<p>Arduino en fonction depuis : ");
          htmlBody += secondsToDays(elapsed / 1000);
          htmlBody += F("</p>\n<p>Ventilation en fonction depuis : ");
          htmlBody += secondsToDays(fanOnTime / 1000);
          htmlBody += F("</p>\n</body>\n</html>");
          client.print(htmlBody);
          break;
        }
        if (c == '\n') 
        {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') 
        {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
   
    // close the connection:
    client.stop();
    Serial.println(F("Client disconnected"));
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
  int powerLevel = LOW;
  if (volts > charged) {
    powerLevel = HIGH;
  }
  digitalWrite(out, powerLevel);
  return powerLevel;
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
  if (temp1 > extraHot or temp2 > extraHot){
    extra_fan = HIGH;
  }
  digitalWrite(FAN_3_OUT, extra_fan);
  return extra_fan;
}

void initializeESP8266()
{
  int test = esp8266.begin();
  if (test != true)
  {
    Serial.println(F("Error talking to ESP8266."));
    errorLoop(test);
  }
  Serial.println(F("ESP8266 Shield Present"));
}

void connectESP8266()
{
  int retVal = esp8266.getMode();
  if (retVal != ESP8266_MODE_STA)
  { 
    retVal = esp8266.setMode(ESP8266_MODE_STA);
    if (retVal < 0)
    {
      Serial.println(F("Error setting mode."));
      errorLoop(retVal);
    }
  }
  Serial.println(F("Mode set to station"));

   retVal = esp8266.status();
  if (retVal <= 0)
  {
    Serial.print(F("Connecting to "));
    Serial.println(mySSID);
    
    retVal = esp8266.connect(mySSID, myPSK);
    if (retVal < 0)
    {
      Serial.println(F("Error connecting"));
      errorLoop(retVal);
    }
  }
}

void(* resetFunc) (void) = 0;//declare reset function at address 0

void errorLoop(int error)
{
  Serial.print(F("Error: ")); Serial.println(error);
  Serial.println(F("Will now reset the arduino."));
  Serial.println();
  delay(3000);
   resetFunc(); 
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
    char buf[35];
    n = sprintf(buf, "%d années, %d jours, %02d h %02d m %02d s", annees, jours, heures, minutes, secondes);
    return buf;
}
