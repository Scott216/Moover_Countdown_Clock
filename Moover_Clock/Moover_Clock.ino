/*
Daylight savings: Second sunday in March and First Sunday in November

The Moover runs on weekends starting on the Saturday after Thanksgiving.  The end date is flexible.
It also runs on holidays: The whole week between Xmas and New Years, MLK day and the whole week of President's day.
President's day is the 3rd Monday in February
MLK is the third monday in January
Thanksgiving is the 4th Thursday in November

in RTClib.h I had to rename dayOfWeek to dayofWeek2 because of a conflict in time.h

Mega Pin Wiring for Display, needs to be on Port A, pins 22-29.  D24-D29 are defined in library, don't try to change
R1   D24  
G1   D25
B1   D26
R2   D27
G2   D28
B2   D29
CLK  D12  clock can be on pins 10-13 or 50-53, but 10, 50-53 interfere with Audio board
OE   D38 aka EN. Can be any pin
A    D30  Can be any pin
B    D32  Can be any pin
C    D34  Can be any pin
LAT  D36 aka ST.  Can be any pin

 
Wiring for Audio player
D50 MISO
D51 MOSI
D52 CLK
D10 VS1053 chip select
D9  RST
D8  XDCS
D4  SD Card chip select
D3  DREQ  Needs to be on an interrupt pin

http://learn.adafruit.com/adafruit-vs1053-mp3-aac-ogg-midi-wav-play-and-record-codec-tutorial/library-reference
Adafruit_VS1053(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq) 
uint8_t begin(void) - Initialize SPI communication and (hard) reset the chip.
void setVolume(uint8_t left, uint8_t right) 

Hardware:
Mega Pro http://www.sparkfun.com/products/11007
LED Matrix Displays  http://www.adafruit.com/products/420
VS1053 audio board http://www.adafruit.com/products/1381
2.8 wattt Amp http://www.adafruit.com/products/1552  
RTC http://www.adafruit.com/products/255

Forum posts:
Display doesn't work with SPI (SOLVED): http://forums.adafruit.com/viewtopic.php?f=47&t=47537

Text to speach: http://www.oddcast.com/home/demos/tts/tts_example.php

*/

#include <Adafruit_GFX.h>    // http://github.com/adafruit/Adafruit-GFX-Library
#include <RGBmatrixPanel.h>  // http://github.com/protonmaster/RGB-matrix-Panel
#include <Wire.h>            // http://arduino.cc/en/reference/wire
#include "RTClib.h"          // http://github.com/adafruit/RTClib  (renamed dayOfWeek to dayofWeek2)
#include "Time.h"            // http://www.pjrc.com/teensy/td_libs_Time.html
#include <SPI.h>             // http://arduino.cc/en/Reference/SPI
#include <Adafruit_VS1053.h> // http://github.com/adafruit/Adafruit_VS1053_Library
#include <SD.h>              // http://arduino.cc/en/Reference/SD   
                             // http://github.com/arduino/Arduino/tree/master/libraries/SD


#define UNIXDAY 86400 // seconds in one day

// Matrix display Pin definitions for Mega
#define CLK 12
#define OE  38
#define A   30 
#define B   32 
#define C   34 
#define LAT 36 

#define SINGLEBUFFER true  // When true, increases refresh rate so there is no flickering
#define NUMDISPLAYS 2

// Pins for audio player
#define DREQ      3   // VS1053 Data request pin, must be in an interrupt pin
#define SDCARDCS  4   // SD Card chip select pin (output)
#define VS1053CS 10   // VS1053 chip select pin (output)
#define DCS       8   // VS1053 Data/command select pin (output)
#define RESET     9   // VS1053 reset pin (output)

#define VOLUMEPIN 0   // Analog input for volume adjustment

RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, SINGLEBUFFER, NUMDISPLAYS);
RTC_DS1307 RTC;
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(RESET, VS1053CS, DCS, DREQ, SDCARDCS);



static unsigned char PROGMEM moover[] = {
  B01111111, B11111111, B11111100, B00000000,
  B10100010, B10001010, B00100010, B00000000,
  B10100010, B10001010, B00100010, B00000000,
  B10100010, B10001010, B00100010, B00000000,
  B10111110, B11111011, B11100010, B00000000,
  B10000000, B00000000, B00000011, B11000000,
  B10000000, B00000000, B00000000, B00100000,
  B10000000, B00000000, B00000000, B00100000,
  B10000000, B00000000, B00000000, B00100000,
  B10000000, B00000000, B00000000, B00100000,
  B10000000, B00000000, B00000000, B00100000,
  B01111111, B11111111, B11111111, B11000000,
  B00010010, B00000000, B00010010, B00000000,
  B00100001, B00000000, B00100001, B00000000,
  B00010010, B00000000, B00010010, B00000000,
  B00001100, B00000000, B00001100, B00000000
};


