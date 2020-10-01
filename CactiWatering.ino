
/* The Plant Waterer


The goal of this project is to create a small irrigation system for my cacti. The end product will be a fully autonomous smart watering system
The system will periodically 'awake' take a soil moisture reading, water when needed, doing so to a preset amount, check that the plants were watered
then put itself back to sleep. 

Hardware: 
  1 Solenoid valve with 1/2" outlets
  1 hall flowmeter (sadly its 3/8", nothing some flex tubing and pipe clamps 
  wont fix)
  1 1N4007 Diode Rectifier
  1 TIP120 Darlington Transistor
  1 external power source for the solenoid
  1 Arduino uno R3 (to hopefully be replaced by a nano or pro mini)
  Whatever tubing and spliters and fitting are necessary
  2 soil hygrometers
  1 coin cell baterry RTC module DS3231
  
Libraries (very important to use this specific RTC library and no other)
  Low Power : https://github.com/rocketscream/Low-Power
  RTC : https://github.com/MrAlvin/RTClib

 
 Thanks to Bruce Emerson for the helpful suggestions and inspiration,
 Miss Morgan for gifting me my lovely Cacti which I am determined to not kill,
 The cave pearl project, specifically the use of the RTC module, check it out:
 https://thecavepearlproject.org/2015/12/22/arduino-uno-based-data-logger-with-no-soldering/
 and as always the many forums and tutorials in the arduino community.

Final Notes and Takeaways:
As far as a practical solution to the automated watering of my desk plants this project falls short. Or perhaps
the more accurate statement is that its overkill. The err is mostly that of hardware and the physical design. 
The sizing of the modules, specifically that of the flow meter and solenoid valve are far too large for the application.
Demanding 5/8 tubing where 1/8 in was plenty. The other large error was in not checking the requirments of the valve before 
purchasing. The 3 psi head pressure was not possible to meet with just gravity alone. (I pressurized the container later on)
This project would be better suited to a larger scale need, a backyard garden for example. 

However, the project to me is still a major success. The lessons learned were invaluable
and the following code is, in my opinion, a modest step up from my past endeavors. 
I learned much in the coding and its application, as well as a valuable lesson 
in the necessity of proper layout and planning on the physical side. Thanks for reading.

  -Ben Ruland

*/

// Libraries necessary for the Alarm module
#include <Wire.h>
#include "LowPower.h"  //these two can be found in the github links above. 
#include <RTClib.h>   // do not use the library suggested by arduino, see link above. 


RTC_DS3231 Clock; 

//Creates RTC object in the code
//varibales for reading the RTC time & handling the INT(1)
#define DS3231_I2C_ADDRESS 0x68
int RTC_INTERRUPT_PIN = 3;  // typically its pin 2 however we are utilizing this for the flowmeter.
                            //Extra care will be needed to make sure the interrupts do not conflict with each other. 
byte Alarmhour;
byte Alarmminute;
byte Alarmday;
int analogPin = 3;

// It is necessary to set up your RTC module before running the first time
// Here is a link to doing so https://www.instructables.com/id/Simple-Guide-to-Setting-Time-on-a-DS3231DS3107DS13/

char CycleTimeStamp [ ] = "000/00/00,00:00"; //16 ascii characters (no seconds)
//#define SampleIntervalHours 8 // adapted this to hours as I will be looking to check 4 times a day
//will alter this for testing purposes
#define SampleIntervalMinutes 1 

volatile boolean clockInterrupt = false; 
//A flag, trips to true when RTC interrupt is executed. 


 const char codebuild[] PROGMEM = __FILE__;
 const char header [] PROGMEM = "Timestamp,RTC temp(C),Analog(A0)"; //including in case its needed, dont think it is



int solenoidPin = 9; //solenoid actuated by pin 9
int flowPin = 2; //input flow meter
unsigned int water_amount = 0; //set how much water is to be delivered in mL. (currently 1/2 glass)
unsigned long totalFlow ; //creates a variable to hold how much water has been dispersed in mL
volatile byte count; //ensures count updates properly during interupt

float flowRate;
float calibrationFactor = 4.5;
unsigned int flow;
unsigned long oldTime;

//will need to set up indicator lights here for debugging.

