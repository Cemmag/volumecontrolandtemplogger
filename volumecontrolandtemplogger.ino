/*****************************************************************************************************************
Author:       Jacobus Van Eeden
Date:         July 2017 -> xxxxxx 2017
File name:    volumecontrolandtemplogger.ino

Description:
The purpose of this program is to set-up a test device that will log temperature measurements of audio power
amplifiers at predictable intervals and while controlling the attenuation of the audio signal being fed to the
amplifier. The basic logic of the system was inspired by the Arduino Temperature and Light Logger tutorial
mostly the SD card interactions, the thermocouple interfacing and attenuator circuit interfacing was done by me.
Schematics should eventually be included in the GIT repository. The systems uses a Arduino Uno, the SD Card shield
a AD8495 Type K thermocouple sensor and a LM1971 Overture™ Audio Attenuator. The intervals are currently set more
for testing than for operation but can easily be changed.
******************************************************************************************************************/

//Includes
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

// Global Variables
///////////////////
uint32_t saveSync_time = 0;                               // time of last sync()
uint32_t volUpdated_time = 0;                             // time of last volume update 

// Defines & Constants
//////////////////////
#define SERIAL_PRINT 1                                    // echo data to serial port

static const int redLED = 2;                              // RED UNO LED
static const int greenLED = 3;                            // GREEN UNO LED
static const int aquirePeriod = 1000;							        // Time in milliseconds between acquires (also affects logging)
static const int savePeriod = 10000;							        // Time in milliseconds between saves (should be greater than aquirePeriod, larger values result in faster operation)
static const int volumePeriod = 1000;                     // Time in milliseconds between volume updates
static const int tempPin = 0;                             // Sets analog channel from which to measure AD8495 breakout board from
static const float thermocouple_voltage = 1.25;           // Constant for the AD8495 equation
static const float thermocouple_divider = 0.005;          // Constant for the AD8495 equation
static const float ADCRes = 0.0049;                       // Resolution of Ardunio ADC 4.9mV per 1 ADC Count

volatile int volume = 0;                                  // "COUNTER" to keep track of the byte for the lm1971 attenuation setting

static const int LM1971_Byte_0 = 0;     // Sets Byte 0 to always be 0 since the LM1971m is a mono device and does not need channel select
static const int LM1971_Byte_1[] =	{
	63, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23,
	22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
}; // 63 is mute, rest of the values are equivalent to attenuation in dB

SPISettings VCSettings(500000, MSBFIRST, SPI_MODE1);      // Sets Volume Controllers SPI settings to a SPI Settings object 500kHz, MSB First and Mode 1.
RTC_PCF8523 RTC; 													                // Data Logger Shield Real Time Clock Object 

static const int sdSelect = 10; 											    // SD Card Select Pin (Set by shield but can be changed if so required refer to data sheets UNO DIGITAL #10)
static const int vcSelect = 9; 												    // Volume Controller Select Pin (UNO DIGITAL #9)
static const int startSelect = 8;                         // Digital I/O pin used for the enable switch.

File TempLogFile;														              // File object for the log file

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
	digitalWrite(redLED, HIGH);

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
	pinMode(redLED, OUTPUT);                 // Status LED
	pinMode(greenLED, OUTPUT);               // Status LED
	pinMode(startSelect, INPUT);                // Start input pin setup
	pinMode(sdSelect, OUTPUT);                  // Sets sdSelect pin as an output (SD Card)
	pinMode(vcSelect, OUTPUT);                  // sets vcSelect pin as an output (Volume Controller)

	digitalWrite(vcSelect, HIGH);               
	// initialize the SD card
	Serial.print("Initializing SD card...");

	// see if the card is present and can be initialized:
	if (!SD.begin(sdSelect)) {
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

  // Fails if the file could not be created
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
  
  // Sets the column headers for the log file
	TempLogFile.println("millis,datetime,Temperature(Deg C)");    
#if SERIAL_PRINT
	Serial.println("millis,datetime,Temperature(Deg C)");
#endif //SERIAL_PRINT

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN LOOP 
//////////////////////////////////////////////////////////////////////////////////////////////////

void loop(void)
{

  // Start/Stop switch check
	if(digitalRead(startSelect))
	{
		DateTime now;

		// delay for the amount of time we want between readings
		delay((aquirePeriod -1) - (millis() % aquirePeriod));

		digitalWrite(greenLED, HIGH);

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
		// log date and time
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

		// Reads the AD8495 output pin then converts it to a temperature in celcius
		int tempReading = analogRead(tempPin);
    delay(10);  // just waits a bit    
    // Converts ADC reading to a voltage V = ADC_Val * 0.0049
		float tempReadingAdjusted = tempReading * ADCRes;
    // AD8495 thermocouple breakout equation. T = (Vout - 1.25)/5mV
		float temperatureC = (tempReadingAdjusted - thermocouple_voltage)/thermocouple_divider;

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

		digitalWrite(greenLED, LOW);

    // If determines if enough time has passed to change the attenuation level of the LM1971m.
		if ((millis() - volUpdated_time) > volumePeriod)
		{
			volUpdated_time = millis(); 
			SPI.beginTransaction(VCSettings);                           // Loads SPI settings for the LM1971
			digitalWrite(vcSelect, LOW);                                // LM1971 chip select
			SPI.transfer(LM1971_Byte_0);                                // Sends the all 0 first byte
			SPI.transfer(LM1971_Byte_1[volume]);                        // Sents the second byte that dictates the attenuation level, simply uses incremental counter to change values.
			delay(10);                                                  // just make sure everything is sent fine.
			digitalWrite(vcSelect, HIGH);                               // Deselect the LM1971
			SPI.endTransaction();                                       // finish the SPI session
			if(volume++ > 45)                                           // resets volume if greater than 45 otherwise just increments it                
			  volume = 0;
		}

		// Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
		// which uses a bunch of power and takes time.
		if ((millis() - saveSync_time) > savePeriod)
		{
			saveSync_time = millis();
			// blink LED to show we are syncing data to the card & updating FAT!
			digitalWrite(redLED, HIGH);
			TempLogFile.flush();
			digitalWrite(redLED, LOW);
		}

	}
}