int moover_hrs;
int moover_min;
int moover_sec;
int moover_hrs_prev = -1; // set default to -1 so time will not equal anything real and digits will get displayed
int moover_min_prev = -1;
int moover_sec_prev = -1;

uint32_t refreshTimer; // time to update the display  
 
tmElements_t tm;


// Function Prototypes
void displayCountdown(time_t nextMooverTime);
void displayBus(bool withSound);
void playTwoMinWarning(time_t nextMooverTime);
time_t convertDay(int y, int m, int d);
int daysUntilNextMoover();
bool isHoliday(time_t checkDate);
time_t thanksgiving(int yr);
time_t mlk(int yr);
time_t president(int yr);
time_t daylightNov(int yr);
time_t daylightMar(int yr);


void setup()
{
  Serial.begin(9600);
  Serial.println(F("Moover Clock Setup"));
  matrix.begin();  // initialize display
  

  // Initialize the music player
  if (musicPlayer.begin()) 
  {
    musicPlayer.sineTest(0x44, 500);  // Make a tone to indicate VS1053 is working, this resets the volume, so use it before setVolume
    musicPlayer.setVolume(30,30);     // Set volume for left, right channels. lower numbers = louder volume
    if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT))
    { Serial.println(F("DREQ pin is not an interrupt pin")); }
  }
  else
  { Serial.println(F("VS1053 not found")); }  
  // Initialize SD card
  if ( SD.begin(SDCARDCS) ) 
  { Serial.println(F("SD OK!")); }
  else
  { Serial.println(F("SD failed, or not present")); }

 
  // Set text size and color
  matrix.setTextSize(1);    // size 1 == 8 pixels high
  matrix.setTextColor(matrix.Color333(0,7,0));  
  
  // Start communication to clock
  Wire.begin();
  RTC.begin();
  if (!RTC.isrunning()) 
  { Serial.println(F("RTC is NOT running!")); }
  
//  RTC.adjust(DateTime(__DATE__, __TIME__));
  RTC.adjust(DateTime(2013, 12, 9, 7, 49, 54 ));  // use for debugging

  refreshTimer = millis();  // Initialize display refresh timer

} // end setup()


void loop() 
{
  time_t nextMooverTime;
  int daysNextMoover; 

  if ((long) (millis() - refreshTimer) > 0 )
  {
    DateTime now = RTC.now();

    // Calculate days until next time Moover will come.  Zero if Moover is running today
    daysNextMoover = daysUntilNextMoover();

    // Determine next Moover time
    if ( daysNextMoover > 0 )
    {
      // Next is sometime after today
      Serial.print("Days to next Moover: ");
      Serial.println(daysNextMoover);
      tm.Second = 0;
      tm.Minute = 50;
      tm.Hour = 6;
      tm.Day = now.day();
      tm.Month = now.month();
      tm.Year = now.year() - 1970;
      nextMooverTime =  makeTime(tm) + 24UL * 3600UL * (long) daysNextMoover;  // to deal with month rollover, it's easier to just add seconds to the time
    }
    else if ( now.hour() < 6 )
    {
      // Early morning, next Moover is the first one at 6:50 AM
      Serial.println("Early Morning");
      tm.Second = 0; 
      tm.Minute = 50;
      tm.Hour = 6;
      tm.Day = now.day();
      tm.Month = now.month();
      tm.Year = now.year() - 1970;
      nextMooverTime =  makeTime(tm); 
    }
    else if ( now.hour() < 12 )
    {
      Serial.println("Morning Schedule");

      // morning Moover schedule
      if (now.minute() < 20)
      {
        // Next Moover time is 20 minutes past the hour
        tm.Second = 0; 
        tm.Minute = 20;
        tm.Hour = now.hour();
        tm.Day = now.day();
        tm.Month = now.month();
        tm.Year = now.year() - 1970;
        nextMooverTime =  makeTime(tm);  
      }
      else if (now.minute() < 50 || now.hour() == 6 )
      {
        // Next Moover time is 50 minutes past the hour
        tm.Second = 0; 
        tm.Minute = 50;
        tm.Hour = now.hour();
        tm.Day = now.day();
        tm.Month = now.month();
        tm.Year = now.year() - 1970;
        nextMooverTime =  makeTime(tm);  
      }
      else
      {
        // Next Moover time is 20 minutes past the next hour
        tm.Second = 0; 
        tm.Minute = 20;
        tm.Hour = now.hour() + 1;
        tm.Day = now.day();
        tm.Month = now.month();
        tm.Year = now.year() - 1970;
        nextMooverTime =  makeTime(tm);  
       }
    }
    else
    {
       Serial.println("Afternoon Schedule ");
       
      // Afternoon schedule, Moover leaves Mount Snow on the every half hour on the half hour, but it varies when it arrives 
      // assume it will arrive at 15 & 45 minutes past the hour
      tm.Second = 0; 
      if ( now.minute() < 15 )
      { 
        tm.Minute = 15; 
        tm.Hour = now.hour();
      }
      else if ( now.minute() < 45 )
      { 
        tm.Minute = 45; 
        tm.Hour = now.hour();
      }
      else
      { 
        tm.Minute = 15; 
        tm.Hour = now.hour() + 1 ;
      }  
      tm.Day = now.day();
      tm.Month = now.month();
      tm.Year = now.year() - 1970;
      nextMooverTime =  makeTime(tm);  
    }
   
    displayCountdown(nextMooverTime); // display countdown timer
    refreshTimer = millis() + 500;    // update display every 1/2 second

    // Play 2 minute warning
    if ( moover_hrs == 0 && moover_min == 2 && moover_sec == 1 )
    { playTwoMinWarning(nextMooverTime); }


    // Countdown complete, display bus
    if (  moover_hrs == 0 && moover_min == 0 && moover_sec == 1)
    {
      // only play sound in the morning
      if (now.hour() < 12)
      { displayBus(true); } // show bus with sound
      else
      { displayBus(false); } // show bus without sound
      moover_hrs_prev = -1;  // forces hours to refresh in display
    }
  }  // refresh

}  // end loop()


