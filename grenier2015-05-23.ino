// reference voltage for temperature measurements 
const int referenceVolts = 5;

// time to wait before measurements
const int delayTime = 1000;

const float hot = 25.0;  //temperature
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
const float voltageDivider = 0.1;


/* relay control outputs */
const int dc = 3;
const int solar1 = 5;
const int solar2 = 6;
const int fan1 = 9;
const int fan2 = 10;



void setup(){
  Serial.begin(9600);
  pinMode(dc, OUTPUT);
  pinMode(solar1, OUTPUT);
  pinMode(solar2, OUTPUT);  
  pinMode(fan1, OUTPUT);
  pinMode(fan2, OUTPUT);
  
}

float valToVolts(int val){
  return (val / 1023.0) * referenceVolts * voltageDivider;
}

float getTemp(int in){
    float volts = (analogRead(in) / 1023.0) * referenceVolts;
    return a3*volts*volts*volts + b2*volts*volts + c1*volts + d;
}

void printTemp(int in, float temp){
  Serial.print(in);
  Serial.print(":");
  Serial.print(temp);
  Serial.println('c');
}

void printVolts(int in, float volts){
  Serial.print(in);
  Serial.print(":");
  Serial.print(volts);
  Serial.println('v');
}

float readTemp(int in){
  float temp = getTemp(in);
  delay(delayTime);
  printTemp(in, temp);
  return temp;
}

float readVoltage(int in){
  float volts = valToVolts(analogRead(in));
  delay(delayTime);
  printVolts(in, volts);
  return volts;
}

void setPowerSource(int battery){
  digitalWrite(dc, battery);
  Serial.print("battery:");
  Serial.println(battery);
}

void controlFans(int fan){
  digitalWrite(fan1, fan);
  digitalWrite(fan2, fan);
  Serial.print("fan:");
  Serial.println(fan);
}

void loop(){
  float temp1 = readTemp(0);
  float temp2 = readTemp(1);
  
  float v1 = readVoltage(2);
  float v2 = readVoltage(3);
  
  int battery = LOW;
  int fan = LOW;

  if (v1 > charged and v2 > charged) {
    battery = HIGH;
  }
  setPowerSource(battery);
  
  if (temp2 > hot and temp2 > temp1){
    fan = HIGH;
  }
  controlFans(fan);
  
  delay(6*delayTime);
}
