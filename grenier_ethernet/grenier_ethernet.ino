
#include <SPI.h>
#include <Ethernet.h>

/* reference voltages */
const int referenceVolts = 5;
const float charged = 12.0;

/* resistor divider network for voltage measurement */
const float voltageDivider = 10.0;

/* timing constants */
const int measurementTime = 100; 
const unsigned long sleepTime = 60000;

/* threshold temperatures */
const float hot = 25.0;
const float extra_hot = 30.0;

/* NTC polynomial coeeficients for voltage to temperature conversion */
const float a3 = 3.296;
const float b2 = -22.378;
const float c1 =  70.951;
const float d = -49.382;

/* analog inputs */
const int t1in = 0;
const int t2in = 1;
const int v1in = 2;
const int v2in = 3;

/* relay control outputs */
const int dc = 3;
const int solar1out = 5;
const int solar2out = 6;
const int fan1out = 9;
const int fan2out = 11;
const int fan3out = 12;

/* network */
const int HTTP_PORT = 80;
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  

/* global variables */
unsigned long start_time;
unsigned long time;
unsigned long leap_time;
unsigned long elapsed = 0;
int battery_status;
int fan_status;
int extra_fan_status;

/* program start */
EthernetServer server = EthernetServer(HTTP_PORT);

void setup(){
  start_time = millis();
  pinMode(dc, OUTPUT);
  pinMode(solar1out, OUTPUT);
  pinMode(solar2out, OUTPUT);  
  pinMode(fan1out, OUTPUT);
  pinMode(fan2out, OUTPUT);
  pinMode(fan3out, OUTPUT);
  Ethernet.begin(mac);
  server.begin();
  time = millis();
}

void loop(){
  float temp1 = readTemp(t1in);
  float temp2 = readTemp(t2in);
  float volt1 = readVoltage(v1in);
  float volt2 = readVoltage(v2in);
  leap_time = millis();
  if (leap_time > time + sleepTime){
    battery_status = setPowerSource(volt1, volt2);
    fan_status = controlFans(temp1, temp2);
    extra_fan_status = controlExtraFan(temp1, temp2);
    time = leap_time;
    Ethernet.maintain(); 
  }
  elapsed = leap_time - start_time;

  EthernetClient client = server.available();
  if(client){
    boolean current_line_is_blank = true;
    while(client.connected()){
      if (client.available()){
        char c = client.read();
        if( c == '\n' && current_line_is_blank){
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html; charset=utf-8");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 60");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<html>");
          client.println("<head><title>Grenier</title></head>");
          client.println("<body><p>");
          client.println("<h1>Grenier</h1>");
          client.println("<p>température 1 :");
          client.println(temp1);
          client.println("<br />température 2 :");
          client.println(temp2);
          client.println("</p><p>panneau solaire 1 :");
          client.println(volt1);
          client.println("<br />panneau solaire 2 :");
          client.println(volt2);  
          client.println("</p><p>sur solaire :");
          client.println(battery_status);
          client.println("<br />ventilateurs :");
          client.println(fan_status);
          client.println("<br />ventilateur supplémentaire :");
          client.println(extra_fan_status);
          client.println("</p><p>fonctionne depuis :");
          client.println((elapsed / 1000.0), 0);
          client.println("s");
          client.println("</p></body></html>");
          break;
        }
        if (c == '\n'){
          current_line_is_blank = true;
        } else if (c != '\r'){
          current_line_is_blank = false;
        }
      }
    }
  delay(1);
  client.stop();
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

int setPowerSource(float v1, float v2){
  int power_level = LOW;
  if (v1 > charged and v2 > charged) {
    power_level = HIGH;
  }
  digitalWrite(dc, power_level);
  return power_level;
}

int controlFans(float temp1, float temp2){
  int fan = LOW;
  if (temp2 > hot and temp2 > temp1){
    fan = HIGH;
  }
  digitalWrite(fan1out, fan);
  digitalWrite(fan2out, fan);
  return fan;
}

int controlExtraFan(float temp1, float temp2){
  int extra_fan = LOW;
  if (temp1 > extra_hot or temp2 > extra_hot){
    extra_fan = HIGH;
  }
  digitalWrite(fan3out, extra_fan);
  return extra_fan;
}