void displayCountdown(time_t nextMooverTime)
{
  char countdownbuf[10]; 
  const byte countdnStartPos = 12;
  DateTime now = RTC.now();

  uint32_t countDownTime = nextMooverTime - now.unixtime();
  moover_sec = countDownTime % 60; // get seconds
  countDownTime /= 60; // convert to minutes
  moover_min = countDownTime % 60; // get minutes
  countDownTime /= 60; // convert to hours
  moover_hrs = countDownTime % 24; // get hours
  
  sprintf(countdownbuf, "%02d:%02d:%02d    ", moover_hrs, moover_min, moover_sec);
  Serial.print(countdownbuf);
  sprintf(countdownbuf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  Serial.println(countdownbuf);
 
  
  matrix.setCursor(14, 0);   
  matrix.print("Moover");
  
  // display hours
  if ( moover_hrs != moover_hrs_prev)
  {
    // Delete old hours digits
    matrix.setTextColor(0);  
    if (moover_hrs_prev == 1 || moover_hrs_prev == 11) 
    { matrix.setCursor(countdnStartPos + 1 , 9); } // Move hours to the right 1 row
    else
    { matrix.setCursor(countdnStartPos, 9); }
    sprintf(countdownbuf, "%02d", moover_hrs_prev);
    matrix.print(countdownbuf);

    // display new hour
    matrix.setTextColor(matrix.Color333(0,7,0));  
    if (moover_hrs == 1 || moover_hrs == 11) 
    { matrix.setCursor(countdnStartPos + 1 , 9); } // Move hours to the right 1 row
    else
    { matrix.setCursor(countdnStartPos, 9); }
    sprintf(countdownbuf, "%02d", moover_hrs);
    matrix.print(countdownbuf);
  }

  matrix.setCursor(countdnStartPos + 10, 9);   
  matrix.print(":");
  
  // display minutes
  if ( moover_min != moover_min_prev)
  {
    // Delete old minutes digits
    matrix.setTextColor(0);  
    matrix.setCursor(countdnStartPos + 14, 9);   
    sprintf(countdownbuf, "%02d", moover_min_prev);
    matrix.print(countdownbuf);
    
    // display new mintues
    matrix.setTextColor(matrix.Color333(0,7,0));  
    matrix.setCursor(countdnStartPos + 14, 9);   
    sprintf(countdownbuf, "%02d", moover_min);
    matrix.print(countdownbuf);
  }
  
  matrix.setCursor(countdnStartPos + 24, 9);   
  matrix.print(":");

  // delete old seconds digits
  matrix.setTextColor(0);  
  matrix.setCursor(countdnStartPos + 28, 9);   
  sprintf(countdownbuf, "%02d", moover_sec_prev);
  matrix.print(countdownbuf);
  
  // display new seconds
  matrix.setTextColor(matrix.Color333(0,7,0));  
  matrix.setCursor(countdnStartPos + 28, 9);   
  sprintf(countdownbuf, "%02d", moover_sec);
  matrix.print(countdownbuf);

  moover_hrs_prev = moover_hrs;
  moover_min_prev = moover_min;
  moover_sec_prev = moover_sec;
  
}  // end displayCountdown()


// Start bus scrolling across the screen and play moover sound
void displayBus(bool withSound)
{

  int scrollDelay = 120;
  
  if (withSound) 
  {
    int volLevel = analogRead(VOLUMEPIN / 4);
    musicPlayer.setVolume(volLevel,volLevel);  // lower number is higher volume

    // Start playing a file, then we can do stuff while waiting for it to finish
    // File names should be < 8 characters
    // moover1.mp3 is cow sound, arrive2x.mp3 is woman announcing moover
    if (! musicPlayer.startPlayingFile("arrive2x.mp3")) 
    { Serial.println(F("Could not open mp3 file on SD card")); }
    
    // while sound is playing, scroll the bus across the display
    while (musicPlayer.playingMusic ) 
    {
      for (int xpos = -27; xpos < 66; xpos++)
      {
        matrix.fillScreen(0);  // fill the screen with 'black'
        matrix.drawBitmap(xpos++, 0, moover, 32, 16, matrix.Color333(7,4,0) ); // draw bus
        delay(scrollDelay);
      }
    }  // end while playing loop
  } // end with sound
  else
  // don't play sound, only display bus scrolling on screen
  {
    for (int xpos = -27; xpos < 66; xpos++)
    {
      matrix.fillScreen(0);  // fill the screen with 'black'
      matrix.drawBitmap(xpos++, 0, moover, 32, 16, matrix.Color333(7,4,0) ); // draw bus
      delay(scrollDelay);
    }
  }      
  
  musicPlayer.setVolume(255,255); // 255 turns volume off
  
}  // end displayBus()

// Play two minute warning sound
void playTwoMinWarning(time_t nextMooverTime)
{

  int volLevel = analogRead(VOLUMEPIN / 4);
  musicPlayer.setVolume(volLevel,volLevel);  // lower number is higher volume

  // Start playing a file
  // File names should be < 8 characters
  if (! musicPlayer.startPlayingFile("arrive.mp3"))
  { Serial.println(F("Could not open mp3 file on SD card")); }

  // while sound is playing, scroll the bus across the display
  while (musicPlayer.playingMusic )
  {
    // Display time
    displayCountdown(nextMooverTime); // display countdown timer
    delay(500);
  }

  musicPlayer.setVolume(255,255); // 255 turns volume off

} // playTwoMinWarning()


// Retuns number of days until the next time Moover comes
// zero if Moover runs today
int daysUntilNextMoover()
{
  DateTime now = RTC.now();
  int seasonEndYear = now.year();
  if ( now.month() >= 10 )
  { seasonEndYear++; } // It's still start of season, season end year is next year 

  // If it's off seasion return 2 days after Thanksgiving
  if ( now.unixtime() < thanksgiving(now.year()) && now.unixtime() > convertDay(now.year(), 4, 16))
  { return  ((thanksgiving(now.year()) + 2UL * UNIXDAY ) - now.unixtime() ) / UNIXDAY; }

  // Check it today is a Saturday between Thanksgiving and April 15
  if ( now.unixtime() > thanksgiving(now.year()) && now.unixtime() < convertDay(seasonEndYear, 4, 16) && now.dayOfWeek2() == 6 )
  {
    if ( now.hour() < 17 )
    { return 0; } // Moover runs today
    else
    { return 1; } // It's after 5PM, Moover runs tomorrow
  }  
  
  // Check if today is a Sunday between Thanksgiving and April 15 and time is < 5 PM
  if ( now.unixtime() > thanksgiving(now.year()) && now.unixtime() < convertDay(seasonEndYear, 4, 16) && now.dayOfWeek2() == 7 && now.hour() < 17)
  { return 0; } // Moover runs today

  // see if any days in the next 7 are a holiday
  int d = 0;
  if (now.hour() >= 17) // if it's after 5PM, don't including today
  { d = 1; }
  for (d; d < 7; d++)
  {
  if (isHoliday(now.unixtime()) + UNIXDAY * d)
    { return d; }
  }

  // If today is Sunday after 5PM, return next Saturday
  if (now.dayOfWeek2() == 7 && now.hour() >= 17)
  { return 6; }

  // Next Moover is next Saturday
  return 6 - now.dayOfWeek2();

} // end daysUntilNextMoover()


// Is today a holiday
bool isHoliday(time_t checkDate)
{
  time_t holiday;
  time_t todayStart = convertDay(year(checkDate), month(checkDate), day(checkDate)); // today at midnight
  
  holiday = mlk(year(checkDate));
  if ( todayStart == holiday )
  { return true; }  // Today is MLK Day
  
  // check for President's week
  holiday = president(year(checkDate));
  if (todayStart >= holiday && todayStart <= (holiday + UNIXDAY * 5UL  ) )
  { return true; }  // Today is President's week
  
  // check for Chirsmas break, if day is between 12/27 and 1/1, return true 
  holiday = convertDay(year(checkDate), 12, 27);
  if (todayStart >= holiday && todayStart <= (holiday + UNIXDAY * 6UL) )
  { return true; }  // Today is Xmas break  
  
  return false; // today is not a holiday
  
}  // end isHoliday()


// Return Thanksgiving date
// 4th Thursday in November
time_t thanksgiving(int yr)
{
  time_t t;
  tmElements_t tm;
  tm.Second = 0;
  tm.Minute = 0;
  tm.Hour = 0;
  tm.Month = 11;
  tm.Year = yr - 1970;
  
  // loop through first 7 days of November until you find first Thursday
  for( int i = 1; i <= 7; i++)
  {
    tm.Day = i; 
    t = makeTime(tm);
    if ( weekday(t) == 5 )  // weekday: Thursday is 5
    {
      tm.Day = i + 21;  // found first Thursday, Thanksgiving is 21 days later
      return makeTime(tm);
    }
  } 
} // end thanksgiving()


// Return MLK Day date
// 3rd Monday in January
time_t mlk(int yr)
{
  time_t t;
  tmElements_t tm;
  tm.Second = 0;
  tm.Minute = 0;
  tm.Hour = 0;
  tm.Month = 1;
  tm.Year = yr - 1970;
  
  // loop through first 7 days of January until you find first Monday
  for( int i = 1; i <= 7; i++)
  {
    tm.Day = i; 
    t = makeTime(tm);
    if ( weekday(t) == 2 )  // weekday: Monday is 2
    {
      tm.Day = i + 14;  // found first Monday, MLK is 14 days later
      return makeTime(tm);
    }
  } 
} // end mlk()


// Return President's Day Day date
// 3rd Monday in February
time_t president(int yr)
{
  time_t t;
  tmElements_t tm;
  tm.Second = 0;
  tm.Minute = 0;
  tm.Hour = 0;
  tm.Month = 2;
  tm.Year = yr - 1970;
  
  // loop through first 7 days of February until you find first Monday
  for( int i = 1; i <= 7; i++)
  {
    tm.Day = i; 
    t = makeTime(tm);
    if ( weekday(t) == 2 )  // weekday: Monday is 2
    {
      tm.Day = i + 14;  // found first Monday, President's Day is 14 days later
      return makeTime(tm);
    }
  } 
} // end president()


// Return November Daylight savings
// 1st Sunday in November, 2 AM
time_t daylightNov(int yr)
{
  time_t t;
  tmElements_t tm;
  tm.Second = 0;
  tm.Minute = 0;
  tm.Hour = 2;
  tm.Month = 11;
  tm.Year = yr - 1970;
  
  // loop through first 7 days of November until you find first Sunday
  for( int i = 1; i <= 7; i++)
  {
    tm.Day = i; 
    t = makeTime(tm);
    if ( weekday(t) == 1 )  // weekday: Sunday is 1
    {
      tm.Day = i;  // found first Sunday
      return makeTime(tm);
    }
  } 
} // end daylightNov()


// Return November Daylight savings
// 2nd Sunday in March, 2 AM
time_t daylightMar(int yr)
{
  time_t t;
  tmElements_t tm;
  tm.Second = 0;
  tm.Minute = 0;
  tm.Hour = 2;
  tm.Month = 3;
  tm.Year = yr - 1970;
  
  // loop through first 7 days of March until you find first Sunday
  for( int i = 1; i <= 7; i++)
  {
    tm.Day = i; 
    t = makeTime(tm);
    if ( weekday(t) == 1 )  // weekday: Sunday is 1
    {
      tm.Day = i + 7;  // found first Sunday, daylight saving is 2nd Sunday
      return makeTime(tm);
    }
  } 
} // end daylightMar()


// Returns date in time_t format
time_t convertDay(int y, int m, int d)
{
  tmElements_t tm;
  tm.Second = 0;
  tm.Minute = 0;
  tm.Hour = 0;
  tm.Day = d;
  tm.Month = m;
  tm.Year = y - 1970;
  return makeTime(tm);
} // end convertDay()
