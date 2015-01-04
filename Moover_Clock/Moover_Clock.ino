/*

The Moover runs on weekends starting on the Saturday after Thanksgiving.  The end date is flexible.
It also runs on holidays: The whole week between Xmas and New Years, MLK day and the whole week of President's day.
President's day is the 3rd Monday in February
MLK is the third monday in January
Thanksgiving is the 4th Thursday in November

In RTClib.h I had to rename dayOfWeek to dayofWeek2 because of a conflict in time.h

To Do:
Add a countdown to the new year


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

Wiring for Audio Amp
VDD - Vcc on Mega
GND - GND on Audio player
SDL - D43 on Mega (disables amp)
L- to AGND on Audio Player
L= to Lout on Audio player


http://learn.adafruit.com/adafruit-vs1053-mp3-aac-ogg-midi-wav-play-and-record-codec-tutorial/library-reference
Adafruit_VS1053(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq) 
uint8_t begin(void) - Initialize SPI communication and (hard) reset the chip.
void setVolume(uint8_t left, uint8_t right) 

Hardware:
Mega Pro http://www.sparkfun.com/products/11007
LED Matrix Displays  http://www.adafruit.com/products/420
VS1053 audio board http://www.adafruit.com/products/1381
2.8 wattt Amp http://www.adafruit.com/products/1552 schematic: http://www.adafruit.com/datasheets/TS2012sch.png
RTC http://www.adafruit.com/products/255

Forum posts:
Display doesn't work with SPI (SOLVED): http://forums.adafruit.com/viewtopic.php?f=47&t=47537
Tried SwapBuffers to see if it reduced flickr, but it didn't look any better. See post Adafruit forum post: http://bit.ly/1jKXVyF
Text to speach: http://www.oddcast.com/home/demos/tts/tts_example.php

Change Log:
         v1.50   Fixed daylight saving, fixed couple small bugs
         v1.51   Set clock to not display countdown if season is over
11/08/14 v1.52   Clock didn't adjust for daylight savings time in Nov.  Changed DLS comparision to a 10 second window insetad of 1 second
12/18/14 v1.53   Added return 0 to non-void functions to get rid of warning
12/30/14 v1.54   Program was locking up because char buf[20]; was not long enough. I increased to 30
01/01/14 v1.55   Changed holiday status.  On Thursday 1/1/15 clock was showing countdown to Saturday.  Added Happy New Year

*/
#define VERSION "v1.55"

#include <Adafruit_GFX.h>    // http://github.com/adafruit/Adafruit-GFX-Library
#include <RGBmatrixPanel.h>  // http://github.com/protonmaster/RGB-matrix-Panel
#include <Wire.h>            // http://arduino.cc/en/reference/wire
#include <RTClib.h>          // http://github.com/adafruit/RTClib  (renamed dayOfWeek to dayofWeek2)
#include <Time.h>            // http://www.pjrc.com/teensy/td_libs_Time.html
#include <SPI.h>             // http://arduino.cc/en/Reference/SPI
#include <Adafruit_VS1053.h> // http://github.com/adafruit/Adafruit_VS1053_Library   MP# audio board
#include <SD.h>              // http://arduino.cc/en/Reference/SD   
                             // http://github.com/arduino/Arduino/tree/master/libraries/SD

// This gets rid of compiler warning: Only initialized variables can be placed into program memory area
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))

#define UNIXDAY 86400 // seconds in one day
#define DOW_SAT 6     // Returned by now.dayOfWeek2()
#define DOW_SUN 0

// Matrix display Pin definitions for Mega
#define CLK 12
#define OE  38
#define A   30 
#define B   32 
#define C   34 
#define LAT 36 
#define CLEAR_DISP 0

const boolean DOUBLEBUFFER = false;  // When true increases refresh rate so there is no flickering.  Need to also use with swapBuffers().  Display looks better when this is false
const int     NUMDISPLAYS =     2; 

enum colorme_t { COLOR_BUS, COLOR_COUNTDOWN, COLOR_TIME };

const float LASTMOOVER = 17.25; 

// Pins for audio player
#define DREQ      3   // VS1053 Data request pin, must be in an interrupt pin
#define SDCARDCS  4   // SD Card chip select pin (output)
#define VS1053CS 10   // VS1053 chip select pin (output)
#define DCS       8   // VS1053 Data/command select pin (output)
#define RESET     9   // VS1053 reset pin (output)