void setup() {
  
  pinMode(solenoidPin, OUTPUT);   // initializes our solenoid pin
  pinMode (flowPin, INPUT);
  attachInterrupt(0,pulseCounter,FALLING);
  pinMode(RTC_INTERRUPT_PIN,INPUT_PULLUP); // RTC alarms low, pullup
  
  Serial.begin(9600);   // begins serial communications
  Serial.println("At least we made it this far");
  Wire.begin();         //starts I2C interface for RTC (used for attached sensors to data log)
  Clock.begin();          //Starts RTC
  
  count = 0;
  flowRate = 0.0;
  flow = 0;
  totalFlow = 0;
  oldTime = 0;
  //Clock.adjust(DateTime(__DATE__, __TIME__));
  //Clock.adjust(DateTime(2020, 8, 12, 17, 14, 0));
  //check RTC status
  clearClockTrigger();  // stops RTC from holding interrupt low if system reset occured
  Clock.turnOffAlarm(1);
  DateTime now = Clock.now();
  sprintf(CycleTimeStamp, "%04d/%02d/%02d %02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute());
  //this creates a char formatted for yyyy/mm/dd/ hh:mm and fills it with current date
  
  Serial.println("intitializing");
  }

 //initialize any test lights here
 


    // main loop
void loop() {
  
  // This part reads the time and disables the RTC alarm
  DateTime now = Clock.now(); // reads the time from the RTC
  //sprintf(CycleTimeStamp, "%04d/%02d/%02d %02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute());
  //this creates a char formatted for yyyy/mm/dd/ hh:mm and fills it with current date
  
  if (clockInterrupt) {
    if(Clock.checkIfAlarm(1)){
      Clock.turnOffAlarm(1);     //then turn it off
    }
  }
  Serial.print("RTC Alarm on INT-1 triggered at ");
  Serial.println(CycleTimeStamp);
  clockInterrupt = false;
  

  //check moisture level of soil returns true for under 15% and false over
  
  if (plant_moisture() == true){
    delay(1000);
    Serial.println("Watering the Plants now");
    actuator() ;
    Serial.println("Process has finished, watering complete, turning off now");
}
  else {
    Serial.println("The plants do not need watering, shutting down now");
  }
totalFlow=0; //resets flow count for next time
delay(1000);
//place any checks, logs, or subroutines here

//set the next alarm time
Serial.print("Setting alarm for ");
Serial.print(SampleIntervalMinutes);
Serial.println("minutes");
delay(1000);

Alarmhour = now.hour(); //+SampleIntervalHours;
Alarmminute = now.minute() +SampleIntervalMinutes;
Alarmday = now.day();

//Serial.println(Alarmhour);
//Serial.println(Alarmminute);
//Serial.println(Alarmday);
//check for roll-overs
if (Alarmminute >59) {
  Alarmminute = 0;
  Alarmhour = Alarmhour + 1;
  if (Alarmhour > 23) {
    Alarmhour = 0;
    //once per day code goes here, will execute on 24 hr roll over
    //great place to setup a data log for soil moisture
  }
}

//set alarm
delay(1000);
Clock.setAlarm1Simple(Alarmhour,Alarmminute);
Clock.turnOnAlarm(1);
if (Clock.checkAlarmEnabled(1)){ //checks alarm enabled, for debugging purposes
  Serial.println("RTC Alarm Enabled");
}
else {
  Serial.print("error setting alarm");
}
delay(1000);
//here is where i would write to led for debugging

//enables the interrupt on pin 3 for the alarm
attachInterrupt(digitalPinToInterrupt(3),rtcISR, LOW);
//power down state 
Serial.println("Powering down");
delay(1000);
LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_ON);
//Processor Starts HERE after RTC wakes it up
Serial.print("I have awoken, fear me!!"); //hehe
detachInterrupt(digitalPinToInterrupt(3));
//here we would write more leds for debugging

// ============== End of Main Loop =============

}


// Here we have a list of functions, things are a little embedded here,

    // check the plant moisture and returns true if it needs watering
boolean plant_moisture() { 
  int Moisture = analogRead(A0);
  Moisture = map(Moisture,1023,210,0,100);
  Serial.print("Soil moisture is ");
  Serial.print(Moisture);
  Serial.println("%");
  if (Moisture < 15) { // This will be 900 out of 1023, or 15% moisture content. Seems reasonable
    return true;
  }
 else {
  return false;
 }
}


    //runs solenoid while the return from the flowmeter is less then predetermined amount.
void actuator () {
  digitalWrite(solenoidPin,HIGH);
  Serial.println("turning actuator");
  while (totalFlow < water_amount) {   //the hope is this will wait and continue to return the totalFlow
    flow_counter();
  }
 digitalWrite(solenoidPin,LOW);
 Serial.println("Done Watering");
 detachInterrupt(0);
}

    // this little bit from instructables hopefully gives us a running count of our flow.
void flow_counter () {
  if((millis() - oldTime) > 1000){
    
   detachInterrupt(0); //this stops the interrupt to do calculations
   flowRate = ((1000.0 / (millis()-oldTime)) * count) / calibrationFactor;
    // converting the ticks to litres per minute while accounting for any variation in the actual second
   oldTime = millis();
   flow = (flowRate / 60) *1000;
   totalFlow += flow;
   count = 0;
   attachInterrupt(0,pulseCounter,FALLING);
   Serial.print("This is the total amount watered: ");
   Serial.print(totalFlow);
   Serial.println("mL"); 
  }
}

//counter for the flow meter pulses
void pulseCounter() {
  count++;
}

  //interrupt subroutine that exectures when RTC alarm goes off
void rtcISR () {
  clockInterrupt = true;
}

  //nifty bit that clears our trigger, copied and pasted this piece
void clearClockTrigger() // from http://forum.arduino.cc/index.php?topic=109062.0
{
  byte bytebuffer1=0;
  Wire.beginTransmission(0x68);   //Tell devices on the bus we are talking to the DS3231
  Wire.write(0x0F);               //Tell the device which address we want to read or write
  Wire.endTransmission();         //Before you can write to and clear the alarm flag you have to read the flag first!
  Wire.requestFrom(0x68,1);       //Read one byte
  bytebuffer1=Wire.read();        //In this example we are not interest in actually using the bye
  Wire.beginTransmission(0x68);   //Tell devices on the bus we are talking to the DS3231 
  Wire.write(0x0F);               //Status Register: Bit 3: zero disables 32kHz, Bit 7: zero enables the main oscilator
  Wire.write(0b00000000);         //Write the byte.  //Bit1: zero clears Alarm 2 Flag (A2F), Bit 0: zero clears Alarm 1 Flag (A1F)
  Wire.endTransmission();
  clockInterrupt=false;           //Finally clear the flag we use to indicate the trigger occurred
}
