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
uint32_t saveSync_time = 0; // time of last sync()
uint32_t volUpdated_time = 0; // time of last volume update 

// Defines & Constants
//////////////////////
#define SERIAL_PRINT   1 // echo data to serial port

// the digital pins that connect to the LEDs
#define redLEDpin 2
#define greenLEDpin 3

//static const int redLED
static const int SamplePeriod = 1000;							        // Time in milliseconds between acquires (also affects logging)
static const int SavePeriod = 10000;							        // Time in milliseconds between saves (should be greater than SamplePeriod, larger values result in faster operation)
static const int volumePeriod = 1000;                     // Time in milliseconds between volume updates
static const int tempPin = 0;                             // Sets analog channel from which to measure AD8495 breakout board from
static const float thermocouple_voltage = 1.25;           // Constant for the AD8495 equation
static const float thermocouple_divider = 0.005;          // Constant for the AD8495 equation
static const float ADCRes = 0.0049;                       // Resolution of Ardunio ADC 4.9mV per 1 ADC Count

volatile int volume = 0;

static const int LM1971_Byte_0 = 0;     // Sets Byte 0 to always be 0 since the LM1971m is a mono device and does not need channel select
static const int LM1971_Byte_1[] =	{
  63, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23,
  22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
}; // 63 is mute, rest of the values are equivalent to attenuation in dB

SPISettings VCSettings(500000, MSBFIRST, SPI_MODE1); // Sets Volume Controllers SPI settings to a SPI Settings object.
RTC_PCF8523 RTC; 													// Data Logger Shield Real Time Clock Object 

static const int SD_Select = 10; 											// SD Card Select Pin (Set by shield but can be changed if so required refer to data sheets)
static const int VC_Select = 9; 												// Volume Controller Select Pin
static const int startSelect = 8;                       // Digital I/O pin used for the enable switch.

File TempLogFile;														// File object for the log file

//////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////////////////////

// Error function sets the RED LED high and hangs if the SD doesnt initialize, it also prints to serial if enabled.
void error(char *str)
{
  #if SERIAL_PRINT
    Serial.print("error: ");
    Serial.println(str);
  #endif
  
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
  
  // Pin Mode selections
  pinMode(redLEDpin, OUTPUT);                 // Status LED
  pinMode(greenLEDpin, OUTPUT);               // Status LED
  pinMode(startSelect, INPUT);                // Start input pin setup
  pinMode(SD_Select, OUTPUT);                 // Sets SD_Select pin as an output (SD Card)
  pinMode(VC_Select, OUTPUT);                 // sets VC_Select pin as an output (Volume Controller)
  
  digitalWrite(VC_Select, HIGH);    // Sets the VC_Select pin high for inactive.
  // initialize the SD card
  Serial.print("Initializing SD card...");
  
  // see if the card is present and can be initialized:
  if (!SD.begin(SD_Select)) {
    error("Card failed, or not present");
  }
  Serial.println("card initialized.");
  
  // Creates a new file name, highest file number is 999
  char filename[] = "Temp_000.CSV";
  for (uint8_t i = 0; i < 1000; i++) {
    filename[5] = i/100 + '0';
    filename[6] = i/10 + '0';
    filename[7] = i%10 + '0';
    // if checks to see if the file exists, if it does it runs through another for iteration, if not file is created and it exits.
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
#if SERIAL_PRINT
    Serial.println("RTC failed");
#endif  //SERIAL_PRINT
  }


  SPI.begin();  // Initiallizes the spi controlls.

  TempLogFile.println("millis,stamp,datetime,light,temp,vcc");    
#if SERIAL_PRINT
  Serial.println("millis,stamp,datetime,light,temp,vcc");
#endif //SERIAL_PRINT

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
#if SERIAL_PRINT
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
#if SERIAL_PRINT
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
#endif //SERIAL_PRINT
   
  analogRead(tempPin); 
  delay(10);
  int tempReading = analogRead(tempPin);    

  float tempReadingAdjusted = tempReading * ADCRes;

  TempLogFile.print(", ");
  TempLogFile.print(tempReading);
  #if SERIAL_PRINT
    Serial.print(", ");   
    Serial.print(tempReading);
  #endif // SERIAL_PRINT
  
  // converting that reading to voltage, for 3.3v arduino use 3.3, for 5.0, use 5.0
  //float voltage = tempReading * aref_voltage / 1024;  
  float temperatureC = (tempReadingAdjusted - thermocouple_voltage)/thermocouple_divider;
  float temperatureF = (temperatureC * 9 / 5) + 32;
  
  TempLogFile.print(", ");    
  TempLogFile.print(temperatureC);
#if SERIAL_PRINT
  Serial.print(", ");    
  Serial.print(temperatureC);
#endif //SERIAL_PRINT

  TempLogFile.println();
#if SERIAL_PRINT
  Serial.println();
#endif // SERIAL_PRINT

  digitalWrite(greenLEDpin, LOW);

  if ((millis() - volUpdated_time) > volumePeriod)
  {
    volUpdated_time = millis(); 
    SPI.beginTransaction(VCSettings);
    digitalWrite(VC_Select, LOW);
    SPI.transfer(LM1971_Byte_0);
    SPI.transfer(LM1971_Byte_1[volume]);
    delay(100);
    digitalWrite(VC_Select, HIGH);
    SPI.endTransaction();
    if(volume++ > 45)
      volume = 0;
  }

 /* // the volume control code
  if ((millis() - volUpdated_time) < volumePeriod)
  {
    
  }
  else 
  {
    volUpdated_time = millis(); 
    SPI.beginTransaction(VCSettings);
    digitalWrite(VC_Select, LOW);
    SPI.transfer(LM1971_Byte_0);
    SPI.transfer(LM1971_Byte_1[volume]);
    delay(100);
    digitalWrite(VC_Select, HIGH);
    SPI.endTransaction();
    if(volume++ > 45)
      volume = 0;
  }
*/

  // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
  // which uses a bunch of power and takes time
  if ((millis() - saveSync_time) < SavePeriod) return;
  saveSync_time = millis();
  
  // blink LED to show we are syncing data to the card & updating FAT!
  digitalWrite(redLEDpin, HIGH);
  TempLogFile.flush();
  digitalWrite(redLEDpin, LOW);


}


