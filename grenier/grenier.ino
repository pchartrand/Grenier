const int referenceVolts = 5;

// time to wait before measurements
const int delayTime = 1000;
const unsigned long sleepTime = 56;

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



void setup(){
  Serial.begin(9600);
  pinMode(solar1, OUTPUT);
  pinMode(solar2, OUTPUT);  
  pinMode(fan1, OUTPUT);
  pinMode(fan2, OUTPUT);
  pinMode(fan3, OUTPUT);
  
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

void controlExtraFan(int fan){
  digitalWrite(fan3, fan);
  Serial.print("extra fan:");
  Serial.println(fan);
}

String timestampToDays(long timestamp){
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

void loop(){
  float temp1 = readTemp(0);
  float temp2 = readTemp(1);
  
  float v1 = readVoltage(2);
  float v2 = readVoltage(3);
  
  int battery = LOW;
  int fan = LOW;
  int extra_fan = LOW;
 
  if (v1 > charged and v2 > charged) {
    battery = HIGH;
  }
  
  if (temp2 > hot and temp2 > temp1){
    fan = HIGH;
  }
  
  if (temp1 > extra_hot or temp2 > extra_hot){
    extra_fan = HIGH;
  }
  
  setPowerSource(battery);
  controlFans(fan);
  controlExtraFan(extra_fan);
  Serial.println(timestampToDays(millis() / 1000));
  delay(sleepTime * delayTime);
}
