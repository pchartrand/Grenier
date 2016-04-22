

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

void setup(){
  start_time = millis();
  pinMode(SOLAR_1_OUT, OUTPUT);
  pinMode(SOLAR_2_OUT, OUTPUT);  
  pinMode(FAN_1_OUT, OUTPUT);
  pinMode(FAN_2_OUT, OUTPUT);
  pinMode(FAN_3_OUT, OUTPUT);
  
  Serial.begin(115200);  
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
