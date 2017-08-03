/*****************************************************************************************************************
 * 
 */

#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

// A simple data logger for the Arduino analog pins

uint32_t syncTime = 0; // time of last sync()

#define ECHO_TO_SERIAL   1 // echo data to serial port
#define WAIT_TO_START    0 // Wait for serial input in setup()

// the digital pins that connect to the LEDs
#define redLEDpin 2
#define greenLEDpin 3

// The analog pins that connect to the sensors
#define tempPin 0                // analog 1
#define BANDGAPREF 14            // special indicator that we want to measure the bandgap

//#define aref_voltage 3.3         // we tie 3.3V to ARef and measure it with a multimeter!
#define bandgap_voltage 1.1      // this is not super guaranteed but its not -too- off

static const int SamplePeriod = 1000;							// Time in milliseconds between acquires (also affects logging)
static const int SavePeriod = 10000;							// Time in milliseconds between saves (should be greater than SamplePeriod, larger values result in faster operation)

static const float thermocouple_voltage = 1.25;
static const float thermocouple_divider = 0.005; 

static const int LM1971_Byte_0 = 0;     // Sets Byte 0 to always be 0 since the LM1971m is a mono device and does not need channel select
static const int LM1971_Byte_1 = 0;	{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
  63 // 63 is MUTE for the LM1971m
};


RTC_PCF8523 RTC; 													// Data Logger Shield Real Time Clock Object 

const int SD_Select = 10; 											// SD Card Select Pin (Set by shield but can be changed if so required refer to data sheets)
const int VC_Select = ; 												// Volume Controller Select Pin

File TempLogFile;														// File object for the log file

void error(char *str)
{
  Serial.print("error: ");
  Serial.println(str);
  
  // red LED indicates error
  digitalWrite(redLEDpin, HIGH);

  while(1);
}

void setup(void)
{
  Serial.begin(9600);
  Serial.println();
  
  // use debugging LEDs
  pinMode(redLEDpin, OUTPUT);
  pinMode(greenLEDpin, OUTPUT);
  
#if WAIT_TO_START
  Serial.println("Type any character to start");
  while (!Serial.available());
#endif //WAIT_TO_START

  // initialize the SD card
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  
  // see if the card is present and can be initialized:
  if (!SD.begin(SD_Select)) {
    error("Card failed, or not present");
  }
  Serial.println("card initialized.");
  
  // create a new file
  char filename[] = "LOGGER00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i/10 + '0';
    filename[7] = i%10 + '0';
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      TempLogFile = SD.open(filename, FILE_WRITE); 
      break;  // leave the loop!
    }
  }
  
  if (! TempLogFile) {
    error("couldnt create file");
  }
  
  Serial.print("Logging to: ");
  Serial.println(filename);

  // connect to RTC
  Wire.begin();  
  if (!RTC.begin()) {
    TempLogFile.println("RTC failed");
#if ECHO_TO_SERIAL
    Serial.println("RTC failed");
#endif  //ECHO_TO_SERIAL
  }
  

  TempLogFile.println("millis,stamp,datetime,light,temp,vcc");    
#if ECHO_TO_SERIAL
  Serial.println("millis,stamp,datetime,light,temp,vcc");
#endif //ECHO_TO_SERIAL
 
  // If you want to set the aref to something other than 5v
  analogReference(EXTERNAL);
}

void loop(void)
{
  DateTime now;

  // delay for the amount of time we want between readings
  delay((SamplePeriod -1) - (millis() % SamplePeriod));
  
  digitalWrite(greenLEDpin, HIGH);
  
  // log milliseconds since starting
  uint32_t m = millis();
  TempLogFile.print(m);           // milliseconds since start
  TempLogFile.print(", ");    
#if ECHO_TO_SERIAL
  Serial.print(m);         // milliseconds since start
  Serial.print(", ");  
#endif

  // fetch the time
  now = RTC.now();
  // log time
  TempLogFile.print(now.unixtime()); // seconds since 1/1/1970
  TempLogFile.print(", ");
  TempLogFile.print('"');
  TempLogFile.print(now.year(), DEC);
  TempLogFile.print("/");
  TempLogFile.print(now.month(), DEC);
  TempLogFile.print("/");
  TempLogFile.print(now.day(), DEC);
  TempLogFile.print(" ");
  TempLogFile.print(now.hour(), DEC);
  TempLogFile.print(":");
  TempLogFile.print(now.minute(), DEC);
  TempLogFile.print(":");
  TempLogFile.print(now.second(), DEC);
  TempLogFile.print('"');
#if ECHO_TO_SERIAL
  Serial.print(now.unixtime()); // seconds since 1/1/1970
  Serial.print(", ");
  Serial.print('"');
  Serial.print(now.year(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);
  Serial.print(":");
  Serial.print(now.second(), DEC);
  Serial.print('"');
#endif //ECHO_TO_SERIALaaaaa

//  analogRead(photocellPin);
//  delay(10); 
//  int photocellReading = analogRead(photocellPin);  
  
  analogRead(tempPin); 
  delay(10);
  int tempReading = analogRead(tempPin);    

  float tempReadingAdjusted = tempReading * 0.0049;

  TempLogFile.print(", ");
  TempLogFile.print(tempReading);
  #if ECHO_TO_SERIAL
    Serial.print(", ");   
    Serial.print(tempReading);
  #endif // ECHO_TO_SERIAL
  
  // converting that reading to voltage, for 3.3v arduino use 3.3, for 5.0, use 5.0
  //float voltage = tempReading * aref_voltage / 1024;  
  float temperatureC = (tempReadingAdjusted - thermocouple_voltage)/thermocouple_divider;
  float temperatureF = (temperatureC * 9 / 5) + 32;
  
  //TempLogFile.print(", ");    
  //TempLogFile.print(photocellReading);
  TempLogFile.print(", ");    
  TempLogFile.print(temperatureC);
#if ECHO_TO_SERIAL
  //Serial.print(", ");   
  //Serial.print(photocellReading);
  Serial.print(", ");    
  Serial.print(temperatureC);
#endif //ECHO_TO_SERIAL

  // Log the estimated 'VCC' voltage by measuring the internal 1.1v ref
  analogRead(BANDGAPREF); 
  delay(10);
  int refReading = analogRead(BANDGAPREF); 
  float supplyvoltage = (bandgap_voltage * 1024) / refReading; 
  
  TempLogFile.print(", ");
  TempLogFile.print(supplyvoltage);
#if ECHO_TO_SERIAL
  Serial.print(", ");   
  Serial.print(supplyvoltage);
#endif // ECHO_TO_SERIAL

  TempLogFile.println();
#if ECHO_TO_SERIAL
  Serial.println();
#endif // ECHO_TO_SERIAL

  digitalWrite(greenLEDpin, LOW);

  // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
  // which uses a bunch of power and takes time
  if ((millis() - syncTime) < SavePeriod) return;
  syncTime = millis();
  
  // blink LED to show we are syncing data to the card & updating FAT!
  digitalWrite(redLEDpin, HIGH);
  TempLogFile.flush();
  digitalWrite(redLEDpin, LOW);
  
}


