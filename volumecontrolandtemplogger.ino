/*****************************************************************************************************************
  Author:       Jacobus Van Eeden
  Date:         July 2017 -> xxxxxx 2017
  Filename:     volumecontrolandtemplogger.ino

  Description:
  The purpose of this program is to setup a test device that will log temperature measurements of audio power
  amplifiers at predictable intervals and while controlling the attenuation of the audio signal being fed to the
  amplifier. The basic logic of the system was insprired by the Arduino Temperature and Light Logger tutorial
  mostly the SD card interactions, the thermocouple interfacing and attenuator circuit interfacing was done by me.
  Schematics should eventually be included in the GIT repository.
******************************************************************************************************************/

//Includes
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

// Global Variables
///////////////////
uint32_t syncTime = 0; // time of last sync()

// Defines
//////////
#define ECHO_TO_SERIAL   1 // echo data to serial port
#define WAIT_TO_START    0 // Wait for serial input in setup()

// the digital pins that connect to the LEDs
#define redLEDpin 2
#define greenLEDpin 3

static const int tempPin = 0;                             // Sets analog channel from which to measure AD8495 breakout board from
static const int SamplePeriod = 1000;							        // Time in milliseconds between acquires (also affects logging)
static const int SavePeriod = 10000;							        // Time in milliseconds between saves (should be greater than SamplePeriod, larger values result in faster operation)
static const float thermocouple_voltage = 1.25;
static const float thermocouple_divider = 0.005;
static const float ADCRes = 0.0049; 

volatile int volume = 0;

static const int LM1971_Byte_0 = 0;     // Sets Byte 0 to always be 0 since the LM1971m is a mono device and does not need channel select
static const int LM1971_Byte_1[] =	{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
  63 // 63 is MUTE for the LM1971m
};

SPISettings VCSettings(5000000, MSBFIRST, SPI_MODE1); // Sets Volume Controllers SPI settings to a SPI Settings object.
RTC_PCF8523 RTC; 													// Data Logger Shield Real Time Clock Object 

static const int SD_Select = 10; 											// SD Card Select Pin (Set by shield but can be changed if so required refer to data sheets)
static const int VC_Select = 9; 												// Volume Controller Select Pin

File TempLogFile;														// File object for the log file

//////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////////

void error(char *str)
{
  Serial.print("error: ");
  Serial.println(str);
  
  // red LED indicates error
  digitalWrite(redLEDpin, HIGH);

  while(1);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP LOOP 
//////////////////////////////////////////////////////////////////////////////////////////////////
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

  pinMode(SD_Select, OUTPUT);       // Sets SD_Select pin as an output (SD Card)
  pinMode(VC_Select, OUTPUT);       // sets VC_Select pin as an output (Volume Controller)
  digitalWrite(VC_Select, HIGH);    // Sets the VC_Select pin high for inactive.
  // initialize the SD card
  Serial.print("Initializing SD card...");
  
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


  SPI.begin();  // Initiallizes the spi controlls.

  TempLogFile.println("millis,stamp,datetime,light,temp,vcc");    
#if ECHO_TO_SERIAL
  Serial.println("millis,stamp,datetime,light,temp,vcc");
#endif //ECHO_TO_SERIAL

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN LOOP 
//////////////////////////////////////////////////////////////////////////////////////////////////

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
#endif //ECHO_TO_SERIAL
   
  analogRead(tempPin); 
  delay(10);
  int tempReading = analogRead(tempPin);    

  float tempReadingAdjusted = tempReading * ADCRes;

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
  
  TempLogFile.print(", ");    
  TempLogFile.print(temperatureC);
#if ECHO_TO_SERIAL
  Serial.print(", ");    
  Serial.print(temperatureC);
#endif //ECHO_TO_SERIAL

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

  // the volume control code

  SPI.beginTransaction(VCSettings);
  digitalWrite(VC_Select, LOW);
  SPI.transfer(LM1971_Byte_0),
  SPI.transfer(LM1971_Byte_1[volume]);
  delay(100);
  digitalWrite(VC_Select, HIGH);
  SPI.endTransaction();
}