#define VOLUMEPIN 0      // Analog input for volume adjustment
#define DISABLE_AUDIO 43 // Use to shut off amp (to prevent humming) - low signal will shuts it down

RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, DOUBLEBUFFER, NUMDISPLAYS);
RTC_DS1307 RTC;
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(RESET, VS1053CS, DCS, DREQ, SDCARDCS);


// This is a bitmap for the shape of the bus which will scroll across the screen
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

 
tmElements_t tm;

// Function Prototypes
void displayCountdown(time_t nextMooverTime);
void displayCurrentTime(unsigned int secondsToDisplay);
void displayBus(bool withSound);
void displayHappyNewYear();
void playTwoMinWarning(time_t nextMooverTime);
time_t convertDay(int y, int m, int d);
int daysUntilNextMoover();
uint16_t setDisplayColor(colorme_t colorme);
bool isHoliday(time_t checkDate);
time_t thanksgiving(int yr);
time_t mlk(int yr);
time_t president(int yr);
time_t daylightNov(int yr);
time_t daylightMar(int yr);
time_t nextMoover();
bool checkDaylightSavings(DateTime now);
time_t setUnixTime(int hour, int min);
void setSpeakerVolume(byte volumeLevel);

void setup()
{
  Serial.begin(9600);
  delay(3000);
  matrix.begin();  // initialize display
  
  pinMode(DISABLE_AUDIO, OUTPUT);
  
  // Initialize the music player
  if (musicPlayer.begin()) 
  {
    musicPlayer.sineTest(0x44, 500);  // Make a tone to indicate VS1053 is working, this resets the volume, so use it before setVolume
    setSpeakerVolume(255);     // turn off speaker
    if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT))  // do not remove
    { Serial.println(F("DREQ pin is not an interrupt pin")); }
  }
  else
  { Serial.println(F("VS1053 Sound card not found")); }  

  // Initialize SD card
  if ( SD.begin(SDCARDCS) ) 
  { Serial.println(F("SD OK!")); }
  else
  { Serial.println(F("SD failed, or not present")); }

  // Set text size and color
  matrix.setTextSize(1);    // size 1 == 8 pixels high
  matrix.setTextWrap(false); // Allow text to run off right edge
  matrix.setTextColor(setDisplayColor(COLOR_COUNTDOWN));
  matrix.fillScreen(CLEAR_DISP); 
 
  // Start communication to clock
  Wire.begin();
  RTC.begin();
  if (RTC.isrunning()) 
  { Serial.println(F("RTC is running!")); }
  else
  { Serial.println(F("RTC is NOT running!")); }
  
//  RTC.adjust(DateTime(__DATE__, __TIME__));       // Will set the RTC clock to the time when program was compiled
//  RTC.adjust(DateTime(2014, 2, 8, 7, 49, 50 ));   // use for debugging, sets time to 10 seconds before moover comes
//  RTC.adjust(DateTime(2014, 11, 2, 1, 59, 50 ));  // November Daylight Savings Test
//  RTC.adjust(DateTime(2014, 3, 9, 1, 59, 50 ));   // March Daylight Savings Test
//  RTC.adjust(DateTime(2014, 12, 31, 23, 59, 50 ));   // 10 seconds before new year
  
  Serial.print(F("Moover Clock Setup "));
  Serial.println(VERSION);
  
  char buf[30];
  DateTime now = RTC.now();
  sprintf(buf, "Time: %d/%d/%d %02d:%02d:%02d", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second());
  Serial.println(buf);

} // end setup()


