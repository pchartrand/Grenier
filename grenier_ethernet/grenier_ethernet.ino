// reference voltage for temperature measurements 
const int referenceVolts = 5;

// time to wait before measurements
const int measurementTime = 100; //time to wait after measuring a voltage
const unsigned long sleepTime = 60000; //time to wait before making a decision
const float hot = 25.0;  //threshold temperature
const float extra_hot = 30.0; //extra threshold for third fan

const float charged = 12.0;  // voltage
/* 
Assuming a third degree polynomial relationship between voltage
and temperature with a voltage divider made of an ntc 
thermistor (pullup) and a 5k6 resistor (to ground).
Voltage measured across the 5k6 resistor.

*/

const float a3 = 3.296;
const float b2 = -22.378;
const float c1 =  70.951;
const float d = -49.382;

// resistor divider network for voltage measurement
const float voltageDivider = 10.0;


/* relay control outputs */
const int dc = 3;
const int solar1 = 5;
const int solar2 = 6;
const int fan1 = 9;
const int fan2 = 10;
const int fan3 = 3;

unsigned long time;
unsigned long leap_time;
unsigned long start_time;
unsigned long elapsed = 0;
unsigned long cycles = 0;

#include <SPI.h>
#include <Ethernet.h>
const int GRENIER_PORT = 80;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  
EthernetServer server = EthernetServer(GRENIER_PORT);

int battery;
int fan;
int extra_fan;
    
void setup(){
  start_time = millis();
  pinMode(dc, OUTPUT);
  pinMode(solar1, OUTPUT);
  pinMode(solar2, OUTPUT);  
  pinMode(fan1, OUTPUT);
  pinMode(fan2, OUTPUT);
  pinMode(fan3, OUTPUT);
  Ethernet.begin(mac);
  server.begin();
  time = millis();
}

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
  int battery = LOW;
  if (v1 > charged and v2 > charged) {
    battery = HIGH;
  }
  digitalWrite(dc, battery);
  return battery;
}

int controlFans(float temp1, float temp2){
  int fan = LOW;
  if (temp2 > hot and temp2 > temp1){
    fan = HIGH;
  }
  digitalWrite(fan1, fan);
  digitalWrite(fan2, fan);
  return fan;
}

int controlExtraFan(float temp1, float temp2){
  int extra_fan = LOW;
  if (temp1 > extra_hot or temp2 > extra_hot){
    extra_fan = HIGH;
  }
  digitalWrite(fan3, extra_fan);
  return extra_fan;
}

void loop(){
  float temp1 = readTemp(0);
  float temp2 = readTemp(1);
  float v1 = readVoltage(2);
  float v2 = readVoltage(3);
  leap_time = millis();
  if (leap_time > time + sleepTime){
    battery = setPowerSource(v1, v2);
    fan = controlFans(temp1, temp2);
    extra_fan = controlExtraFan(temp1, temp2);
    cycles += 1;
    time = leap_time;
  }
  elapsed = leap_time - start_time;

  EthernetClient client = server.available();
  if(client){
    boolean current_line_is_blank = true;
    while(client.connected()){
      if (client.available()){
        char c = client.read();
        if( c == '\n' && current_line_is_blank){
          //client.println("GRENIER/1.0 200 OK");
          //client.println("Content-Type: text/plain");
          client.println();
          client.print("temp1:");
          client.println(temp1);
          client.print("temp2:");
          client.println(temp2);
          client.print("v1:");
          client.println(v1);
          client.print("v2:");
          client.println(v2);  
          client.print("battery:");
          client.println(battery);
          client.print("fan:");
          client.println(fan);
          client.print("extra fan:");
          client.println(fan);
          client.print("cycles;");
          client.println(cycles);
          client.print("running time(ms):");
          client.println(elapsed);
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