void loop() 
{
  static uint32_t updateMooverTimer = millis(); // time to update the display  
  DateTime now = RTC.now();
  
  // Check for New Years Eve, display "Happy New Year" for 3 minutes
  bool showHappyNewYear = false; 
  if (now.month() == 1 && now.day() == 1 && now.hour() == 0 && now.minute() < 3 )
  { 
    showHappyNewYear = true; 
    displayHappyNewYear(); 
  }

  
  bool isDaytime = (now.hour() >= 4 && now.hour() <= 17); 
  // display Moover countdown
  if ((long) (millis() - updateMooverTimer) > 0 && isDaytime && daysUntilNextMoover() < 15 ) 
  {
    displayCountdown( nextMoover() );   // display countdown timer
    updateMooverTimer = millis() + 500; // update display every 1/2 second

    // Play 2 minute warning, in the morning only
    if ( moover_hrs == 0 && moover_min == 2 && moover_sec == 0 && now.hour() < 12 )
    { playTwoMinWarning( nextMoover() ); }

    // Countdown complete, display bus
    if (  moover_hrs == 0 && moover_min == 0 && moover_sec == 1 )
    {
      // only play sound in the morning
      if ( now.hour() > 6 && now.hour() < 12 )
      { displayBus(true); } // show bus with sound
      else
      { displayBus(false); } // show bus without sound
      moover_hrs_prev = -1;  // forces hours to refresh in display
    }
  }  // refresh moover countdown


  // show time if showCurrentTimeTimer has expired and moover is not due in the next 125 seconds
  static uint32_t showCurrentTimeTimer = millis() + 5000UL;  // initialize timer 
  bool isMoverComingSoon =  (nextMoover() - now.unixtime()) < 125;
  if ((long) (millis() - showCurrentTimeTimer) > 0 && !isMoverComingSoon && !showHappyNewYear )
  {
    displayCurrentTime(3000); // display time for 3 seconds
    showCurrentTimeTimer = millis() + 5000UL; // show again in 5 seconds
    moover_hrs_prev = -1;  // forces hours to refresh in display
    moover_min_prev = -1;  // forces minutes to refresh in display
    // If daytime, clear dispaly for countdown timer
    if ( isDaytime && daysUntilNextMoover() < 15 )
    {  matrix.fillScreen(CLEAR_DISP); }
  } 

  checkDaylightSavings(now); 

}  // end loop()


time_t nextMoover()
{
  DateTime now = RTC.now();

  // Calculate days until next time Moover will come.  Zero if Moover is running today
  int daysNextMoover = daysUntilNextMoover();
  
  float decimalHour = (float)now.hour() + (float)now.minute()/60.0 + (float)now.second()/3600.0;

  // Determine next Moover time
  if ( daysNextMoover > 0 )
  {
    // Next Moover is after today
    return setUnixTime(6, 50) + 24UL * 3600UL * (long) daysNextMoover;  // to deal with month rollover, it's easier to just add seconds to the time
  }
  else if ( decimalHour <= (6 + 50.0/60.0) )
  {
    // Early morning, next Moover at 6:50 AM
    return setUnixTime(6, 50); 
  }
  else if ( decimalHour <= (11 + 20.0/60.0) )
  {
    if (now.minute() < 20)
    {  return setUnixTime(now.hour(), 20); } // Next Moover time is 20 minutes past current hour
     else if (now.minute() < 50)
    { return setUnixTime(now.hour(), 50); } // Next Moover time is 50 minutes past the current hour
    else if (now.minute() >= 50)
    {  return setUnixTime(now.hour()+1, 20); } // Next Moover time is 20 minutes past the next hour
  }  // end morning schedule
  else
  {
    // Afternoon schedule, Moover leaves Mount Snow on the every half hour on the half hour, but it varies when it arrives 
    // assume it will arrive at 15 & 45 minutes past the hour
    if ( decimalHour < (12 + 15.0/60.0) )
    { return setUnixTime(12, 15); }  // Next moover is at 12:15 
    else
    {
      if (now.minute() < 15)
      {  return setUnixTime(now.hour(), 15); } // Next Moover time is 15 minutes past current hour
       else if (now.minute() < 45)
      { return setUnixTime(now.hour(), 45); } // Next Moover time is 5045 minutes past the current hour
      else if (now.minute() >= 45)
      {  return setUnixTime(now.hour()+1, 15); } // Next Moover time is 15 minutes past the next hour
    }
  } // End afternoon schedule
  return 0;
} // nextMoover()

time_t setUnixTime(int hour, int min)
{
  DateTime now = RTC.now();
  tm.Second = 0; 
  tm.Minute = min;
  tm.Hour = hour;
  tm.Day = now.day();
  tm.Month = now.month();
  tm.Year = now.year() - 1970;
  return  makeTime(tm);  
}  // end setUnixTime()


// Retuns number of days until the next time Moover comes
// zero if Moover runs today
int daysUntilNextMoover()
{
  DateTime now = RTC.now();
  float decimalHour = (float)now.hour() + (float)now.minute()/60.0 + (float)now.second()/3600.0;
  
  int seasonEndYear = now.year();
  if ( now.month() >= 10 )
  { seasonEndYear++; } // It's still start of season, season end year is next year 
  
  // If it's off-seasion return 2 days after Thanksgiving
  if ( now.unixtime() < thanksgiving(now.year()) && now.unixtime() > convertDay(now.year(), 4, 16))
  { return  ((thanksgiving(now.year()) + 2UL * UNIXDAY ) - now.unixtime() ) / UNIXDAY; }

  // Check if today is a Saturday 
  if ( now.dayOfWeek2() == DOW_SAT )
  {
    if ( decimalHour <= LASTMOOVER) // 5:15 PM
    { return 0; } // Moover runs today
    else
    { return 1; } // It's after 5:15 PM, Moover runs tomorrow
  }  

  // Check if today is a Sunday and time is < 5:15 PM
  if ( now.dayOfWeek2() == DOW_SUN && decimalHour <= LASTMOOVER )
  { return 0; } // Moover runs today

  // see if any days in the next 5 are a holiday
  int startday = 0;
  if ( decimalHour > LASTMOOVER) // if it's after 5PM, don't including today
  { startday = 1; }
  
  for (int d = startday; d < 5; d++)
  {
    if ( isHoliday(now.unixtime() + UNIXDAY * d) )
    { 
      // See if a Saturday is before the  holiday
      if ( ( 6 - now.dayOfWeek2()) < d )
      { return 6 - now.dayOfWeek2(); }  // Saturday comes before holiday, so return that
      else
      { return d; }  // Holiday is next day Moover comes
    }
  }

  // If today is Sunday after 5PM, return next Saturday
  if ( now.dayOfWeek2() == 7 && decimalHour > LASTMOOVER )
  { return 6; }

  // Next Moover is next Saturday
  return 6 - now.dayOfWeek2();

} // end daysUntilNextMoover()


// Is today a holiday
// Checks for MLK, President's week, Christmas break
bool isHoliday(time_t checkDate)
{
  time_t holiday;
  time_t todayStart = convertDay(year(checkDate), month(checkDate), day(checkDate)); // today at midnight
  
  holiday = mlk(year(checkDate));
  if ( todayStart == holiday )
  { return true; }  // Today is MLK Day
  
  // Check for President's week
  holiday = president(year(checkDate));
  if (todayStart >= holiday && todayStart <= (holiday + UNIXDAY * 5UL ) )
  { return true; }  // Today is President's week
  
  // check for Chirsmas break, if day is between Dec 27 and Jan 3, return true 
  // Get the year.  If it just turned into a new year, use last year - 1
  uint16_t getYear = year(checkDate);
  if ( month(checkDate) == 1 && day(checkDate) < 10 )
  { getYear = getYear - 1; }
  
  holiday = convertDay(getYear, 12, 27);
  if (todayStart >= holiday && todayStart <= ( holiday + UNIXDAY * 6UL ) )
  { return true; }  // Today is Xmas break  
  
  return false; // today is not a holiday
  
}  // end isHoliday()


// Return Thanksgiving date (4th Thursday in November)
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
  return 0;
} // end thanksgiving()


// Return MLK Day date (3rd Monday in January)
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
  return 0;
} // end mlk()


// Return President's Day Day date (3rd Monday in February)
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
  return 0;
} // end president()


// Chack and adjust time for daylight savings
bool checkDaylightSavings(DateTime now)
{
  static bool daylightFlag; // prevents daylight savings from being adjusted more then once, only need in November
  static byte lastHourChecked; // only check DLS once an hour
  
  if ( lastHourChecked == now.hour() )
  { return false; }; 
  lastHourChecked = now.hour();  // It's a new hour, reset variable and go on to check for daylight savings
  
  // Check time with +/- 5 second range
  time_t dslMar = daylightMar(now.year());
  if ( now.unixtime() > dslMar - 5 && now.unixtime() < dslMar + 5  )
  {
    RTC.adjust(now.unixtime() + 3600);  // Add an hour
    Serial.println(F("Added 1 hour for dalylight savings"));
    return true;
  }
  
  // Check time with +/- 5 second range
  time_t dslNov = daylightNov(now.year());
  if ( (now.unixtime() > dslNov - 5 && now.unixtime() < dslNov + 5)  && daylightFlag == false )
  {
    RTC.adjust(now.unixtime() - 3600);  // Subtract an hour
    daylightFlag = true; 
    Serial.println(F("Subtracted 1 hour for dalylight savings"));
    return true;
  }

  if (now.hour() == 3)
  { daylightFlag = false; } // Reset flag at 3AM
  
  return false;  // didn't adjust time
  
}  // checkDaylightSavings


// Return November Daylight savings (1st Sunday in November, 2 AM)
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
  return 0;
} // end daylightNov()


// Return March Daylight savings (2nd Sunday in March, 2 AM)
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
  return 0;
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


// Displays the countdown time in the LED matrix
void displayCountdown(time_t nextMooverTime)
{
  char countdownbuf[30]; 
  const byte countdnStartPos = 12;
  byte highHrsPosOffset; // If hours are over 100, then you need to change hours starting column
  DateTime now = RTC.now();

  uint32_t countDownTime = nextMooverTime - now.unixtime();
  moover_sec = countDownTime % 60; // get seconds
  countDownTime /= 60; // convert to minutes
  moover_min = countDownTime % 60; // get minutes
  countDownTime /= 60; // convert to hours
  moover_hrs = countDownTime; // get hours

//  sprintf(countdownbuf, "Countdown: %02d:%02d:%02d    ", moover_hrs, moover_min, moover_sec);
//  Serial.print(countdownbuf);
//  sprintf(countdownbuf, "Time: %d/%d/%d %02d:%02d:%02d", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second());
//  Serial.println(countdownbuf);

  matrix.setTextColor(setDisplayColor(COLOR_COUNTDOWN));
  matrix.setCursor(14, 0);   
  matrix.print("Moover");
  
  // display hours
  if ( moover_hrs != moover_hrs_prev)
  {
    // Delete old hours digits
    matrix.setTextColor(0);  
    if ( moover_hrs_prev > 99 )
    { highHrsPosOffset = 5; }
    else
    { highHrsPosOffset = 0; }
    if (moover_hrs_prev % 10 == 1 )  // If right digit is a 1, then move hours to the right 1 row
    { matrix.setCursor(countdnStartPos + 1 - highHrsPosOffset, 9); }  
    else
    { matrix.setCursor(countdnStartPos - highHrsPosOffset, 9); }
    sprintf(countdownbuf, "%02d", moover_hrs_prev);
    matrix.print(countdownbuf);

    // display new hour
    if ( moover_hrs > 99 )
    { highHrsPosOffset = 5; }
    else
    { highHrsPosOffset = 0; }
    matrix.setTextColor(setDisplayColor(COLOR_COUNTDOWN));  
    if (moover_hrs % 10 == 1)    // If right digit is a 1, then move hours to the right 1 row
    { matrix.setCursor(countdnStartPos + 1 - highHrsPosOffset , 9); }  
    else
    { matrix.setCursor(countdnStartPos - highHrsPosOffset, 9); }
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
    matrix.setTextColor(setDisplayColor(COLOR_COUNTDOWN));  
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
  matrix.setTextColor(setDisplayColor(COLOR_COUNTDOWN));  
  matrix.setCursor(countdnStartPos + 28, 9);   
  sprintf(countdownbuf, "%02d", moover_sec);
  matrix.print(countdownbuf);
  
  moover_hrs_prev = moover_hrs;
  moover_min_prev = moover_min;
  moover_sec_prev = moover_sec;
  
}  // end displayCountdown()


void displayCurrentTime(unsigned int secondsToDisplay)
{
  DateTime now = RTC.now();

  char dispbuf[30];
  int hour12; 
  if (now.hour() == 0 )
  { hour12 = 12; }
  else if (now.hour() > 12 )
  { hour12 = now.hour() - 12; }
  else
  { hour12 = now.hour(); }
  sprintf(dispbuf, "%d:%02d", hour12, now.minute());

  int xPos;
  (hour12 >= 10) ? xPos = 17 : xPos = 20;
  matrix.fillScreen(CLEAR_DISP);
  matrix.setTextColor(setDisplayColor(COLOR_TIME)); 
  matrix.setCursor(20, 0); 
  matrix.print("Time");
  matrix.setCursor(xPos, 8); 
  matrix.print(dispbuf);
  delay(secondsToDisplay);

} // end displayCurrentTime()


// Start bus scrolling across the screen and play sound
void displayBus(bool withSound)
{
  int scrollDelay = 120;
  
  if (withSound) 
  {
    setSpeakerVolume(analogRead(VOLUMEPIN) / 11);  // lower number is higher volume
    
    // Start playing a file, then we can do stuff while waiting for it to finish
    // File names should be < 8 characters
    // moover1.mp3 is cow sound, arrive2x.mp3 is woman announcing moover
    char mp3FileName[] = "arrive2x.mp3";
    if (! musicPlayer.startPlayingFile(mp3FileName))
    { Serial.println(F("Could not open arrive2x.mp3 file on SD card")); }
    
    // while sound is playing, scroll the bus across the display
    while (musicPlayer.playingMusic ) 
    {
      for (int xpos = -27; xpos < 66; xpos++)
      {
        matrix.fillScreen(CLEAR_DISP); 
        matrix.drawBitmap(xpos++, 0, moover, 32, 16, setDisplayColor(COLOR_BUS) ); // draw bus
        delay(scrollDelay);
      }
    }  // end while playing loop
  } // end with sound
  else
  // don't play sound, only display bus scrolling on screen
  {
    for (int xpos = -27; xpos < 66; xpos++)
    {
      matrix.fillScreen(CLEAR_DISP);  
      matrix.drawBitmap(xpos++, 0, moover, 32, 16, setDisplayColor(COLOR_BUS) ); // draw bus
      delay(scrollDelay);
    }
  }      

  setSpeakerVolume(255); // Turn speaker off
  
}  // end displayBus()


// Display Happy New Year for first 10 minutes of the new year
void displayHappyNewYear()  
{
  
  matrix.setTextSize(2);  // large text

  const char  str[]   = "Happy New Year"; 
  static int  textX   = matrix.width(); 
  int       textMin = sizeof(str) * -12;
  static long   hue = 0;
  
  matrix.fillScreen(0); // Clear background

  // Draw big scrolly text on top
  matrix.setTextColor(matrix.ColorHSV(hue, 255, 255, true));
  matrix.setCursor(textX, 1);
  matrix.print(str);

  // Move text left (w/wrap), increase hue
  if((--textX) < textMin) textX = matrix.width();
  hue += 7;

  // Update display
  matrix.swapBuffers(false);

  matrix.setTextSize(1);  // go back to medium text

}  // displayHappyNewYear()


uint16_t setDisplayColor(colorme_t colorme)
{
  uint16_t returnColor;
  
  switch(colorme)
  {  
    case COLOR_BUS:
      returnColor = matrix.Color333(7,4,0);
      break;
    case COLOR_COUNTDOWN:
//      returnColor = matrix.Color333(0,7,0);
      returnColor = matrix.Color333(3,3,0);
      break;
    case COLOR_TIME:
      returnColor = matrix.Color333(3,3,0);
      break;
    default:
      returnColor = matrix.Color333(6,0,0);
      break;
  }
  return returnColor;
}  // setDisplayColor()


// Play two minute warning sound
// Note: for some reason the display flickers when the sound is playing, even if I comment out displayCountdown()
void playTwoMinWarning(time_t nextMooverTime)
{
  setSpeakerVolume(analogRead(VOLUMEPIN) / 11);  // lower number is higher volume

  // Start playing sound file
  // File names should be < 8 characters
  char mp3FileName[] = "twomin1.mp3";
  if (! musicPlayer.startPlayingFile(mp3FileName))
  { Serial.println(F("Could not open twomin1.mp3 file on SD card")); }

  // while sound is playing update the time
  while (musicPlayer.playingMusic )
  {
    displayCountdown(nextMooverTime);  
    delay(400);
  }

  setSpeakerVolume(255); // Turn speaker off

} // playTwoMinWarning()

// 255 is lowest volume, zero is highest
// There is hum when not playing anything, to avoid, disable audio amp
void setSpeakerVolume(byte volumeLevel)
{
  if ( volumeLevel == 255)
  { // turn off speaker
    musicPlayer.setVolume(255, 255);  
    digitalWrite(DISABLE_AUDIO, LOW);  // disable audio amp
  }
  else
  { // turn off speaker
    musicPlayer.setVolume(volumeLevel, volumeLevel);  
    digitalWrite(DISABLE_AUDIO, HIGH);  // enable audio amp
  }


}


