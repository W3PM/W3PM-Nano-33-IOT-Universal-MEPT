/*
  Si5351 bandswitched WSPR, FST4W, CW, and FSK(QRSS) source designed for 2200 - 10 meters 
  using an Arduino Nano 33 IoT. Internet time using  WiFi connection is used for timing.

  This sketch uses a frequency resolution down to 0.01 Hz to provide optimum
  reolution for the FST4W/WSPR tone separation specification. This limits
  the upper frequency to 40 MHz.

  This sketch is developed for use with the K4CLE circuit board. This board does
  not support FST4W 900 and 1800 operation. FST4W 900 and 1800 mode operation is
  dependent upon Si5351 oscillator and Nano 33 IoT millisecond timing stability.

  Permission is granted to use, copy, modify, and distribute this software
  and documentation for non-commercial purposes.

  Copyright (C) 2022,  Gene Marcus W3PM GM4YRE

  14 May, 2022

  8 JUL 2022   Inludes GPS timing
  22 JUl 2022  Include timing source selection at start up
  26 JUL 2022  Include CW and FSK
  30 JUL 2022  Include control of D3 for external CW keying
  3 AUG 2022   Now uses external EEPROMon K4CLE board
  5 AUG 2022   Modified CW transmit to use D3 to configure output keying
  6 AUG 2022   Fixed bug in CWtransmit
  8 AUG 2022   Added transmit offset to displayed frequency for WSPR and FST4W
  16 AUG 2022  Added solar weather data page
  22 AUG 2022  Added callsign and gridsquare editing and EEPROM storage.
  25 AUG 2022  Added GPS gridsquare calculation.
  27 AUG 2022  Fixed clock display and WiFi time set bugs
  4 SEP 2022   Now uses NTP time
  9 SEP 2022   Fixed minor scheduling bugs
  21 SEP 2022  Fixed TXband EEPROM bug

  TODO: Use algorithm to determine TXband length in setup function.


  The code was sucessfully compiled using Arduino 1.8.14 and 2.0.2


  IMPORTANT NOTE: The Arduino Nano 33 IoT is a 3.3V device. Refer to Arduino
                set up instructions prior to use with this sketch.



  ------------------------------------------------------------------------
  Nano Digital Pin Allocations follow:
  ------------------------------------------------------------------------
  TX
  RX    GPS serial input (9600)
  D2    1Hz output from DS3231
  D3
  D4
  D5
  D6    pushButton 1
  D7    pushButton 2
  D8    pushButton 3
  D9    pushButton 4
  D10   MENU
  D11
  D12
  D13

  A0/D14   XmitLED/PTT/external CW keying
  A1/D15
  A2/D16
  A3/D17
  A4/D18   Si5351, OLED, DS3231 SDA
  A5/D19   Si5351, OLED, DS3231 SCL


  ---------------------------------------------------------------------------------------------------------------
  Five pushbuttons are used to control the functions.
  Pushbutton pin allocations follow:
  ---------------------------------------------------------------------------------------------------------------
      TRANSMIT                  HOME SCREEN               BAND
  PB1   N/A                       N/A                       Decrease band
  PB2   Select HOME SCREEN        Turn transmitter ON/OFF   Depress with PB3 to save
  PB3   N/A                       N/A                       Depress with PB2 to save
  PB4   N/A                       N/A                       Increase band

      POWER                     TX INTERVAL               SET OFFSET
  PB1   Decrease power            Increase TX interval      increase offset
  PB2   Depress with PB3 to save  Depress with PB3 to save  Depress with PB3 to save
  PB3   Depress with PB2 to save  Depress with PB2 to save  Depress with PB2 to save
  PB4   Increase power            Decrease TX interval      Decrease offset

      TX HOPPING                  ANT TUNE      CLOCK       MODE
  PB1   "YES"    increase offset    N/A           N/A         Depress to select mode
  PB2   Depress with PB3 to save    N/A           N/A         Depress with PB3 to save
  PB3   Depress with PB2 to save    N/A           N/A         Depress with PB2 to save
  PB4   "NO"                        N/A           N/A         Depress to select mode

     FREQ CALIB
  PB1  Decrease frequency
  PB2  Depress with PB3 to save
  PB3  Change resolution / Depress with PB2 to save
  PB4  Increase frequency

  (Clock settings are only required if WiFi or GPS are not available)
      CLOCK SET                      DATE SET
  PB1  Time sync* / Set Hour          Set Day
  PB2  Set Minute                     Set Month
  PB3  N/A                            Set Year
  PB4  Hold to change time**          Hold to change date**
         Depress at top of the minute to synchronize
       ** Changes are saved when PB4 is released

       EDIT CALL                                  EDIT GRID
  PB1   Depress to change selected character       Depress to change selected character
  PB2   Depress with PB3 to save                   Depress with PB3 to save
  PB3   Depress with PB2 to save  N/A              Depress with PB2 to save
  PB4   Depress to select character                Depress to select character   
  Note: Use "#" to denote the end of edited call or gridsquare entries.  

  MENU - Select function / exit function and return to menu when not transmitting
*/



//____________________________________________________________________________________________________
//                      User configurable variables begin here
//____________________________________________________________________________________________________

//----------------------------------------------------------------------------------------------------
//                         WiFi configuration data follows:
//----------------------------------------------------------------------------------------------------
char ssid[] = "xxxx";       // Wifi SSID
char pass[] = "xxxx";       // Wifi password
int keyIndex = 0;           // Network key Index number (needed only for WEP)

//----------------------------------------------------------------------------------------------------
//                         WSPR configuration data follows:
//----------------------------------------------------------------------------------------------------
// Enter your callsign below:
// Note: Upper or lower case characters are acceptable. Compound callsigns e.g. W3PM/4 W4/GM4YRE
//       are supported.
char call2[12] = "wz9xyz";
char locator[8] = "fn21";

// Enter TX power in dBm below: (e.g. 0 = 1 mW, 10 = 10 mW, 20 = 100 mw, 30 = 1 W)
// Min = 0 dBm, Max = 43 dBm, steps 0,3,7,10,13,17,20,23,27,30,33,37,40,43
int  ndbm = 7;

// Enter desired transmit interval below:
// Avoid entering "1" as the unit will transmit every 2 minutes not allowing for
// autocal updates between transmissions
int TXinterval = 3; // Transmit interval e.g. 3 = transmit once every 3rd 2 minute transmit slot

// Enter desired transmit offset from dial frequency below:
// Transmit offset frequency in Hz. Range = 1400-1600 Hz (used to determine TX frequency within WSPR window)
unsigned long TXoffset = 1500UL;

// Enter desired FT8 transmit offset from dial frequency below:
// Transmit offset frequency in Hz. Range = 200-2500 Hz (used to determine FT8 TX frequency within WSPR window)
unsigned long FT8_TXoffset = 1000UL;

// Enter In-band frequency hopping option:
// This option ignores TXoffset above and frequency hops within the WSPR 200 Hz wide window
// Before using this option be sure the system is calibrated to avoid going outside band edges.
// In-band transmit frequency hopping? (true = Yes, false = No)
bool FreqHopTX = false;



//----------------------------------------------------------------------------------------------------
//                         CW/FSK configuration data follows:
//----------------------------------------------------------------------------------------------------
// Enter CW words per minute:
const int CWwpm = 8;

// Enter CW offset in Hz:
const int CWoffset = 600;

// Enter CW message - NOTE: All capital letters. Do NOT use punctuation or special characters:
char CWmessage[] = " WZ7XYZ AB55 ";

// Enter FSK offset in Hz:
const int FSKoffset = 5;

// Enter FSK message - end with period:
char FSKmessage[] = " WZ9XYZ AA00 ";

// Enter FSK dit length in seconds:
int FSKditDelay = 3;

/*
  ----------------------------------------------------------------------------------------------------
                       Scheduling configuration follows:
  ----------------------------------------------------------------------------------------------------
  Format: (TIME, MODE, BAND),

  TIME: WSPR & FST4W are in 2 minute increments past the top of the hour
        FST4W300 is in 5 minute increments past the top of the hour

  MODE: 1-WSPR 2-FST4W120 3-FSR4W300 4-CW 5-FSK(QRSS) 6-FT8

  BAND: is 'TXdialFreq' band number beginng with 0  i.e. 2 = 1836600 kHz



  Note: FST4W modes are normally used only on LF and MF

        Ensure times do not overlap when using FST4W300

        The K4CLE board does not support FST4W 900 and 1800 operation.

        Ensure format is correct
        i.e. brackets TIME comma MODE comma BAND comma brackets comma {24,1,11},


  example follows:

  const int schedule [][3] = {
  {0, 1, 1},   // WSPR at top of the hour on 474.200 kHz
  {2, 2, 0},   // FST4W120 at 2 minutes past the hour on 136.000 kHz
  {4, 1, 6},   // WSPR at 4 minutes past the hour on 10138.700 kHz
  {20, 3, 0},  // FST4W300 at 20 minutes past the hour on 136.000 kHz
  {58, 1, 11}, // WSPR at 58 minutes past the hour on 28124600 kHz
  (-1,-1,-1),  // End of schedule flag
  };
  ----------------------------------------------------------------------------------------------------
*/
const int schedule_1 [][3] = { // 630m WSPR, FST4W 120, and FST4W 300
  {0, 3, 1},
  {6, 2, 1},
  {8, 1, 1},
  {10, 3, 1},
  {16, 2, 1},
  {18, 1, 1},
  {20, 3, 1},
  {26, 2, 1},
  {28, 1, 1},
  {30, 3, 1},
  {36, 2, 1},
  {38, 1, 1},
  {40, 3, 1},
  {46, 2, 1},
  {48, 1, 1},
  {50, 3, 1},
  {56, 2, 1},
  {58, 1, 1},
  { -1, -1, -1},
};

const int schedule_2 [][3] = { // 10, 15, and 20m WSPR
  {10, 1, 10},
  {14, 1, 12},
  {18, 1, 14},
  {30, 1, 10},
  {34, 1, 12},
  {38, 1, 14},
  {50, 1, 10},
  {54, 1, 12},
  {58, 1, 14},
  { -1, -1, -1},
};

const int schedule_3 [][3] = { // WSPR band hopping 10 - 40m
  {0, 1, 8},
  {2, 1, 9},
  {4, 1, 10},
  {6, 1, 8},
  {8, 1, 9},
  {10, 1, 10},
  {12, 1, 11},
  {14, 1, 12},
  {16, 1, 13},
  {18, 1, 14},
  {20, 1, 8},
  {22, 1, 9},
  {24, 1, 10},
  {26, 1, 8},
  {28, 1, 9},
  {30, 1, 10},
  {32, 1, 11},
  {34, 1, 12},
  {36, 1, 13},
  {38, 1, 14},
  {40, 1, 8},
  {42, 1, 9},
  {44, 1, 10},
  {46, 1, 8},
  {48, 1, 9},
  {50, 1, 10},
  {52, 1, 11},
  {54, 1, 12},
  {56, 1, 13},
  {58, 1, 14},
  (-1, -1, -1),
};

const unsigned long TXdialFreq [] =
{
  136000  , // Band 0
  474200  , // Band 1
  1836600 , // Band 2
  1836800 , // Band 3
  3568600 , // Band 4
  3592600 , // Band 5
  5287200 , // Band 6
  5364700 , // Band 7
  7038600 , // Band 8
  10138700, // Band 9
  14095600, // Band 10
  18104600, // Band 11
  21094600, // Band 12
  24924600, // Band 13
  28124600, // Band 14
  40620000, // Band 15 - 8 meter WSPR
  40680000, // Band 16 - 8 meter WSPR & FT8
  3569950,  // Band 17 - QRSS (mid band)
  7039950,  // Band 18 - QRSS (mid band)
  10140050, // Band 19 - QRSS (mid band)
  14096950, // Band 20 - QRSS (mid band)
  28074000, // Band 21 - 10 meter FT8
  0
};



/*                 FST4W Symbol Configuration
   ----------------------------------------------------------------------------------------------------
    Open Windows Command Prompt. Navigate to directory C:\WSJTX\bin>. Enter the folling command
    using your own callsign, grid squate , and power level(dBM) enclosed in quotes as follows:

    fst4sim "WZ9ZZZ FN21 30" 120 1500 0.0 0.1 1.0 0 99 F > FST4W_message.txt"

    This will create a file named "FST4W_message.txt" in C:\WSJTX\bin. Open the "FST4W_message.txt"
    file into a text editor. Separate the "Channel symbols:" data with commas. Replace the following
    data with the comma separated data:
  ----------------------------------------------------------------------------------------------------
*/
// Load FST4W channel symbols for WZ9ZZZ FN21 30dBm
const int  FST4Wsymbols[160] = {
  0, 1, 3, 2, 1, 0, 2, 3, 2, 3,
  3, 2, 3, 2, 3, 3, 0, 3, 1, 1,
  1, 2, 1, 1, 0, 0, 3, 3, 0, 3,
  3, 1, 0, 1, 3, 3, 0, 1, 2, 3,
  1, 0, 3, 2, 0, 1, 2, 0, 1, 0,
  3, 3, 1, 3, 3, 2, 3, 3, 0, 0,
  1, 2, 1, 1, 3, 0, 3, 2, 0, 3,
  2, 0, 3, 3, 0, 0, 0, 1, 3, 2,
  1, 0, 2, 3, 2, 2, 3, 1, 0, 2,
  2, 0, 2, 2, 2, 1, 0, 1, 0, 0,
  3, 1, 2, 0, 0, 3, 1, 0, 2, 2,
  1, 2, 2, 2, 2, 3, 1, 0, 3, 2,
  0, 1, 2, 1, 3, 2, 3, 3, 3, 2,
  1, 0, 1, 0, 2, 2, 3, 0, 2, 3,
  3, 0, 2, 0, 0, 1, 1, 3, 2, 1,
  1, 0, 0, 1, 3, 2, 1, 0, 2, 3
};


/*                       FT8 Symbol Configuration
   ----------------------------------------------------------------------------------------------------
    Open Windows Command Prompt. Navigate to directory C:\WSJTX\bin>. Enter the folling command
    using your own callsign and grid squre enclosed in quotes as follows:

    ft8code "CQ WZ9ZZZ AA99"  > FT8_message.txt"

    This will create a file named "FT8_message.txt" in C:\WSJTX\bin. Open the "FST4W_message.txt"
    file into a text editor. Separate the "Channel symbols:" data with commas. Replace the following
    data with the comma separated data:
  ----------------------------------------------------------------------------------------------------
*/
// Load FT8 channel symbols for "CQ WZ9ZZZ AA99"
const int FT8symbols[79] = {
  3, 1, 4, 0, 6, 5, 2, 0, 0, 0,
  0, 0, 0, 0, 0, 1, 1, 4, 6, 4,
  7, 3, 1, 1, 3, 7, 0, 0, 0, 2,
  0, 4, 3, 2, 1, 7, 3, 1, 4, 0,
  6, 5, 2, 0, 5, 7, 5, 1, 4, 0,
  7, 1, 7, 0, 0, 7, 4, 3, 5, 7,
  3, 0, 5, 4, 7, 4, 1, 7, 2, 0,
  0, 7, 3, 1, 4, 0, 6, 5, 2
};



//____________________________________________________________________________________________________
//                      User configurable variables end here
//____________________________________________________________________________________________________



//_________________________________________________________________________________________________

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FlashStorage_SAMD.h>
#include <WiFiNINA.h>
//#include <WiFiUdp.h>
#include <RTCZero.h>
#include <TinyGPS++.h>
#include <NTPClient.h>
#include <TimeLib.h>

// Object for Real Time Clock
RTCZero rtc;

int status = WL_IDLE_STATUS;

#define SCREEN_WIDTH 128 // display display width, in pixels
#define SCREEN_HEIGHT 64 // display display height, in pixels

#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 4 pin OLED
//#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 4 pin OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Set DS3231 I2C address
#define DS3231_addr           0x68

// Set external EEPROM address
//#define EEPROMaddress 0x54    //Address of 24LC64 eeprom chip on K4CLE board

// The TinyGPS++ object
TinyGPSPlus gps;

// Initialize the client object and WiFi data
WiFiClient client;
const char server_qsl[] = "www.hamqsl.com";
#define router "192.168.0.1";

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");


// Load WSPR/FST4W symbol frequency offsets
const int OffsetFreq[4] [4] = {
  { -219, -73, 73 , 219}, // 120
  { -84, -28, 28, 84},    // 300
  { -27, -9, 9, 27},      // 900
  { -13, -4, 4, 13}       // 1800
};

// Load FT8 symbol frequency offsets
const int FT8_OffsetFreq[8] {0, 625, 1250, 1875, 2500, 3125, 3750, 4375};

// Load WSPR/FST4W symbol length
unsigned long SymbolLength[4] = {
  683,    // 120
  1792,   // 300
  5547,   // 900
  11200   // 1800
};

// Load FT8 symbol length
unsigned long FT8_SymbolLength = 159;

const int T_R_time[4] = {
  120,
  300,
  900,
  1800
};

const char SyncVec[162] = {
  1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0,
  1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0,
  0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0,
  0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0
};


const byte Morse[38] = {
  B11111101,  //  0
  B01111101,  //  1
  B00111101,  //  2
  B00011101,  //  3
  B00001101,  //  4
  B00000101,  //  5
  B10000101,  //  6
  B11000101,  //  7
  B11100101,  //  8
  B11110101,  //  9
  B01000010,  //  A
  B10000100,  //  B
  B10100100,  //  C
  B10000011,  //  D
  B00000001,  //  E
  B00100100,  //  F
  B11000011,  //  G
  B00000100,  //  H
  B00000010,  //  I
  B01110100,  //  J
  B10100011,  //  K
  B01000100,  //  L
  B11000010,  //  M
  B10000010,  //  N
  B11100011,  //  O
  B01100100,  //  P
  B11010100,  //  Q
  B01000011,  //  R
  B00000011,  //  S
  B10000001,  //  T
  B00100011,  //  U
  B00010100,  //  V
  B01100011,  //  W
  B10010100,  //  X
  B10110100,  //  Y
  B11000100,  //  Z
  B00000000,  //  space
  B10010101   // "/"
};

// Set up MCU pins
#define InterruptPin             2
#define extCWsocket              3
#define CFup                     7
#define CFdown                   6
#define calibrate                9
#define endCalibrate             8
//#define NanoLED                 13
#define XmitLED                 14
#define MinLED                  15
#define SecLED                  16
#define pushButton1              6
#define pushButton2              7
#define pushButton3              8
#define pushButton4              9
#define menu                    10

// Set sI5351A I2C address
#define Si5351A_addr          0x60

// Define Si5351A register addresses
#define CLK_ENABLE_CONTROL       3
#define CLK0_CONTROL            16
#define CLK1_CONTROL            17
#define CLK2_CONTROL            18
#define SYNTH_PLL_A             26
#define SYNTH_PLL_B             34
#define SYNTH_MS_0              42
#define SYNTH_MS_1              50
#define SYNTH_MS_2              58
#define SSC_EN                 149
#define PLL_RESET              177
#define XTAL_LOAD_CAP          183


typedef struct {  // Used in conjunction with PROGMEM to reduce RAM usage
  char description [4];
} descriptionType;


// configure variables
bool data_changeFlag = false, clockTimeChange = false, TransmitFlag = true, suspendUpdateFlag = false, scrollFlag = false, MenuExitFlag = true;
bool toggle = false, WSPR_toggle = false, timeSet_toggle = false, StartCalc = true, startFlag = true;
byte sym[170], c[11], symt[170], calltype, msg_type, tcount3;
volatile byte ii;
int nadd, nc, n, ntype, TXcount, TXband, sats;
int p, m, dBm, AutoCalFactor[50], CFcount, tcount2, SWRval, column;
int hours, minutes, seconds, xday, xdate, xmonth, xyear, startCount = 0;
int GPS_hours, GPS_minutes, GPS_seconds, character, i, byteGPS = -1;
char call1[7], grid4[5];
long MASK15 = 32767, ihash, lastMsecCount, resolution = 10;
unsigned long XtalFreq = 25000000, tcount = 2, time_now = 0, LEDtimer, epoch;
unsigned long t1, ng, n2, cc1, n1,  mult = 0;
const descriptionType daysOfTheWeek [7] PROGMEM = { {"SUN"}, {"MON"}, {"TUE"}, {"WED"}, {"THU"}, {"FRI"}, {"SAT"},};
const descriptionType monthOfTheYear [12] PROGMEM = { {"JAN"}, {"FEB"}, {"MAR"}, {"APR"}, {"MAY"}, {"JUN"}, {"JUL"}, {"AUG"}, {"SEP"}, {"OCT"}, {"NOV"}, {"DEC"},};
const byte daysInMonth [] PROGMEM = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
byte Buffer[10], bufCounter, tempBuf, mode = 1, TransmitMode ;
int lastSecond, MsecCF, symbolTime, T_R_period, CalFactor, schedule [31][3], TempSeconds;
char buf[11], *TimeSource[5];
bool menuFlag, WiFiStatus, validGPSflag = false, timeAdjust_toggle = false, doOneTimeTrigger = true, GPSselectFlag = false;
static const int RXPin = 1, TXPin = 0;
static const uint32_t GPSBaud = 9600;
char MorseChar, EW, NS;;
int DitDelay, DahDelay, SymbolDelay, LetterDelay, WordDelay;
int FSKdahDelay, FSKsymbolDelay, FSKletterDelay, TickCounter;

char *ptr1_target;
char *ptr_test;
char term = '\0';   // add to terate character arraies
char *d;
int addrs, actionFlag;

int Lat10, Lat1, Lon100, Lon10, Lon1;
int dLon1, dLon2, dLat1, dLat2, dlon, sat1, sat10;





//***********************************************************************
// This interrupt is used Nano 33 IoT millisecond calibration
// Called every second by from the DS3231 RTC 1PPS to Nano pin D2
//***********************************************************************
void Interrupt()
{
  tcount++;
  if (tcount == 100)
  {
    MsecCF = millis() - lastMsecCount;
    lastMsecCount = millis();
    tcount = 0;
  }
  TickCounter++;
}


//******************************************************************
//  findIt alogrithm follows:
//
//  Attributed to Richard H. Grote, K6PBF
//  ATWIFI Clock/Weather/Solar Display
//  QEX May/June 2022
// 
//
//  Called by solar_conditions()
//******************************************************************

/* findIt routine for parsing data from an Internet stream.

  target -- search key
  lgt -- length of return bytes if search code =1 choosen
  skip -- number of characters skipped after key matched before
        characters are returned.  Used to save memory
  search_code -
  0: return character sequence, exit
  1: return float and exit
  2: return int and exit
  return type -
  0: character buffer
  1: float number
  2: integer number
  data_float - return address of returned float data
  integer_float - return address of returned float data

  Routine exits as soon as match as been determined and desired
  data returned.  Routine can be called multiple times to
  find sequential data.
*/

char * findIt (char *target, int lgt, int skip = 0,
               int search_code = 0, int return_type = 0, float *data_float = 0,
               int *data_int = 0) {

  // up to 10 characters can be returned
  // static character definition so can be accessed outside of function.

  static char c[11] = "         " ;


  int status_code = 0;
  char *ptr_c;
  char *ptr_target;
  char web_Data;
  ptr_target = &target[0];
  bool flag = false;
  int test_counter = 0;

  char quot = '"';
  char xmp_left = '>';
  char xmp_right = '<';
  static char nothing_found[4] = {' ', '-', ' ', '\0'}; //returned when search item isn't found in search mode 1 or 2

  int lgth_target = strlen (target);
  ptr_c = &c[0];         // Characters captured in c[]
  memset(c, ' ', 11); // clear out character buffer
  int x = 0;   //counter of unknown

  /*  Status code used in switch commands:
    0 no match
    1 match of some key characters
    2 key match complete
    3 # characters to skip, search mode = 0
    4 use "" to isolate data; search mode = 1
    5 use <> to isolate data; search mode = 2
    6 decide which format to return data

  */
  // start reading data and searching

  while ((status_code != 6) && client.available() != 0) {
    web_Data = client.read();
    //Serial.print(web_Data);  // use to print out data being read from website
    switch (status_code) {

      case 0:
        if (*ptr_target != web_Data) { // no match
          test_counter ++;   //increment test counter
          break;
        }
        else {
          status_code = 1;
          ptr_target++;
          x++;
          break;
        }

      case 1: {

          x++;

          if (*ptr_target == web_Data &&  (x < lgth_target)) {
            // x++;
            ptr_target++;
            break;
          }
          if (*ptr_target != web_Data && (x < lgth_target)) {
            // incomplete match--reset for new search
            status_code = 0;
            x = 0;          //reset counter and target
            ptr_target = &target[0];
            break;
          }

          if (x == lgth_target) {

            if  (skip != 0) {

              status_code = 2;
              x = 0;
              break;
            }
            if (search_code == 0) {   //load number of specified characters
              status_code = 3;
              break;
            }
            if (search_code == 1) {   //load characters between **'s
              test_counter = 0;
              status_code = 4;
              break;
            }
            if (search_code == 2) {   // load characters between "<>"
              x = 0;
              status_code = 5;
              break;
            }
          }

        }

      case 2:  {          // skip characters

          if (x < skip) {
            ptr_target++;
            x++;
            break;
          }
          x = 0;    //skip complete or no skip requested

          if (search_code == 0) {   //load number of specified characters
            status_code = 3;
            break;
          }
          if (search_code == 1) {   //load characters between **'s
            status_code = 4;
            break;
          }
          if (search_code == 2) {   // load characters between "<>"
            status_code = 5;
            break;
          }

        }

      case 3: {

          if (x < lgt ) {
            *ptr_c = web_Data;   // add the data to the string
            x++;
            ptr_c++;
            break;
          }
          else {
            *ptr_c = term;  //terminate character array
            status_code = 6;
            break;
          }
        }
      case 4: {
          if (web_Data == quot && flag == false) {
            flag = true;
            break;
          }

          if (flag == true && web_Data != quot) {
            *ptr_c = web_Data;
            ptr_c++;
            break;
          }
          if (flag == true && web_Data == quot) {
            flag = false;
            *ptr_c = term;
            status_code = 6;  //exit routine
            break;
          }
        }

      case 5: {
          if (web_Data != xmp_left && flag == false) {  // '>' detected, start collecting
            break;
          }

          if (web_Data == xmp_left && flag == false) {
            flag = true;
            break;
          }

          if (web_Data != xmp_right && flag == true) {
            *ptr_c = web_Data;
            ptr_c++;
            break;

          }
          if (flag == true && web_Data == xmp_right) {
            status_code = 6;  //exit routine
            break;
          }
        }

    }
  }

  if (client.available() == 0) {
    Serial.println(F("ran through webstrean without target match"));  // prints message when search passes through string without match
    return nothing_found;   // got to this point without finding the target word--return character " - "

  }
  if (return_type == 0) {  //case 6 return character pointer

    return c;


  }
  if (return_type == 1 ) {     //return float
    double temp = atof(c);

    float temp1 =  temp;
    *data_float = temp1;

    return c;

  }

  *data_int = atoi(c);

  return c;
}

//=============================================================================================
void setup() {
  delay(2000);

  Serial.begin(115200);
  Serial1.begin(GPSBaud);

  Wire.begin();                                 // join I2C bus

  //  Wire.begin();
  // Initialize the Si5351
  Si5351_write(XTAL_LOAD_CAP, 0b11000000);      // Set crystal load to 10pF
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000000); // Enable CLK0, CLK1 and CLK2
  Si5351_write(CLK0_CONTROL, 0b01001111);       // Set PLLA to CLK0, 8 mA output, INT mode
  Si5351_write(CLK1_CONTROL, 0b01101111);       // Set PLLB to CLK1, 8 mA output, INT mode
  Si5351_write(CLK2_CONTROL, 0b01101111);       // Set PLLB to CLK2, 8 mA output, INT mode
  Si5351_write(PLL_RESET, 0b10100000);          // Reset PLLA and PLLB
  Si5351_write(SSC_EN, 0b00000000);            // Disable spread spectrum
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // Disable clocks

  // Set up DS3231 for 1 Hz squarewave output
  // Needs only be written one time provided DS3231 battery is not removed
  Wire.beginTransmission(DS3231_addr);
  Wire.write(0x0E);
  Wire.write(0);
  Wire.endTransmission();

  // Inititalize 1 Hz input pin
  pinMode(InterruptPin, INPUT);
  digitalWrite(InterruptPin, LOW);              // internal pull-up enabled

  // Set pin 2 for external 1 Hz interrupt input
  attachInterrupt(digitalPinToInterrupt(InterruptPin), Interrupt, FALLING);


  // Set up push buttons
  pinMode(pushButton1, INPUT);
  digitalWrite(pushButton1, HIGH);      // internal pull-up enabled
  pinMode(pushButton2, INPUT);
  digitalWrite(pushButton2, HIGH);      // internal pull-up enabled
  pinMode(pushButton3, INPUT);
  digitalWrite(pushButton3, HIGH);      // internal pull-up enabled
  pinMode(pushButton4, INPUT);
  digitalWrite(pushButton4, HIGH);      // internal pull-up enabled
  pinMode(menu, INPUT);
  digitalWrite(menu, HIGH);             // internal pull-up enabled
  pinMode(extCWsocket, INPUT);
  digitalWrite(extCWsocket, HIGH);      // internal pull-up enabled


  // Set up LEDs
  pinMode(LED_BUILTIN, OUTPUT);             // CW monitor
  pinMode(XmitLED, OUTPUT);              // Use with dropping resistor on pin D14
  digitalWrite(XmitLED, LOW);
  pinMode(SecLED, OUTPUT);               // Use with dropping resistor on pin D16
  digitalWrite(SecLED, LOW);
  pinMode(MinLED, OUTPUT);               // Use with dropping resistor on pin D15
  digitalWrite(MinLED, LOW);


  // Ensure input data is upper case
  for (int i = 0; i < 11; i++)if (call2[i] >= 97 && call2[i] <= 122)call2[i] = call2[i] - 32;
  for (int i = 0; i < 7; i++)if (locator[i] >= 97 && locator[i] <= 122)locator[i] = locator[i] - 32;

  // Get stored WSPR band
  TXband = EEPROM.read(0);
  if (TXband > 22 | TXband < 0) TXband = 10; //Ensures valid EEPROM data - if not valid will default to 20m

  // Get stored transmit interval
  EEPROM.get(10, TXinterval);
  if (TXinterval > 30 | TXinterval < 0) TXinterval = 2; //Ensures valid EEPROM data - if not valid will default to 2

  // Get stored TX hopping variable
  EEPROM.get(20, FreqHopTX);
  if (FreqHopTX < 0 | FreqHopTX > 1) FreqHopTX = false; // Set to false if out of bounds

  // Get stored TX offset variable
  EEPROM.get(30, TXoffset);
  if (TXoffset < 1400 | TXoffset > 1600) TXoffset = 1500UL;  // Set to 1500 if out of bounds

  // Get stored mode
  EEPROM.get(40, mode);
  if (mode < 0 | mode > 9) mode = 1;                 // Set to 1 (WSPR) if out of bounds
  if (mode < 3) T_R_period = 0;                      // Set T_R_period to 120 sec if WSPR or FST4W120
  if (mode > 2 | mode < 6) T_R_period = mode - 2;    // Set T_R_period for FST4W 300, 900, 1800
  switch (mode)
  {
    case 7:
      memmove(schedule, schedule_1, sizeof(schedule_1));
      break;

    case 8:
      memmove(schedule, schedule_2, sizeof(schedule_2));
      break;

    case 9:
      memmove(schedule, schedule_3, sizeof(schedule_3));
      break;
  }

  // Get stored Si5351 Xtal frequency calibration factor
  EEPROM.get(50, CalFactor);
  if ((CalFactor >> 15) == 1) CalFactor = CalFactor - 65536;
  if (CalFactor < -5000 | CalFactor > 5000) CalFactor = 0; //Set to zero if out of bounds
  XtalFreq = XtalFreq + CalFactor;

  // Get stored Si5351 power in dBm
  EEPROM.get(60, ndbm);
  if (ndbm < 0 | ndbm > 40) ndbm = 7;  // Set to 5 mW if out of bounds

  // Get stored callsign
  actionFlag = EEPROM.read(120);
  if (actionFlag == 1)
  {
    for (int i = 0; i < 12; i++) 
    {
     call2[i] = EEPROM.read(70 + (i * 2));
     if (call2[i] == 0x00 | call2[i] == 35) i = 12;  
    }
  }

  // Get stored gridsquare
  actionFlag = EEPROM.read(122);
  if (actionFlag == 1)
  {
    for (int i = 0; i < 12; i++) 
    {
     locator[i] = EEPROM.read(100 + (i * 2));
     if (locator[i] == 0x00 | locator[i] == 35) i = 12;  
    }
  }


  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  //if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
  //  Serial.println(F("SSD1306 allocation failed"));
  //  for (;;); // Don't proceed, loop forever
  //}

  DitDelay = 60000 / (CWwpm * 50);
  DahDelay = DitDelay * 3;
  SymbolDelay = DitDelay;
  LetterDelay = DitDelay * 3;
  WordDelay = DitDelay * 4;

  FSKditDelay = FSKditDelay * 1000;
  FSKdahDelay = FSKditDelay * 3;
  FSKsymbolDelay = FSKditDelay;
  FSKletterDelay = FSKditDelay * 3;

  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE, BLACK);

  rtc.begin();

  timeClient.begin();

  // Generate unique WSPR message for your callsign, locator, and power
  wsprGenCode(); // WSPR message calculation

  // Store time reference for sketch timing i.e. delays, EEPROM writes, display toggles
  // The Arduino "delay" function is not used because of critical timing conflicts
  time_now = millis();

}

//=============================================================================================

void loop() {

  // Timing source selection
  // Loop display menu and loop until a selection is made
  // If no input exit after 30 seconds and default to RTC time (power failure mide)
  if (doOneTimeTrigger == true)
  {
    display.setCursor(0, 0);
    display.println(F("Timing:"));
    display.println(F("PB1 - WiFi"));
    display.println(F("PB2 - GPS "));
    display.println(F("PB3 - RTC "));
    display.display();

    while (doOneTimeTrigger == true & digitalRead(pushButton1 == HIGH) & digitalRead(pushButton2 == HIGH) & digitalRead(pushButton3 == HIGH))
    {
      if (millis() > time_now + 30000)
      {
        display.clearDisplay();
        doOneTimeTrigger = false; // timed out exit default to RTC
      }
    }

    if (digitalRead(pushButton1) == LOW)
    {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print(F("Connect to"));
      display.print(ssid);
      display.setCursor(0, 48);
      display.print(F("-WAITING-"));
      display.display();

      long tempTimer = millis() + 8000L;
      while (tempTimer > millis())
      {
        status = WiFi.begin(ssid, pass);
        if (status == WL_CONNECTED) tempTimer = 0;
      }

      // Print connection status
      printWiFiStatus();

      // Use internet time to update DS3231 RTC
      if (WiFiStatus == true)
      {
        updateRTCtime();
        saveDate();
        timeSet_toggle = true;
        //TimeSource[5] = "WiFi";
        doOneTimeTrigger = false;
      }
      else
      {
        display.clearDisplay();
        //TimeSource[5] = "RTC ";
        doOneTimeTrigger = false;
      }
    }

    if (digitalRead(pushButton2) == LOW)
    {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("GPS CHECK"));
      display.display();
      for (int i = 120; i > 0; i--)
      {
        while (Serial1.available() > 0)
        if (gps.encode(Serial1.read())) SetGPStime();
        display.setCursor(0, 16);
        if (GPS_hours < 10) display.print(F("0"));
        display.print(GPS_hours);
        display.print(F(":"));
        if (GPS_minutes < 10) display.print(F("0"));
        display.print(GPS_minutes);
        display.print(F(" "));
        if (GPS_seconds < 10) display.print(F("0"));
        display.print(GPS_seconds);
        display.display();
        display.setCursor(0, 32);
        display.print(F("Sats = "));
        display.print(sats);
        display.setCursor(42, 48);
        display.print(F("   "));
        display.setCursor(42, 48);
        display.print(i);
        altDelay(1000);
        display.display();
        if (sats > 4)
        {
          validGPSflag = true;
          i = -1;
          display.setCursor(0, 16);
          display.print(F("GPS VALID "));
          display.display();
          updateRTCtime();
          timeSet_toggle = true; // Enable RTC time update from GPS time at new UTC day
          altDelay(5000);
          tinyGPS_calcGridSquare();
          GPSselectFlag = true;
          display.clearDisplay();
          doOneTimeTrigger = false;
        }
      }
      if (validGPSflag == false)
      {
        display.setCursor(0, 0);
        display.print(F("GPS FAILED"));
        display.setCursor(0, 32);
        display.println(F("DEFAULT TO"));
        display.println(F(" RTC TIME "));
        display.display();
        altDelay(5000);
        display.clearDisplay();
        //TimeSource[5] = "RTC ";
        doOneTimeTrigger = false;
      }
    }

    if (digitalRead(pushButton3) == LOW)
    {
      // Start Real Time Clock
      //rtc.begin();
      display.clearDisplay();
      //TimeSource[5] = "RTC ";
      doOneTimeTrigger = false;
    }
  }

  else
  {
    // This displays information every time a new sentence is correctly encoded.
    if (GPSselectFlag == true)
    {
      while (Serial1.available() > 0)
        if (gps.encode(Serial1.read()))
          SetGPStime();
    }

    // Loop through the  menu rota until a selection is made
    while (digitalRead(pushButton2) == HIGH & startFlag == false)
    {
      //if (digitalRead (menu) == LOW) menuFlag = true;
      if (digitalRead(pushButton3) == LOW | (digitalRead (menu) == LOW))
      {
        altDelay(500);
        startCount = startCount + 1;
        if (startCount > 14) startCount = 0;
      }

      switch (startCount) {

          DisplayText("          ", 0 , 0);
        case 1:
          DisplayText("  CLOCK   ", 0, 0); //("text",col,row)
          break;
        case 0:
          DisplayText(" TRANSMIT ", 0, 0); //("text",col,row)
          break;
        case 2:
          DisplayText("  BAND   ", 0, 0); //("text",col,row)
          break;
        case 3:
          DisplayText("EDIT POWER", 0, 0); //("text",col,row)
          break;
        case 4:
          DisplayText("TX INTRVAL", 0, 0); //("text",col,row)
          break;
        case 5:
          DisplayText("SET OFFSET", 0, 0); //("text",col,row)
          break;
        case 6:
          DisplayText("TX HOPPING", 0, 0); //("text",col,row)
          break;
        case 7:
          DisplayText(" ANT TUNE ", 0, 0); //("text",col,row)
          break;
        case 8:
          DisplayText("  MODE    ", 0, 0); //("text",col,row)
          break;
        case 9:
          DisplayText("FREQ CALIB", 0, 0); //("text",col,row)
          break;
        case 10:
          DisplayText("SET CLOCK ", 0, 0); //("text",col,row)
          break;
        case 11:
          DisplayText(" SET DATE ", 0, 0); //("text",col,row)
          break;
        case 12:
          DisplayText("EDIT CALL ", 0, 0); //("text",col,row)
          break;
        case 13:
          DisplayText("EDIT GRID ", 0, 0); //("text",col,row)
          break;  
        case 14:
          DisplayText("SOLAR DATA", 0, 0); //("text",col,row)
          break;
      }
    }

    if (startFlag == false)
    {
      display.clearDisplay();
      startFlag = true;
    }
    switch (startCount) {
      case 1: // Clock display algorithm begins here:
        DisplayClock();
        break;

      case 0: // Begin WSPR processing
        transmitPrep();
        break;

      case 2: // Select band
        changeBand();
        break;

      case 3: // Enter power level adjustment begins here:
        EditPower();
        break;

      case 4: // Enter transmit interval begins here:
        EditTXinterval();
        break;

      case 5: // Transmit offset frequency adjustment begins here:
        EditTXoffset();
        break;

      case 6: // Transmit TX frequency hopping selection begins here:
        EditTXhop();
        break;

      case 7: // Antenna tune algorithm begins here:
        AntennaTune();
        break;

      case 8: // Mode selection algorithm begins here:
        ModeSelect();
        break;

      case 9: // 25MHz frequency calibration algorithm begins here:
        XtalFreqCalibrate();
        break;

      case 10: // RTC clock adjustment begins here:
        adjClock();
        break;

      case 11: // RTC date adjustment begins here:
        setDATE();
        break;

      case 12: // Callsign editing begins here:
        EditCall();
        break;        

      case 13: // Gridsquare editing begins here:
        EditGrid();
        break;

      case 14: // Solar condition updates begin here:
        solar_conditions();
        break;

     // Selection is made at this point, now go to the selected function
    }

    if (digitalRead (menu) == LOW)
    {
      altDelay(300);
      menuFlag = false;
      startFlag = false;
      suspendUpdateFlag = false;
      data_changeFlag = false;
      column = 0;
      character = 0;
      digitalWrite(XmitLED, LOW);
      display.clearDisplay();
      display.setCursor(0, 32);
      //   display.println(F("PB3 scroll"));
      display.print(F("PB2 select"));
      display.display();

    }
    if (WiFiStatus == true | validGPSflag == true)
    {
      if (hours == 23 & minutes == 57 & seconds == 0 & timeSet_toggle == true)
      {
        updateRTCtime();
        timeSet_toggle = false; // Set to prevent multiple writes to DS3231 RTC
      }
      if (hours == 23 & minutes == 57 & seconds == 2) timeSet_toggle == true;
    }
  }
}


//******************************************************************
//  GPS time follows:
//  Used to set time and determine satellite count when using GPS
//
//  Called by loop()
//******************************************************************
void SetGPStime()
{
  if (gps.time.isValid())
  {
    GPS_seconds = gps.time.second();
    GPS_minutes = gps.time.minute();
    GPS_hours   = gps.time.hour();
    sats        = gps.satellites.value();
    if (sats > 4) validGPSflag = true;
    else validGPSflag = false;
  }
}



//******************************************************************
//  Edit callsign follows:
//  Used to change callsign
//
//  menu - exit
//  PB1  - edit selected character 
//  PB2  - press with PB3 to save
//  PB3  - press with PB2 to save
//  PB4  - select character
//
//  Note: Enter "#" at the end of the call before saving
//
//  Called by loop()
//******************************************************************
void EditCall()
{
  if (data_changeFlag == false)
  {
    DisplayText("menu: exit", 0, 2);
    DisplayText("2:3 - save", 0, 3);
    char temp = 35;   // Set to "#" to indicate end of text
    display.setCursor((column * 12), 0);
    display.print("_");
    display.setCursor(0, 16);
    display.print(call2);
    display.display();

    if (digitalRead(pushButton1) == LOW)
    {
      character++;
      altDelay(300);
      if ((character >= 0) & (character < 47)) character = 47;
      if ((character > 57) & (character < 65)) character = 65;
      call2[column] = character;

      if (character > 90)
      {
        call2[column] = 35;
        character = 0;
      }

    }
    if (digitalRead(pushButton4) == LOW)
    {
      altDelay(300);
      display.setCursor((column * 12), 0);
      display.print(" ");
      display.display();
      column++;
      character = call2[column];
      if (column > 10) column = 0;
    }
    if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW))
    {
      for (int i = 0; i < 12; i++)
      {
        if (call2[i] == temp | call2[i] == 0x00) data_changeFlag = true;
        if (data_changeFlag == true) call2[i] = 0x00;
      }
      if (data_changeFlag == true)
      {
        display.setCursor(0, 0);
        display.print(call2);
        display.setCursor(0, 16);
        display.print(F("  SAVED  "));
        display.display();
        for (int i = 0; i < 12; i++)
        { 
          EEPROM.put(70 + (i * 2), call2[i]);
          if (call2[i] == temp | call2[i] == 0x00) i = 12;
        }
        EEPROM.put(120, 1);     // Now set callsign change flag
        altDelay(4000);
      }
      else
      {
        display.setCursor(0, 0);
        display.print(F("NOT SAVED "));
        display.setCursor(0, 16);
        display.print(F("END WITH #"));
        display.display();
        //display.print(temp);
        altDelay(4000);
      }
      column = 0;
      display.setCursor(0, 0);
      display.print(F("          "));
      display.setCursor(0, 16);
      display.print(call2);
      display.print(F("          "));
      display.display();
      character = 0;
      data_changeFlag = false;
    }
  }

}


//******************************************************************
//  Edit grid square follows:
//  Used to change grid square
//
//  menu - exit
//  PB1  - edit selected character 
//  PB2  - press with PB3 to save
//  PB3  - press with PB2 to save
//  PB4  - select character
//
//  Note: Enter "#" at the end of the grid square before saving
//
//  Called by loop()
//******************************************************************
void EditGrid()
{
  if (data_changeFlag == false)
  {
    DisplayText("menu: exit", 0, 2);
    DisplayText("2:3 - save", 0, 3);
    char temp = 35; // Set to "#" to indicate end of text
    display.setCursor((column * 12), 0);
    display.print("_");
    display.setCursor(0, 16);
    display.print(locator);
    display.display();

    if (digitalRead(pushButton1) == LOW)
    {
      altDelay(300);
      character++;
      if ((character > 0) & (character < 47)) character = 47;
      if ((character > 57) & (character < 65)) character = 65;
      locator[column] = character;
      if (character > 90)
      {
        locator[column] = 35;
        character = 0;
      }

    }
    if (digitalRead(pushButton4) == LOW)
    {
      altDelay(300);
      display.setCursor((column * 12), 0);
      display.print(" ");
      display.display();
      column++;
      character = locator[column];
      if (column > 7) column = 0;
    }
    if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW))
    {
      for (int i = 0; i < 8; i++)
      {
        if (locator[i] == temp | locator[i] == 0x00) data_changeFlag = true;
        if (data_changeFlag == true) locator[i] = 0x00;
      }
      if (data_changeFlag == true)
      {
        display.setCursor(0, 0);
        display.print(locator);
        display.setCursor(0, 16);
        display.print(F("  SAVED  "));
        display.display();
        for (int i = 0; i < 10; i++)
        {
          EEPROM.put(100 + (i * 2), locator[i]);
          if (locator[i] == temp | locator[i] == 0x00) i = 10;
        }
        EEPROM.put(122, 1);     // Now set grid square change flag
        altDelay(4000);
      }
      else
      {
        display.setCursor(0, 0);
        display.print(F("NOT SAVED "));
        display.setCursor(0, 16);
        display.print(F("END WITH #"));
        display.display();
        altDelay(4000);
      }
      column = 0;
      display.setCursor(0, 0);
      display.print(F("          "));
      display.setCursor(0, 16);
      display.print(locator);
      display.print(F("          "));
      display.display();
      character = 0;
      data_changeFlag = false;
    }
  }
}




//******************************************************************
// Calculate the 6 digit Maidenhead Grid Square location
//******************************************************************
void tinyGPS_calcGridSquare()
{
  double longitude = gps.location.lng();
  double latitude  = gps.location.lat();
  double temp;
  char GPSgrid[8];
  int index;

  longitude = longitude + 180;
  latitude = latitude + 90;
  
  temp = longitude / 20;
  index = (int) temp;
  longitude = longitude - (index * 20);
  GPSgrid[0] = index + 65;
  
  temp = latitude / 10;
  index = (int) temp;
  GPSgrid[1] = index + 65;
  latitude = latitude - (index * 10);

  temp = longitude / 2;
  index = (int) temp;
  longitude = longitude - (index * 2);
  GPSgrid[2] = index + 48;
  
  temp = latitude;
  index = (int) temp;
  GPSgrid[3] = index + 48;
  latitude = latitude - index;
    
  temp = longitude / 0.083333;
  index = (int) temp;
  GPSgrid[4] = index + 97;
  
  temp = latitude / 0.0416665;
  index = (int) temp;
  GPSgrid[5] = index + 97;

  GPSgrid[6] = '\0';
  bool doOneTimeTrigger_2 = true;
  if (strcmp(locator, GPSgrid) != 0)
  {
    while (doOneTimeTrigger_2 == true)
    {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("GPS grid: "));
    display.setCursor(0, 16);
    display.print(GPSgrid);
    display.setCursor(0, 32);
    display.print("menu: exit");
    display.setCursor(0, 48);
    display.print("2:3 - save");
    display.display();

    if (digitalRead(menu) == LOW) doOneTimeTrigger_2 = false;
    altDelay(200);
    if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW))
    {
      for (int i = 0; i < 8; i++)
      {
        locator[i] = GPSgrid[i];
        if (locator[i] == temp | locator[i] == 0x00) data_changeFlag = true;
        if (data_changeFlag == true) locator[i] = 0x00;
      }
      if (data_changeFlag == true)
      {
        display.setCursor(0, 0);
        display.print(locator);
        display.setCursor(0, 16);
        display.print(F("  SAVED  "));
        display.display();
        for (int i = 0; i < 10; i++)
        {
          EEPROM.put(100 + (i * 2), locator[i]);
          if (locator[i] == temp | locator[i] == 0x00) i = 10;
        }
        EEPROM.put(122, 1);     // Now set grid square change flag
        altDelay(4000);
        doOneTimeTrigger_2 == false;
      }
    }
   }    
 }
 for (int i = 0; i < 8; i++) locator[i] = GPSgrid[i];
}




//******************************************************************
// Clock adjust function follows:
// Used to adjust the system time
// Note: Quickly depressing pushbutton 1 will synchronize the clock
//       to the top of the minute "0"
//       Holding down pushbutton 4 while depressing pushbutton 1
//       will advance the hours.
//       Holding down pushbutton 3 while depressing pushbutton 2
//       will advance the minutes.
// Called called by "switch case4" in loop
//******************************************************************
void adjClock()
{
  getTime();
  display.clearDisplay();
  // Display current time
  int tempHour = hours;
  int tempMinute = minutes;
  int tempSecond = seconds;
  display.setCursor(0, 0);
  display.print(F("  "));
  int a = hours;
  int b = a % 10;
  a = a / 10;
  display.print(a);
  display.print(b);
  display.print(F(":"));
  a = minutes;
  b = a % 10;
  a = a / 10;
  display.print(a);
  display.print(b);
  display.print(F(":"));
  a = seconds;
  b = a % 10;
  a = a / 10;
  display.print(a);
  display.print(b);

  // Display legend
  display.setCursor(0, 16);
  display.print(F("Hold PB4"));
  display.setCursor(0, 32);
  display.print(F("PB1: Hours"));
  display.setCursor(0, 48);
  display.print(F("PB2: Minutes"));
  display.display();

  // Start one button time synchronization routine
  if (digitalRead(pushButton1) == LOW)
  {
    if (tempSecond > 30) tempMinute++;
    if (tempMinute > 59) tempMinute = 0;
    tempSecond = 0;
    adjClockTime(tempSecond, tempMinute, tempHour);
    altDelay(500);
  }
  timeAdjust_toggle = false;

  // Start set time routine
  while (digitalRead(pushButton4) == LOW)
  {
    if (digitalRead(pushButton1) == LOW)
    {
      altDelay(500);
      tempHour++;
      if (tempHour > 23) tempHour = 0;
      timeAdjust_toggle = true;
    }

    if (digitalRead(pushButton2) == LOW)
    {
      altDelay(500);
      tempMinute++;
      if (tempMinute > 59) tempMinute = 0;
      timeAdjust_toggle = true;
    }

    // Display set time
    display.setCursor(0, 0);
    display.print(F("  "));
    int a = tempHour;
    int b = a % 10;
    a = a / 10;
    display.print(a);
    display.print(b);
    display.print(F(":"));
    a = tempMinute;
    b = a % 10;
    a = a / 10;
    display.print(a);
    display.print(b);
    display.print(F(":00"));
    display.display();
  }

  // Update time if change is made
  if (timeAdjust_toggle == true)
  {
    int tempSecond = 0;
    adjClockTime(tempSecond, tempMinute, tempHour);
    timeAdjust_toggle = false;
  }
}


//******************************************************************
// Date adjust function follows:
// Used to adjust the system date
// Note:
//       Holding down pushbutton 4 while depressing pushbutton 1
//       will advance the date.
//       Holding down pushbutton 4 while depressing pushbutton 2
//       will advance the month.
//       Holding down pushbutton 4 while depressing pushbutton 3
//       will advance the year.
// Called called by "switch case5" in loop
//******************************************************************
void setDATE()
{
  getDate();

  int updateDate = xdate;
  int updateMonth = xmonth;
  int updateYear = xyear;

  // Display currently stored date
  display.setCursor(0, 0);
  display.print(updateDate);
  display.print(F(" "));
  descriptionType oneItem;
  memcpy_P (&oneItem, &monthOfTheYear [updateMonth - 1], sizeof oneItem);
  display.print (oneItem.description);
  display.print(F(" "));
  display.print(updateYear);

  // Display legend
  display.setCursor(0, 16);
  display.print(F("Hold PB4"));
  display.setCursor(0, 32);
  display.print(F("1:Date"));
  display.setCursor(0, 48);
  display.print(F("2:Month 3:Yr"));
  display.display();

  // Start update
  while (digitalRead(pushButton4) == LOW)
  {
    if (digitalRead(pushButton1) == LOW)
    {
      altDelay(500);
      updateDate++;
      if (updateDate > 31) updateDate = 0;
      timeAdjust_toggle = true;
    }

    if (digitalRead(pushButton2) == LOW)
    {
      altDelay(500);
      updateMonth++;
      if (updateMonth > 12) updateMonth = 1;
      timeAdjust_toggle = true;
    }

    if (digitalRead(pushButton3) == LOW)
    {
      altDelay(500);
      updateYear++;
      if (updateYear > 30) updateYear = 19;
      timeAdjust_toggle = true;
    }

    // Display updates
    display.setCursor(0, 0);
    //if (xdate < 10) display.print(F("0"));
    display.print(updateDate);
    display.print(F(" "));
    descriptionType oneItem;
    memcpy_P (&oneItem, &monthOfTheYear [updateMonth - 1], sizeof oneItem);
    display.print (oneItem.description);
    display.print(F(" "));
    display.print(updateYear);
    display.display();
  }

  // Save data if updated
  if (timeAdjust_toggle == true)
  {
    // Convert DEC to BCD
    updateDate = ((updateDate / 10) * 16) + (updateDate % 10);
    updateMonth = ((updateMonth  / 10) * 16) + (updateMonth % 10);
    updateYear = ((updateYear / 10) * 16) + (updateYear % 10);

    // Write the data
    Wire.beginTransmission(DS3231_addr);
    Wire.write((byte)0x04); // start at register 4
    Wire.write(updateDate);
    Wire.write(updateMonth);
    Wire.write(updateYear);
    Wire.endTransmission();
  }
}



//******************************************************************
//  Calibrate alogrithm follows:
//  Used to create a frequency calibration factor
//
//  Called by loop()
//******************************************************************
void XtalFreqCalibrate()

{
  char buf[5];
  XtalFreq = 25000000 + CalFactor;
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000101); // Enable CLK1 - disable CLK0 & CLK2
  si5351aSetFreq(SYNTH_MS_1, 25000000, 0);
  display.clearDisplay();
  while (digitalRead(menu) == HIGH)
  {
    if (digitalRead(pushButton3) == LOW)
    {
      altDelay(200);
      resolution = resolution * 10L;
      if (resolution > 1000) resolution = 1L;
    }

    if (digitalRead(pushButton4) == LOW)
    {
      altDelay(200);
      XtalFreq = XtalFreq - resolution;
      CalFactor = CalFactor - resolution;
      si5351aSetFreq(SYNTH_MS_1, 25000000, 0);
    }

    if (digitalRead(pushButton1) == LOW)
    {
      altDelay(200);
      XtalFreq = XtalFreq + resolution;
      CalFactor = CalFactor + resolution;
      si5351aSetFreq(SYNTH_MS_1, 25000000, 0);
    }
    display.setCursor(0, 0);
    display.print(F("          "));
    display.setCursor(0, 0);
    display.print(F("CF: "));
    if ((CalFactor >> 15) == 1) CalFactor = CalFactor - 65536;
    display.print(CalFactor);
    display.setCursor(0, 16);
    display.print(F("Res: "));
    snprintf(buf, sizeof(buf), "%4u", resolution);
    display.print(buf);
    display.setCursor(0, 32);
    display.print(F("PB3: Res"));
    display.setCursor(0, 48);
    display.print(F("2:3 - save"));
    if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW)) saveEEPROM();
    display.display();
  }
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // Disable clocks
}


//******************************************************************
//  Save updates function follows:
//  Used to save data to pseudo EEPROM
//
//  Called by loop()
//******************************************************************
void  saveEEPROM()
{
  altDelay(200);
  display.clearDisplay();
  data_changeFlag = true;
  EEPROM.put(0, TXband);
  EEPROM.put(10, TXinterval);
  EEPROM.put(20, FreqHopTX);
  EEPROM.put(30, TXoffset);
  EEPROM.put(40, mode);
  EEPROM.put(50, CalFactor);
  EEPROM.put(60, ndbm);
  display.setCursor(0, 16);
  display.print(F("  SAVED   "));
  display.display();
  altDelay(2000);
}



//******************************************************************
//  Time update function follows:
//  Used to retrieve the correct time from the DS3231 RTC
//
//
//******************************************************************
void  getTime()
{
  if (validGPSflag == true)
  {
    if (TempSeconds != GPS_seconds)
    {
    seconds = GPS_seconds;
    TempSeconds = GPS_seconds;
    minutes = GPS_minutes;
    hours   = GPS_hours;
    }
  }

  else
  {
    // Send request to receive data starting at register 0
    Wire.beginTransmission(DS3231_addr); // DS3231_addr is DS3231 device address
    Wire.write((byte)0); // start at register 0
    Wire.endTransmission();
    Wire.requestFrom(DS3231_addr, 3); // request three bytes (seconds, minutes, hours)

    while (Wire.available())
    {
      seconds = Wire.read(); // get seconds
      minutes = Wire.read(); // get minutes
      hours = Wire.read();   // get hours

      seconds = (((seconds & 0b11110000) >> 4) * 10 + (seconds & 0b00001111)); // convert BCD to decimal
      minutes = (((minutes & 0b11110000) >> 4) * 10 + (minutes & 0b00001111)); // convert BCD to decimal
      hours = (((hours & 0b00100000) >> 5) * 20 + ((hours & 0b00010000) >> 4) * 10 + (hours & 0b00001111)); // convert BCD to decimal (assume 24 hour mode)
    }
  }
}



//******************************************************************
//  Time set function follows:
//  Used to set the DS3231 RTC time
//
//  Called by setup() & loop()
//******************************************************************
void updateRTCtime()
{

  int updateSecond;
  int updateMinute;
  int updateHour;

  if (validGPSflag == true)
  {
    updateSecond = GPS_seconds;
    updateMinute = GPS_minutes;
    updateHour   = GPS_hours;
  }
  else
  {
    //epoch = timeClient.update();
    timeClient.update();
    //rtc.setEpoch(epoch);
    updateSecond = timeClient.getSeconds();
    updateMinute = timeClient.getMinutes();
    updateHour = timeClient.getHours();
  }

   //Convert BIN to BCD
   updateSecond = updateSecond + 6 * (updateSecond / 10);
   updateMinute = updateMinute + 6 * (updateMinute / 10);
   updateHour = updateHour + 6 * (updateHour / 10);

   //Write the data
   Wire.beginTransmission(DS3231_addr);
   Wire.write((byte)0); // start at location 0
   Wire.write(updateSecond);
   Wire.write(updateMinute);
   Wire.write(updateHour);
   Wire.endTransmission();
}



//******************************************************************
//  Save date function follows:
//  Used to save the correct to the DS3231 RTC
//
//  called by loop()
//******************************************************************
void saveDate()
{
  epoch = timeClient.getEpochTime();
  int updateDate = day(epoch);
  int updateMonth = month(epoch); 
  int updateYear = year(epoch) - 2000;

  // Convert DEC to BCD
  updateDate = ((updateDate / 10) * 16) + (updateDate % 10);
  updateDate = updateDate;
  updateMonth = ((updateMonth  / 10) * 16) + (updateMonth % 10);
  updateYear = ((updateYear / 10) * 16) + (updateYear % 10);

  // Write the data
  Wire.beginTransmission(DS3231_addr);
  Wire.write((byte)0x04); // start at register 4
  Wire.write(updateDate);
  Wire.write(updateMonth);
  Wire.write(updateYear);
  Wire.endTransmission();
  
}



//******************************************************************
//  Date display function follows:
//  Used to retrieve the correct date from the DS3231 RTC
//
//  The day of the week algorithm is a modified version
//  of the open source code found at:
//  Code by JeeLabs http://news.jeelabs.org/code/
//
// called by displayData(), displayClock()
//******************************************************************
void  getDate()
{
  int nowDay;
  int nowDate;
  int tempdate;
  int nowMonth;
  int nowYear;

  // send request to receive data starting at register 3
  Wire.beginTransmission(DS3231_addr); // DS3231_addr is DS3231 device address
  Wire.write((byte)0x03); // start at register 3
  Wire.endTransmission();
  Wire.requestFrom(DS3231_addr, 4); // request four bytes (day date month year)

  while (Wire.available())
  {
    nowDay = Wire.read();    // get day (serves as a placeholder)
    nowDate = Wire.read();   // get date
    nowMonth = Wire.read();  // get month
    nowYear = Wire.read();   // get year

    xdate = (((nowDate & 0b11110000) >> 4) * 10 + (nowDate & 0b00001111)); // convert BCD to decimal
    tempdate = xdate;
    xmonth = (((nowMonth & 0b00010000) >> 4) * 10 + (nowMonth & 0b00001111)); // convert BCD to decimal
    xyear = ((nowYear & 0b11110000) >> 4) * 10 + ((nowYear & 0b00001111)); // convert BCD to decimal

    if (xyear >= 2000) xyear -= 2000;
    for (byte i = 1; i < xmonth; ++i)
      tempdate += pgm_read_byte(daysInMonth + i - 1);
    if (xmonth > 2 && xyear % 4 == 0)
      ++tempdate;
    tempdate = tempdate + 365 * xyear + (xyear + 3) / 4 - 1;
    xday = (tempdate + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
  }
}



//******************************************************************
// Date/Time Clock display function follows:
// Used to display date and time
//
// Called called by "switch case" in loop
//******************************************************************
void DisplayClock()
{
  display.setTextSize(3); // Draw 3X-scale text
  display.setCursor(0, 0);

  getTime();

  if (hours < 10) display.print(F("0"));
  display.print(hours);
  display.print(F(":"));

  if (minutes < 10) display.print(F("0"));
  display.print(minutes);

  display.setTextSize(2); // Draw 2X-scale text
  display.print(F(" "));
  if (seconds < 10) display.print(F("0"));
  display.print(seconds);
  display.display();

  getDate();
  display.setCursor(0, 28);
  descriptionType oneItem;
  //   memcpy_P (&oneItem, &daysOfTheWeek [now.dayOfTheWeek()], sizeof oneItem);
  memcpy_P (&oneItem, &daysOfTheWeek [xday], sizeof oneItem);
  display.print (oneItem.description);
  //    display.print(daysOfTheWeek[now.dayOfTheWeek()]);
  display.print(F(" "));

  if (xdate < 10) display.print(F("0"));
  display.print(xdate);
  display.print(F(" "));
  memcpy_P (&oneItem, &monthOfTheYear [xmonth - 1], sizeof oneItem);
  display.print (oneItem.description);
  display.print(F("  "));

  Wire.beginTransmission(DS3231_addr);         // Start I2C protocol with DS3231 address
  Wire.write(0x11);                            // Send register address
  Wire.endTransmission(false);                 // I2C restart
  Wire.requestFrom(DS3231_addr, 2);            // Request 11 bytes from DS3231 and release I2C bus at end of reading
  int temperature_msb = Wire.read();           // Read temperature MSB
  int temperature_lsb = Wire.read();           // Read temperature LSB

  temperature_lsb >>= 6;

  // Convert the temperature data to F and C
  // Note: temperature is DS3231 temperature and not ambient temperature
  //       correction factor of 0.88 is used
  display.setCursor(0, 48);
  int DegC = int(((temperature_msb * 100) + (temperature_lsb * 25)) * 0.88);
  display.print(DegC / 100);
  display.print(F("."));
  display.print((DegC % 100) / 10);
  display.print(F("C "));
  int DegF = round(int(DegC * 9 / 5 + 3200));
  display.print(DegF / 100);
  //display.print(F("."));
  //display.print((DegF % 100) / 10);
  display.print(F("F "));

  altDelay(300);
}


//******************************************************************
//  Time set function follows:
//  Used to set the DS3231 RTC time
//
//  Called by adjClock()
//******************************************************************
void adjClockTime(int updateSecond, int updateMinute, int updateHour)
{
  // Convert BIN to BCD
  updateSecond = updateSecond + 6 * (updateSecond / 10);
  updateMinute = updateMinute + 6 * (updateMinute / 10);
  updateHour = updateHour + 6 * (updateHour / 10);

  // Write the data
  Wire.beginTransmission(DS3231_addr);
  Wire.write((byte)0); // start at location 0
  Wire.write(updateSecond);
  Wire.write(updateMinute);
  Wire.write(updateHour);
  Wire.endTransmission();
}


//******************************************************************
//  Select band follows:
//
//  Called by loop()
//******************************************************************
void changeBand()
{
  DisplayText("1&4 - edit", 0, 0);
  DisplayText("menu: exit", 0, 2);
  DisplayText("2:3 - save", 0, 3);
  if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW)) saveEEPROM();

  if (digitalRead(pushButton4) == LOW)
  {
    altDelay(200);
    TXband++;
    if (TXdialFreq [TXband] == 0)TXband = 0;
    altDelay(200);
  }

  if (digitalRead(pushButton1) == LOW)
  {
    altDelay(200);
    TXband--;
    if (TXband < 0) TXband = 0;
    altDelay(200);
  }

  setfreq();
}


//******************************************************************
//  Edit power level follows:
//  Used to change power level
//
//  Called by loop()
//******************************************************************
void EditPower()
{
  //DisplayText("1&4 - edit", 0, 2);
  DisplayText("menu: exit", 0, 2);
  DisplayText("2:3 - save", 0, 3);
  if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW)) saveEEPROM();

  display.setCursor(0, 0);
  display.print(ndbm);
  double pWatts = pow( 10.0, (ndbm - 30.0) / 10.0);
  display.setCursor(0, 16);
  display.print(pWatts, 3);
  display.print(F(" W"));
  display.display();

  if (digitalRead(pushButton4) == LOW)
  {
    altDelay(200);
    display.setCursor(0, 0);
    display.print(F("   "));
    display.setCursor(0, 16);
    display.print(F("          "));
    display.display();
    ndbm++;
  }

  if (digitalRead(pushButton1) == LOW)
  {
    altDelay(200);
    display.setCursor(0, 0);
    display.print(F("   "));
    display.setCursor(0, 16);
    display.print(F("          "));
    display.display();
    ndbm--;
  }
}



//******************************************************************
//  Edit transmit interval follows:
//  Used to determine time between transmissions
//
//  Called by loop()
//******************************************************************
void EditTXinterval()
{
  DisplayText("1&4 - edit", 0, 1);
  DisplayText("menu: exit", 0, 2);
  DisplayText("2:3 - save", 0, 3);
  if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW)) saveEEPROM();

  display.setCursor(0, 0);
  display.print(TXinterval);

  if (digitalRead(pushButton4) == LOW)
  {
    altDelay(200);
    display.setCursor(0, 0);
    display.print(F("   "));
    display.setCursor(0, 16);
    display.print(F("          "));
    TXinterval++;
  }

  if (digitalRead(pushButton1) == LOW)
  {
    altDelay(200);
    display.setCursor(0, 0);
    display.print(F("   "));
    display.setCursor(0, 16);
    TXinterval--;
  }

}



//******************************************************************
//  Edit transmit frequency hop follows:
//  Used to engage/disengage frequency hopping within the WSPR window
//
//  Called by loop()
//******************************************************************
void EditTXhop()
{
  DisplayText("1&4 - edit", 0, 1);
  DisplayText("menu: exit", 0, 2);
  DisplayText("2:3 - save", 0, 3);
  if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW)) saveEEPROM();

  display.setCursor(0, 0);
  if (FreqHopTX == true) display.print(F("YES"));
  else display.print(F("NO "));

  if (digitalRead(pushButton4) == LOW)
  {
    altDelay(200);
    display.setCursor(0, 0);
    display.print(F("NO "));
    FreqHopTX = false;
  }

  if (digitalRead(pushButton1) == LOW)
  {
    altDelay(200);
    display.setCursor(0, 0);
    display.print(F("YES"));
    FreqHopTX = true;
  }
}



//******************************************************************
//  Edit WSPR offset follows:
//  Used to determine the WSPR offset frequency (1400 - 1600) Hz
//
//  Called by loop()
//******************************************************************
void EditTXoffset()
{
  DisplayText("1&4 - edit", 0, 1);
  DisplayText("menu: exit", 0, 2);
  DisplayText("2:3 - save", 0, 3);
  if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW)) saveEEPROM();

  display.setCursor(0, 0);
  display.print(TXoffset);

  if (digitalRead(pushButton4) == LOW)
  {
    altDelay(200);
    TXoffset++;
    if (TXoffset > 1600UL) TXoffset = 1600UL;
  }

  if (digitalRead(pushButton1) == LOW)
  {
    altDelay(200);
    TXoffset--;
    if (TXoffset < 1400UL) TXoffset = 1400UL;
  }

}



//******************************************************************
//  Antenna tune alogrithm follows:
//  Used to set up antenna tuning parameters
//
//  Called by loop()
//******************************************************************
void AntennaTune()
{
  DisplayText("ANT TUNE", 0, 0);
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000101); // Enable CLK1 - disable CLK0 & CLK2
  setfreq();

  do
  {
    if (digitalRead(menu) == LOW) MenuExitFlag = true;

    display.setCursor(0, 0);
    digitalWrite(XmitLED, HIGH);
    si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband] + TXoffset), 0);
    SWRval = analogRead(A3);  // read the input pin
    display.setCursor(0, 48);
    SWRval = SWRval / 20;
    display.setTextSize(1); // Draw 1X-scale text
    for (int i = 0; i < 20; i++)
    {
      if (i < SWRval) display.print(F("*"));
      else display.print(F(" "));
    }
    display.display();
    altDelay(100);
    display.setTextSize(2); // Draw 2X-scale text
    display.display();
  } while (MenuExitFlag == false);
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // Disable clocks
}


//******************************************************************
//  Shortcut to display text follows:
//
//  Called by various functions
//******************************************************************
void DisplayText(char message[], int col, int row)
{
  col = col * 12.8;
  row = row * 16;
  display.setCursor(col, row);
  display.print(message);
  display.display();
}


//******************************************************************
//  WiFi status follows:
//
//  Called by setup()
//******************************************************************
void printWiFiStatus() {
  display.clearDisplay();
  display.setCursor(0, 0);
  if (status != WL_CONNECTED)
  {
    display.println(F("  NO WiFi"));
    display.println(F(" "));
    display.println(F("DEFAULT TO"));
    display.println(F(" RTC TIME "));
    display.display();
    WiFiStatus = false;
  }
  else
  {
    display.println(F("CONNECTED"));
    // Device IP address
    IPAddress ip = WiFi.localIP();
    display.setTextSize(1); // Draw 1X-scale text
    display.println("IP Address: ");
    display.println(ip);
    display.print("\tSignal: ");
    display.print(WiFi.RSSI());
    display.println(" dBm");
    display.print("\tEncryption: ");
    display.print(WiFi.encryptionType());
    display.display();
    display.setTextSize(2); // Draw 2X-scale text
    WiFiStatus = true;
  }
  altDelay(5000);
  display.clearDisplay();
}


//******************************************************************
// displayData follows:
// Displays operational WSPR function operation
//
// Called by transmitPrep()
//******************************************************************
void displayData()
{
  if (millis() > time_now + 900) // delay for displayed data
  {
    if (toggle == true)
    {
      tcount3++;
      if (tcount3 > 11) tcount3 = 0;
      display.setCursor(0, 48);
      display.print(F("          "));
    }
    toggle = !toggle;
    time_now = millis();
  }

  if (toggle == false)
  {
    //    getDate();
    display.setCursor(0, 48);
    switch (tcount3)
    {
      case 0: // Display WSPR TX offset
        display.print (F("Offset"));
        if (mode != 6)
        {
          display.print(TXoffset);
        }
        else
        {
          display.print(FT8_TXoffset);
        }
        break;

      case 1: // Display grid square
        display.print(F("GRD "));
        display.print (locator);
        break;

      case 2: // Display power
        display.print(F("PWR: "));
        display.print (ndbm);
        display.print(F("dBm "));
        break;

      case 3: // Display transmit interval
        display.print(F("TXint: "));
        display.print (TXinterval);
        break;

      case 4: // Display frequency hopping YES/NO
        display.print(F("TXhop: "));
        if (FreqHopTX == false) display.print(F("NO"));
        else display.print(F("YES"));
        break;

      case 5: // Display date
        getDate();
        descriptionType oneItem;

        if (xdate < 10) display.print(F("0"));
        display.print(xdate);
        display.print(F(" "));
        memcpy_P (&oneItem, &monthOfTheYear [xmonth - 1], sizeof oneItem);
        display.print (oneItem.description);
        display.print(F(" "));
        display.print(xyear);
        break;

      case 6: // Display the time source
        display.print(F("TIME: "));
        if (WiFiStatus == true) display.print("WiFi");
        else if (validGPSflag == true) display.print("GPS ");
        else display.print("RTC ");

      case 7: // Display WiFi status
        display.print(F("WiFi:  "));
        if (WiFi.status() == WL_CONNECTED)
        {
          display.print(F("ON "));
          WiFiStatus = true;
        }
        else
        {
          display.print(F("OFF"));
          WiFiStatus = false;
        }
        break;

      case 8: // Display GPS status
        display.print(F("GPS: "));
        if (validGPSflag == true)
        {
          display.print(F("VALID"));
        }
        else
        {
          display.print(F("OFF  "));
        }
        break;

      case 9: // Display the GPS satellite number
        display.print(F("SATS = "));
        display.print(sats);
        break;

      case 10: // Display transmit mode
        displayMode();
        break;

      case 11: // Display Si5351 frequency calibration factor
        display.print(F("CF = "));
        if ((CalFactor >> 15) == 1) CalFactor = CalFactor - 65536;
        display.print(CalFactor);
        break;
    }
  }
}



//******************************************************************
// transmitPrep function follows:
// Used to select band, determine if it is time to transmit, and
// blink WSPR related LEDs.
// Called called by "switch case0" in loop
//******************************************************************
void transmitPrep()
{
  // Generate unique WSPR message for your callsign, locator, and power
  wsprGenCode(); // WSPR message calculation

  // Print callsign, frequency, and status to the display
  DisplayText(call2, 0, 0); //("text",col,row)
  XmitStatus();
  setfreq();

  // ---------------------------------------------------------------------
  // Transmit determined by 'schedule' begins here
  // ---------------------------------------------------------------------
  getTime();
  if ((mode == 6 | TransmitMode == 6) & TransmitFlag == true & (seconds == 15 | seconds == 30)) FT8transmit(); // FT8 mode

  if (seconds == 0 & TransmitFlag == true)
  {
    if (mode > 6) // Check 'schedule' first
    {
      for (int timeslot = 0; timeslot < 30; timeslot++)
      {
        if (schedule [timeslot] [0] == -1) return;
        if ((schedule [timeslot] [0]) == minutes)
        {
          //TXinterval = 1;
          TXband = schedule [timeslot] [2];
          TransmitMode = schedule [timeslot][1];
          if (TransmitMode < 0 | TransmitMode > 5) return;
          if (TransmitMode < 3) T_R_period = 0;
          else T_R_period = TransmitMode - 2;
          if (TransmitMode < 0 | TransmitMode > 5) return;
          transmit();
        }
      }
    }

    // ---------------------------------------------------------------------
    // Transmit determined by transmit interval begins here
    // ---------------------------------------------------------------------
    else
    {
      if (mode < 3) // must be either FST4W120 or WSPR 120 second mode
      {
        if (bitRead(minutes, 0) == 0)
        {
          T_R_period = 0;
          TransmitMode = mode;
          transmit();
        }
      }
      if (mode == 3) // FST4W300 300 second mode
      {
        if (minutes % 5 == 0)
        {
          T_R_period = 1;
          TransmitMode = mode;
          transmit();
        }
      }

      if (mode == 4 & (bitRead(minutes, 0) == 0)) CWtransmit(); // CW mode
      if (mode == 5 & (bitRead(minutes, 0) == 0)) FSKtransmit(); // FSK mode
    }

  }


  //Turn tranmitter inhibit on and off during non-transmit period
  if (digitalRead(pushButton2) == LOW & suspendUpdateFlag == false)
  {
    altDelay(200);
    WSPR_toggle = !WSPR_toggle;
    if (WSPR_toggle == false)
    {
      TransmitFlag = false;
      XmitStatus();
    }
    else
    {
      TransmitFlag = true;
      XmitStatus();
    }
    altDelay(1000);
  }

  getTime();

  if (seconds != lastSecond)
  {
    lastSecond = seconds;
    snprintf(buf, sizeof(buf), "          ");
    display.setCursor(0, 32);
    display.print(buf);
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hours, minutes, seconds);
    display.setCursor(0, 32);
    display.print(buf);
  }

  // Display data during non-transmit period
  if (suspendUpdateFlag == false) displayData();
}


//******************************************************************
// WSPR status function follows:
// Displays WSPR transmitter ON/OFF status
//
// Called by loop()
//******************************************************************
void XmitStatus()
{
  if (TransmitFlag == false)
  {
    DisplayText("OFF", 7, 0); //("text",col,row)
  }
  else
  {
    DisplayText("ON ", 7, 0); //("text",col,row)
  }
}



// ****************************************************************
// Mode selection function follows;
//
//  Determine WSPR FST4W or Multi of operation follows:
//  1 = WSPR
//  2 = FST4W 120
//  3 = FST4W 300
//  4 = CW
//  5 = FSK/QRSS
//  6 = FT8
//  7 = schedule 1
//  7 = schedule 2
//  7 = schedule 3
//
//  FST4W 900 and 1800 are not supported by the K4CLE board
//
//  Called by Menu_selection()
//******************************************************************
void ModeSelect()
{
  // display.clearDisplay();
  DisplayText("PB1: Down", 0, 1); //("text",col,row)
  DisplayText("PB4: Up  ", 0, 2); //("text",col,row)
  DisplayText("2:3 save  ", 0, 3); //("text",col,row)
  if ((digitalRead(pushButton2) == LOW) & (digitalRead(pushButton3) == LOW)) saveEEPROM();

  if (digitalRead(pushButton4) == LOW)
  {
    mode++;
    altDelay(200);
  }
  if (digitalRead(pushButton1) == LOW)
  {
    mode--;
    altDelay(200);
  }
  display.setCursor(0, 0);
  switch (mode)
  {
    case 1:
      display.print(F("   WSPR   "));
      break;

    case 2:
      display.print(F("FST4W 120 "));
      break;

    case 3:
      display.print(F("FST4W 300 "));
      break;

    case 4:
      display.print(F("    CW    "));
      break;

    case 5:
      display.print(F(" FSK/QRSS "));
      break;

    case 6:
      display.print(F("   FT8    "));
      break;

    case 7:
      display.print(F("SCHEDULE 1"));
      memmove(schedule, schedule_1, sizeof(schedule_1));
      break;

    case 8:
      display.print(F("SCHEDULE 2"));
      memmove(schedule, schedule_2, sizeof(schedule_2));
      break;

    case 9:
      display.print(F("SCHEDULE 3"));
      memmove(schedule, schedule_3, sizeof(schedule_3));
      break;
  }
  altDelay(100);
  if (mode < 1 | mode > 9)mode = 1;
}


//******************************************************************
//  displayMode() follows:
//  Used to display the mode of operation on the home screen
//
//  Called loop()
//******************************************************************
void displayMode()
{
  display.setCursor(0, 48);
  switch (mode)
  {
    case 1:
      display.print(F("   WSPR   "));
      break;

    case 2:
      display.print(F("FST4W 120 "));
      break;

    case 3:
      display.print(F("FST4W 300 "));
      break;

    case 4:
      display.print(F("    CW    "));
      break;

    case 5:
      display.print(F(" FSK/QRSS "));
      break;

    case 6:
      display.print(F("   FT8    "));
      break;


    case 7:
      display.print(F("SCHEDULE 1"));
      break;

    case 8:
      display.print(F("SCHEDULE 2"));
      break;

    case 9:
      display.print(F("SCHEDULE 3"));
      break;
  }
}



//******************************************************************
// Set WSPR frequency function follows:
// Calculates the frequency data to be sent to the Si5351 clock generator
// and displays the frequency on the olded display
//
// Called by loop(), setup, and transmit()
//******************************************************************
void setfreq()
{
  unsigned long frequency;
  if (suspendUpdateFlag == true & TransmitMode < 4) frequency = TXdialFreq [TXband] + TXoffset; // Temporarily store Freq_1 (WSPR & FST4W modes)
  else if (suspendUpdateFlag == true & mode == 6) frequency = TXdialFreq [TXband] + FT8_TXoffset; // Temporarily store Freq_1 (FT8 mode)
  else frequency = TXdialFreq [TXband]; // Temporarily store Freq_1 (offset not added)

  display.setCursor(0, 16);
  if (frequency >= 1000000UL)
  {
    int MHz = int(frequency / 1000000UL);
    int kHz = int ((frequency - (MHz * 1000000UL)) / 1000UL);
    int Hz = int (frequency % 1000UL);
    snprintf(buf, sizeof(buf), "%2u,%03u,%03u", MHz, kHz, Hz);
  }

  else if (frequency >= 1000UL & frequency < 1000000UL)
  {
    int kHz = int (frequency / 1000UL);
    int Hz = int (frequency % 1000UL);
    snprintf(buf, sizeof(buf), "%6u,%03u", kHz, Hz);
  }
  else if (frequency < 1000UL)
  {
    int Hz = int (frequency);
    snprintf(buf, sizeof(buf), "%10u", Hz);
  }
  display.print(buf);
  display.display();
}



//******************************************************************
// Transmit algorithm follows:
// Configures data and timing and then sends data to the Si5351
//
// Called by loop()
//******************************************************************
void transmit()
{
  TransmitFlag = false;
  int SymbolCount;
  if (mode < 6) // if not in schedule mode check transmit interval
  {
    TXcount++;
    if (TXcount > TXinterval) TXcount = 1;
    if (TXcount != TXinterval)
    {
      altDelay(1000);
      TransmitFlag = true;
      return;
    }
  }

  time_now = millis();
  suspendUpdateFlag = true;
  if (FreqHopTX == true) // Enables in-band TX frequency hopping in incremental 10Hz steps
  {
    TXoffset = TXoffset + 10UL;
    if (TXoffset > 1550UL) TXoffset = 1440UL;
  }
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000101); // Enable CLK1 - disable CLK0 & CLK2
  setfreq();
  display.setCursor(0, 32);
  display.print(F(" XMITTING "));
  display.setCursor(0, 48);
  if (TransmitMode > 1 & TransmitMode < 6) display.print(F("FST4W "));
  if (TransmitMode == 1) display.print(F(" WSPR "));
  display.print(T_R_time [T_R_period]);
  display.print(F(" "));
  display.display();
  digitalWrite(XmitLED, HIGH);
  symbolTime = (SymbolLength[T_R_period] * MsecCF) / 100000;
  unsigned long currentTime = millis();
  if (TransmitMode == 1) SymbolCount = 162; // WSPR symbol count
  else
  {
    SymbolCount = 160; // FST4 symbol count
  }
  for (int count = 0; count < SymbolCount; count++)
  {
    unsigned long timer = millis();
    if (TransmitMode == 1) si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband] + TXoffset) , OffsetFreq [0][sym[count]]); // WSPR mode
    else si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband] + TXoffset), OffsetFreq[T_R_period][FST4Wsymbols[count]]); // FST4W mode
    while ((millis() - timer) <= symbolTime) {
      __asm__("nop\n\t");
    };
  }
  suspendUpdateFlag = false;
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // disable CLKs
  digitalWrite(XmitLED, LOW);
  TransmitFlag = true;
}

//******************************************************************
// FT8 Transmit algorithm follows:
// Configures data and timing and then sends data to the Si5351
//
// Called by loop()
//******************************************************************
void FT8transmit()
{
  TransmitFlag = false;
  suspendUpdateFlag = true;
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000101); // Enable CLK1 - disable CLK0 & CLK2
  setfreq();
  display.setCursor(0, 32);
  display.print(F(" XMITTING "));
  display.setCursor(0, 48);
  display.print(F("   FT8    "));
  display.display();
  digitalWrite(XmitLED, HIGH);
  symbolTime = (FT8_SymbolLength * MsecCF) / 100000;
  unsigned long currentTime = millis();
  for (int count = 0; count < 79; count++)
  {
    unsigned long timer = millis();
    si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband] + FT8_TXoffset) , FT8_OffsetFreq[FT8symbols[count]]); // FT8 mode
    while ((millis() - timer) <= symbolTime) {
      __asm__("nop\n\t");
    };
  }
  suspendUpdateFlag = false;
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // disable CLKs
  digitalWrite(XmitLED, LOW);
  TransmitFlag = true;
}


//******************************************************************
// CW Transmit algorithm follows:
// Continually repeats CW message until 1 min 50 seconds have elapsed.
// The end of the CW message may be padded with spaces to avoid being
// trucated at the end of the transmission.
//
// If the TXinterval is set to 1 it loop forever until it detects
// the MENU pushbutton is depressed.
//
// The 3.5mm mono socket connected to pin D3 is used to control how the
// CW output is keyed. Without a plug insterted, the D3 is LOW and
// the Si5351 outputs a keyed CW signal and the PTT line remains ON.
// With a plug inserted, D3 is HIGH, the Si5351 is turned off, and
// the PTT line is used as a CW output to key an external transmitter.
//
// Note: The 3.5mm socket is gounded (LOW) without a plug inserted
//
// Called by loop()
//******************************************************************
void CWtransmit()
{
  TransmitFlag = false;
  TXcount++;
  if (TXcount > TXinterval) TXcount = 1;
  if (TXcount != TXinterval)
  {
    altDelay(1000);
    TransmitFlag = true;
    return;
  }

  suspendUpdateFlag = true;
  setfreq();
  display.setCursor(0, 32);
  display.print(F(" XMITTING "));
  display.setCursor(0, 48);
  display.print(F("    CW    "));
  display.display();

  si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband] + CWoffset), 0);
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // disable CLKs
  int i = 0, j, k, temp, ByteMask = B00000111, MorseBitCount, MsgLength = strlen(CWmessage);
  TickCounter = 0;
  while (TickCounter < 110) //Transmit CW for 110 seconds
  {
    if (TXinterval == 1) TickCounter = 0; // Loop forever when interval = 1
    if (digitalRead (menu) == LOW) TickCounter = 120;
    MorseChar = CWmessage[i];
    altDelay(50);
    j = int(MorseChar);
    if (j < 65) j = j - 48; // Converts to Morse[] table number
    else j = j - 55; // Note: space = -16
    if (j == -16) j = 36;   // If a space set j to correct table number
    if (j == -1) j = 37;    // If a "/" set j to correct table number
    temp = Morse[j];    // temp now equals binary coded character
    MorseBitCount = ByteMask & Morse[j];
    if ((digitalRead(extCWsocket) == HIGH)) // PTT line is used as a CW output to key an external transmitter
    {
      Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // disable CLKs
      for (k = 7; k >= (8 - MorseBitCount); k--)
      {
        if (bitRead(Morse[j], k) == HIGH)
        {
          digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
          digitalWrite(XmitLED, HIGH); // Tied to PTT line
          altDelay(DahDelay);
          digitalWrite(LED_BUILTIN, LOW);
          digitalWrite(XmitLED, LOW); // Tied to PTT line
        }

        else
        {
          digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
          digitalWrite(XmitLED, HIGH); // Tied to PTT line
          altDelay(DitDelay);
          digitalWrite(LED_BUILTIN, LOW);
          digitalWrite(XmitLED, LOW); // Tied to PTT line
        }
        altDelay(SymbolDelay);
      }
    }
    else // Si5351 outputs a keyed CW signal and the PTT line remains ON
    { digitalWrite(XmitLED, HIGH); // Tied to PTT line
      for (k = 7; k >= (8 - MorseBitCount); k--) {
        if (bitRead(Morse[j], k) == HIGH)
        {
          Si5351_write(CLK_ENABLE_CONTROL, 0b00000101); // Enable CLK1 - disable CLK0 & CLK2
          digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
          altDelay(DahDelay);
          Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // disable CLKs
          digitalWrite(LED_BUILTIN, LOW);
        }

        else
        {
          Si5351_write(CLK_ENABLE_CONTROL, 0b00000101); // Enable CLK1 - disable CLK0 & CLK2
          digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
          altDelay(DitDelay);
          Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // disable CLKs
          digitalWrite(LED_BUILTIN, LOW);
        }
        altDelay(SymbolDelay);
      }
    }

    altDelay (LetterDelay);
    if (j == 36) altDelay(LetterDelay); // Extra delay for a space

    i++;
    if (i == MsgLength)i = 0;
  }

  suspendUpdateFlag = false;
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // disable CLKs
  digitalWrite(XmitLED, LOW);
  TransmitFlag = true;
}



//******************************************************************
// FSK/QRSS Transmit algorithm follows:
//
// The FSKditDelay timing may be changed to attempt to fit the FSK
// message within a 10 minute window.
//
// Called by loop()
//******************************************************************
void FSKtransmit()
{
  TransmitFlag = false;
  suspendUpdateFlag = true;
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000101); // Enable CLK1 - disable CLK0 & CLK2
  si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband]), 0); // FSK mode frequency
  setfreq();
  display.setCursor(0, 32);
  display.print(F(" XMITTING "));
  display.setCursor(0, 48);
  display.print(F("    FSK   "));
  display.display();
  digitalWrite(XmitLED, HIGH);

  int i = 0, j, k, temp, ByteMask = B00000111, MorseBitCount, MsgLength = strlen(FSKmessage);
  bool MessageEnd = false;
  TickCounter = 0;

  while (MessageEnd == false)
  {
    MorseChar = FSKmessage[i];
    j = int(MorseChar);
    if (j < 65) j = j - 48; // Converts to Morse[] table number
    else j = j - 55; // Note: space = -16
    if (j == -16) j = 36;   // If a space set j to correct table number
    if (j == -1) j = 37;    // If a "/" set j to correct table number

    temp = Morse[j];    // temp now equals binary coded character
    MorseBitCount = ByteMask & Morse[j];
    for (k = 7; k >= (8 - MorseBitCount); k--) {
      if (bitRead(Morse[j], k) == HIGH)
      {
        si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband] + FSKoffset), 0);
        digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        altDelay(FSKdahDelay);
        si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband]), 0);
        digitalWrite(LED_BUILTIN, LOW);
      }

      else
      {
        digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband] + FSKoffset), 0);
        altDelay(FSKditDelay);
        si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband]), 0);
        digitalWrite(LED_BUILTIN, LOW);
      }

      altDelay(FSKsymbolDelay);
    }

    altDelay (FSKletterDelay);
    if (j == 36) altDelay(FSKletterDelay); // Extra delay for a space

    i++;
    if (i == MsgLength) MessageEnd = true; // End of message transmission
  }

  suspendUpdateFlag = false;
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000111); // disable CLKs
  digitalWrite(XmitLED, LOW);
  TransmitFlag = true;
}



//******************************************************************
//  Solar conditions reader follows:
//  Used to download and display latest slar data
//
//  Attributed to Richard H. Grote, K6PBF
//  ATWIFI Clock/Weather/Solar Display
//  QEX May/June 2022
// 
//  Modified by Gene Marcus, W3PM 
//
//  Called by loop()
//******************************************************************

void solar_conditions()
{
  if (WiFiStatus == false)
  {
    display.clearDisplay();
    display.setCursor(36, 0);
    display.print(F("WiFi"));
    display.setCursor(36, 24);
    display.print(F("not"));
    display.setCursor(0, 48);
    display.print(F("available"));
    display.display();
    while(digitalRead (menu) == HIGH){}; //Stop here until menu button is depressed
  }

  else
  {
  // This is code associated with the connection to QSL solar data XML feed

  int solarflux;
  int aindex;
  int kindex;
  char * xray;
  int sunspots;
  int protonflux;
  int electonflux;
  float swind;
  int swind_int;
  float dummy;

  bool flag_alert;
  int alert_type;
  int y;  // used to define alert type: 0-Xray, 1-EFLUX, 2-PFlux

  char disp_line0[11];    //display buffer and pointers
  char disp_line1[11];
  char disp_line2[11];
  char disp_line3[11];

  // limits for electron flux and proton flux
  int eflux_limit = 1000;
  int pflux_limit = 100;

  char target1[] = "solarflux";
  char target2[] = "aindex";
  char target3[] = "kindex";
  char target4[] = "xray";
  char target5[] = "sunspots";
  char target6[] = "proton";
  char target7[] = "electon";
  char target8[] = "solarwind";

  char alert [3][6] = {{'X', '-', 'R', 'a', 'y', '\0'} , {'E', 'F', 'l', 'u', 'x', '\0'}, {'P', 'F', 'l', 'u', 'x', '\0'}};


  flag_alert = false;  //set to quiescent state

  if (client.connect(server_qsl, 80)) {
    Serial.println(F("Connected to QSL server"));
    // Make a HTTP request
    client.println(F("GET /solarxml.php HTTP/1.1"));
    //client.println(F("GET /solar.php HTTP/1.1"));
    client.println(F("Host:www.hamqsl.com"));
    client.println(F("User-Agent: arduino-Wifi"));
    client.println(F("Connection: close"));
    client.println();


  }
  else {
   // Serial.println(F("unable to connect 3"));
  }
  altDelay(1000);  // wait for data to arrive from hamqsl.com

  findIt(target1, 0, 0, 2, 2, &dummy, &solarflux);

  findIt(target2, 0, 0, 2, 2, &dummy, &aindex);

  findIt(target3, 0, 0, 2, 2, &dummy, &kindex);

  xray = findIt(target4, 0, 0, 2, 0);

  if (( *xray != 'A') && (*xray != 'B')) {
    flag_alert = true;
    y = 0;
    //Serial.println("x-ray true");
  }

  findIt(target5, 0, 0, 2, 2, &dummy, &sunspots);

  findIt(target6, 0, 0, 2, 2, &dummy, &protonflux);
  if (protonflux > pflux_limit) {
    flag_alert = true;
    y = 2;
    //Serial.println("PFlux true");
  }

  findIt(target7, 0, 0, 2, 2, &dummy, &electonflux);
  if (electonflux > eflux_limit) {
    flag_alert = true;
    y = 1;
    //Serial.println("Eflux true");
  }

  findIt(target8, 0, 0, 2, 1, &swind);
  // Serial.print(swind);
  swind_int = (int) swind;

  if (solarflux == 0) {
    return; // if get zero readings, skip update
  }

  for (alert_type = 0; alert_type != 3; alert_type ++) {
    // Serial.println(alert[alert_type]);}

    // if no alerts, use this output sprintf statements
    // if (flag_alert == false) {

    sprintf(disp_line0, "Kindex: %-1i", kindex);
    sprintf(disp_line1, "Aindex:%-3i", aindex);
    sprintf(disp_line2, "SFI: %-3i", solarflux);
    sprintf(disp_line3, "SN: %-3i", sunspots);
    //sprintf(disp_line3, "Swind:%-4i", swind_int);
    //sprintf(disp_line2,"SN=%-3i  A=%-2i SWind= ",sunspots,aindex);
    //sprintf(disp_line3,"SFI=%-3i K=%-1i %4ikm/s",solarflux,kindex,swind_int);
  }

  // if alerts use these output statements instead
  /*
     else{

     sprintf(disp_line2,"SN=%-3i  A=%-2i *ALERT*",sunspots,aindex);
     sprintf(disp_line3,"SFI=%-3i K=%-1i  %6s ",solarflux,kindex,alert[y]);
     }
  */

  /*
  Serial.println();   // print of all solar weather information
  Serial.print ("Solar Flux =  ");
  Serial.println (solarflux);
  Serial.print ("A index =  ");
  Serial.println (aindex);
  Serial.print (" K index =  ");
  Serial.println (kindex);
  Serial.print ("Sunspot number is  ");
  Serial.println (sunspots);
  */
  
  display.setCursor(0, 0);
  display.print(disp_line0);
  display.setCursor(0, 16);
  display.print(disp_line1);
  display.setCursor(0, 32);
  display.print(disp_line2);
  display.setCursor(0, 48);
  display.print(disp_line3);
  display.display();
  altDelay(1000);
  
  while(digitalRead (menu) == HIGH){}; //Stop here until menu button is depressed
  }
}


//******************************************************************
//  Si5351 processing follows:
//  Generates the Si5351 clock generator frequency message
//
//  Called by sketch setup() and loop()
//******************************************************************
void si5351aSetFreq(int synth, unsigned long  frequency, int symbolOffset)
{
  unsigned long long  CalcTemp;
  unsigned long PLLfreq, divider, a, b, c, p1, p2, p3, PLL_P1, PLL_P2, PLL_P3;
  byte dividerBits = 0;
  int PLL, multiplier = 1;
  if (synth == SYNTH_MS_0) PLL = SYNTH_PLL_A;
  else PLL = SYNTH_PLL_B;

  frequency = frequency * 100UL + symbolOffset;
  c = 0xFFFFF;  // Denominator derived from max bits 2^20

  divider = 90000000000ULL / frequency;
  while (divider > 900UL) {           // If output divider out of range (>900) use additional Output divider
    dividerBits++;
    divider = divider / 2UL;
    multiplier = multiplier * 2;
    //multiplier = dividerBits << 4;
  }

  dividerBits = dividerBits << 4;
  if (divider % 2) divider--;
  //  PLLfreq = (divider * multiplier * frequency) / 10UL;

  unsigned long long PLLfreq2, divider2 = divider, multiplier2 = multiplier;
  PLLfreq2 = (divider2 * multiplier2 * frequency) / 100UL;
  PLLfreq = PLLfreq2;

  a = round(PLLfreq / XtalFreq);
  CalcTemp = PLLfreq2 - a * XtalFreq;
  CalcTemp *= c;
  CalcTemp /= XtalFreq ;
  b = CalcTemp;  // Calculated numerator


  p1 = 128UL * divider - 512UL;
  CalcTemp = 128UL * b / c;
  PLL_P1 = 128UL * a + CalcTemp - 512UL;
  PLL_P2 = 128UL * b - CalcTemp * c;
  PLL_P3 = c;


  // Write data to PLL registers
  Si5351_write(PLL, 0xFF);
  Si5351_write(PLL + 1, 0xFF);
  Si5351_write(PLL + 2, (PLL_P1 & 0x00030000) >> 16);
  Si5351_write(PLL + 3, (PLL_P1 & 0x0000FF00) >> 8);
  Si5351_write(PLL + 4, (PLL_P1 & 0x000000FF));
  Si5351_write(PLL + 5, 0xF0 | ((PLL_P2 & 0x000F0000) >> 16));
  Si5351_write(PLL + 6, (PLL_P2 & 0x0000FF00) >> 8);
  Si5351_write(PLL + 7, (PLL_P2 & 0x000000FF));


  // Write data to multisynth registers
  Si5351_write(synth, 0xFF);
  Si5351_write(synth + 1, 0xFF);
  Si5351_write(synth + 2, (p1 & 0x00030000) >> 16 | dividerBits);
  Si5351_write(synth + 3, (p1 & 0x0000FF00) >> 8);
  Si5351_write(synth + 4, (p1 & 0x000000FF));
  Si5351_write (synth + 5, 0);
  Si5351_write (synth + 6, 0);
  Si5351_write (synth + 7, 0);

}



//******************************************************************
// Write I2C data function for the Si5351A follows:
// Writes data over the I2C bus to the appropriate device defined by
// the address sent to it.

// Called by sketch setup, WSPR, transmit(), si5351aSetFreq, and
// si5351aStart functions.
//******************************************************************
uint8_t Si5351_write(uint8_t addr, uint8_t data)
{
  Wire.beginTransmission(Si5351A_addr);
  Wire.write(addr);
  Wire.write(data);
  Wire.endTransmission();
}


//******************************************************************
// Alternate delay function follows:
// altDelay is used because the command "delay" causes critical
// timing errors.
//
// Called by all functions
//******************************************************************
unsigned long altDelay(unsigned long delayTime)
{
  time_now = millis();
  while (millis() < time_now + delayTime) //delay 1 second
  {
    __asm__ __volatile__ ("nop");
  }
}


void writeEEPROM(int deviceAddress, unsigned int EEaddress, int data )
{
  int bdata1 = data >> 8;
  int bdata2 = data & 0xFF;

  Wire.beginTransmission(deviceAddress);
  Wire.write((int)(EEaddress >> 8));   // MSB
  Wire.write((int)(EEaddress & 0xFF)); // LSB
  Wire.write(bdata1);
  Wire.endTransmission();
  delay(50);

  Wire.beginTransmission(deviceAddress);
  Wire.write((int)((EEaddress + 1) >> 8));   // MSB
  Wire.write((int)((EEaddress + 1) & 0xFF)); // LSB
  Wire.write(bdata2);
  Wire.endTransmission();
  delay(50);
}

int readEEPROM(int deviceAddress, unsigned int EEaddress )
{
  byte rdata1, rdata2;
  Wire.beginTransmission(deviceAddress);
  Wire.write((int)(EEaddress >> 8));   // MSB
  Wire.write((int)(EEaddress & 0xFF)); // LSB
  Wire.endTransmission();
  Wire.requestFrom(deviceAddress, 1);
  if (Wire.available()) rdata1 = Wire.read();

  Wire.beginTransmission(deviceAddress);
  Wire.write((int)((EEaddress + 1) >> 8));   // MSB
  Wire.write((int)((EEaddress + 1) & 0xFF)); // LSB
  Wire.endTransmission();
  Wire.requestFrom(deviceAddress, 1);
  if (Wire.available()) rdata2 = Wire.read();

  int rdata = (rdata1 << 8) + rdata2;
  return rdata;
}




//***********************************************************************
// WSPR message generation algorithm follows:
// Configures the unique WSPR message based upon your callsign, location, and power.
// Note: Type 2 callsigns (e.g. GM/W3PM) are supported in this version
//
// Called by sketch setup.
//***********************************************************************
void wsprGenCode()
{
  for (i = 0; i < 11; i++) {
    if (call2[i] == 47)calltype = 2;
  };
  if (calltype == 2) type2();
  else
  {
    for (i = 0; i < 7; i++) {
      call1[i] = call2[i];
    };
    for (i = 0; i < 5; i++) {
      grid4[i] = locator[i];
    };
    packcall();
    packgrid();
    n2 = ng * 128 + ndbm + 64;
    pack50();
    encode_conv();
    interleave_sync();
  }
}

//******************************************************************
void type2()
{
  if (msg_type == 0)
  {
    packpfx();
    ntype = ndbm + 1 + nadd;
    n2 = 128 * ng + ntype + 64;
    pack50();
    encode_conv();
    interleave_sync();
  }
  else
  {
    hash();
    for (ii = 1; ii < 6; ii++)
    {
      call1[ii - 1] = locator[ii];
    };
    call1[5] = locator[0];
    packcall();
    ntype = -(ndbm + 1);
    n2 = 128 * ihash + ntype + 64;
    pack50();
    encode_conv();
    interleave_sync();
  };

}

//******************************************************************
void packpfx()
{
  char pfx[3];
  int Len;
  int slash;

  for (i = 0; i < 7; i++)
  {
    call1[i] = 0;
  };
  Len = strlen(call2);
  for (i = 0; i < 11; i++)
  {
    if (call2[i] == 47) slash = i;
  };
  if (call2[slash + 2] == 0)
  { //single char add-on suffix
    for (i = 0; i < slash; i++)
    {
      call1[i] = call2[i];
    };
    packcall();
    nadd = 1;
    nc = int(call2[slash + 1]);
    if (nc >= 48 && nc <= 57) n = nc - 48;
    else if (nc >= 65 && nc <= 90) n = nc - 65 + 10;
    else if (nc >= 97 && nc <= 122) n = nc - 97 + 10;
    else n = 38;
    ng = 60000 - 32768 + n;
  }
  else if (call2[slash + 3] == 0)
  {
    for (i = 0; i < slash; i++)
    {
      call1[i] = call2[i];
    };
    packcall();
    n = 10 * (int(call2[slash + 1]) - 48) +  int(call2[slash + 2]) - 48;
    nadd = 1;
    ng = 60000 + 26 + n;
  }
  else
  {
    for (i = 0; i < slash; i++)
    {
      pfx[i] = call2[i];
    };
    if (slash == 2)
    {
      pfx[2] = pfx[1];
      pfx[1] = pfx[0];
      pfx[0] = ' ';
    };
    if (slash == 1)
    {
      pfx[2] = pfx[0];
      pfx[1] = ' ';
      pfx[0] = ' ';
    };
    ii = 0;
    for (i = slash + 1; i < Len; i++)
    {
      call1[ii] = call2[i];
      ii++;
    };
    packcall();
    ng = 0;
    for (i = 0; i < 3; i++)
    {
      nc = int(pfx[i]);
      if (nc >= 48 && nc <= 57) n = nc - 48;
      else if (nc >= 65 && nc <= 90) n = nc - 65 + 10;
      else if (nc >= 97 && nc <= 122) n = nc - 97 + 10;
      else n = 36;
      ng = 37 * ng + n;
    };
    nadd = 0;
    if (ng >= 32768)
    {
      ng = ng - 32768;
      nadd = 1;
    };
  }
}

//******************************************************************
void packcall()
{
  // coding of callsign
  if (chr_normf(call1[2]) > 9)
  {
    call1[5] = call1[4];
    call1[4] = call1[3];
    call1[3] = call1[2];
    call1[2] = call1[1];
    call1[1] = call1[0];
    call1[0] = ' ';
  }

  n1 = chr_normf(call1[0]);
  n1 = n1 * 36 + chr_normf(call1[1]);
  n1 = n1 * 10 + chr_normf(call1[2]);
  n1 = n1 * 27 + chr_normf(call1[3]) - 10;
  n1 = n1 * 27 + chr_normf(call1[4]) - 10;
  n1 = n1 * 27 + chr_normf(call1[5]) - 10;
}

//******************************************************************
void packgrid()
{
  // coding of grid4
  ng = 179 - 10 * (chr_normf(grid4[0]) - 10) - chr_normf(grid4[2]);
  ng = ng * 180 + 10 * (chr_normf(grid4[1]) - 10) + chr_normf(grid4[3]);
}

//******************************************************************
void pack50()
{
  // merge coded callsign into message array c[]
  t1 = n1;
  c[0] = t1 >> 20;
  t1 = n1;
  c[1] = t1 >> 12;
  t1 = n1;
  c[2] = t1 >> 4;
  t1 = n1;
  c[3] = t1 << 4;
  t1 = n2;
  c[3] = c[3] + ( 0x0f & t1 >> 18);
  t1 = n2;
  c[4] = t1 >> 10;
  t1 = n2;
  c[5] = t1 >> 2;
  t1 = n2;
  c[6] = t1 << 6;
}

//******************************************************************
//void hash(string,len,ihash)
void hash()
{
  int Len;
  uint32_t jhash;
  int *pLen = &Len;
  Len = strlen(call2);
  byte IC[12];
  byte *pIC = IC;
  for (i = 0; i < 12; i++)
  {
    pIC + 1;
    &IC[i];
  }
  uint32_t Val = 146;
  uint32_t *pVal = &Val;
  for (i = 0; i < Len; i++)
  {
    IC[i] = int(call2[i]);
  };
  jhash = nhash_(pIC, pLen, pVal);
  ihash = jhash & MASK15;
  return;
}
//******************************************************************
// normalize characters 0..9 A..Z Space in order 0..36
char chr_normf(char bc )
{
  char cc = 36;
  if (bc >= '0' && bc <= '9') cc = bc - '0';
  if (bc >= 'A' && bc <= 'Z') cc = bc - 'A' + 10;
  if (bc >= 'a' && bc <= 'z') cc = bc - 'a' + 10;
  if (bc == ' ' ) cc = 36;

  return (cc);
}


//******************************************************************
// convolutional encoding of message array c[] into a 162 bit stream
void encode_conv()
{
  int bc = 0;
  int cnt = 0;
  int cc;
  unsigned long sh1 = 0;

  cc = c[0];

  for (int i = 0; i < 81; i++) {
    if (i % 8 == 0 ) {
      cc = c[bc];
      bc++;
    }
    if (cc & 0x80) sh1 = sh1 | 1;

    symt[cnt++] = parity(sh1 & 0xF2D05351);
    symt[cnt++] = parity(sh1 & 0xE4613C47);

    cc = cc << 1;
    sh1 = sh1 << 1;
  }
}

//******************************************************************
byte parity(unsigned long li)
{
  byte po = 0;
  while (li != 0)
  {
    po++;
    li &= (li - 1);
  }
  return (po & 1);
}

//******************************************************************
// interleave reorder the 162 data bits and and merge table with the sync vector
void interleave_sync()
{
  int ii, ij, b2, bis, ip;
  ip = 0;

  for (ii = 0; ii <= 255; ii++) {
    bis = 1;
    ij = 0;
    for (b2 = 0; b2 < 8 ; b2++) {
      if (ii & bis) ij = ij | (0x80 >> b2);
      bis = bis << 1;
    }
    if (ij < 162 ) {
      sym[ij] = SyncVec[ij] + 2 * symt[ip];
      ip++;
    }
  }
}


//_____________________________________________________________________________
// Note: The parts of the routine that follows is used for WSPR type 2 callsigns
//(i.e. GM/W3PM) to generate the required hash code.
//_____________________________________________________________________________

/*
  -------------------------------------------------------------------------------
  lookup3.c, by Bob Jenkins, May 2006, Public Domain.

  These are functions for producing 32-bit hashes for hash table lookup.
  hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
  are externally useful functions.  Routines to test the hash are included
  if SELF_TEST is defined.  You can use this free for any purpose.  It's in
  the public domain.  It has no warranty.

  You probably want to use hashlittle().  hashlittle() and hashbig()
  hash byte arrays.  hashlittle() is is faster than hashbig() on
  little-endian machines.  Intel and AMD are little-endian machines.
  On second thought, you probably want hashlittle2(), which is identical to
  hashlittle() except it returns two 32-bit hashes for the price of one.
  You could implement hashbig2() if you wanted but I haven't bothered here.

  If you want to find a hash of, say, exactly 7 integers, do
  a = i1;  b = i2;  c = i3;
  mix(a,b,c);
  a += i4; b += i5; c += i6;
  mix(a,b,c);
  a += i7;
  final(a,b,c);
  then use c as the hash value.  If you have a variable length array of
  4-byte integers to hash, use hashword().  If you have a byte array (like
  a character string), use hashlittle().  If you have several byte arrays, or
  a mix of things, see the comments above hashlittle().

  Why is this so big?  I read 12 bytes at a time into 3 4-byte integers,
  then mix those integers.  This is fast (you can do a lot more thorough
  mixing with 12*3 instructions on 3 integers than you can with 3 instructions
  on 1 byte), but shoehorning those bytes into integers efficiently is messy.
  -------------------------------------------------------------------------------
*/

//#define SELF_TEST 1

//#include <stdio.h>      /* defines printf for tests */
//#include <time.h>       /* defines time_t for timings in the test */
//#ifdef Win32
//#include "win_stdint.h"  /* defines uint32_t etc */
//#else
//#include <stdint.h> /* defines uint32_t etc */
//#endif

//#include <sys/param.h>  /* attempt to define endianness */
//#ifdef linux
//# include <endian.h>    /* attempt to define endianness */
//#endif

#define HASH_LITTLE_ENDIAN 1

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*
  -------------------------------------------------------------------------------
  mix -- mix 3 32-bit values reversibly.

  This is reversible, so any information in (a,b,c) before mix() is
  still in (a,b,c) after mix().

  If four pairs of (a,b,c) inputs are run through mix(), or through
  mix() in reverse, there are at least 32 bits of the output that
  are sometimes the same for one pair and different for another pair.
  This was tested for:
  pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
  "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
  the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

  Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
  satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
  Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
  for "differ" defined as + with a one-bit base and a two-bit delta.  I
  used http://burtleburtle.net/bob/hash/avalanche.html to choose
  the operations, constants, and arrangements of the variables.

  This does not achieve avalanche.  There are input bits of (a,b,c)
  that fail to affect some output bits of (a,b,c), especially of a.  The
  most thoroughly mixed value is c, but it doesn't really even achieve
  avalanche in c.

  This allows some parallelism.  Read-after-writes are good at doubling
  the number of bits affected, so the goal of mixing pulls in the opposite
  direction as the goal of parallelism.  I did what I could.  Rotates
  seem to cost as much as shifts on every machine I could lay my hands
  on, and rotates are much kinder to the top and bottom bits, so I used
  rotates.
  -------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
  { \
    a -= c;  a ^= rot(c, 4);  c += b; \
    b -= a;  b ^= rot(a, 6);  a += c; \
    c -= b;  c ^= rot(b, 8);  b += a; \
    a -= c;  a ^= rot(c,16);  c += b; \
    b -= a;  b ^= rot(a,19);  a += c; \
    c -= b;  c ^= rot(b, 4);  b += a; \
  }

/*
  -------------------------------------------------------------------------------
  final -- final mixing of 3 32-bit values (a,b,c) into c

  Pairs of (a,b,c) values differing in only a few bits will usually
  produce values of c that look totally different.  This was tested for
  pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
  "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
  the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

  These constants passed:
  14 11 25 16 4 14 24
  12 14 25 16 4 14 24
  and these came close:
  4  8 15 26 3 22 24
  10  8 15 26 3 22 24
  11  8 15 26 3 22 24
  -------------------------------------------------------------------------------
*/
#define final(a,b,c) \
  { \
    c ^= b; c -= rot(b,14); \
    a ^= c; a -= rot(c,11); \
    b ^= a; b -= rot(a,25); \
    c ^= b; c -= rot(b,16); \
    a ^= c; a -= rot(c,4);  \
    b ^= a; b -= rot(a,14); \
    c ^= b; c -= rot(b,24); \
  }

/*
  -------------------------------------------------------------------------------
  hashlittle() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
  Returns a 32-bit value.  Every bit of the key affects every bit of
  the return value.  Two keys differing by one or two bits will have
  totally different hash values.

  The best hash table sizes are powers of 2.  There is no need to do
  mod a prime (mod is sooo slow!).  If you need less than 32 bits,
  use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
  In which case, the hash table should have hashsize(10) elements.

  If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

  By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
  code any way you wish, private, educational, or commercial.  It's free.

  Use for hash table lookup, or anything where one collision in 2^^32 is
  acceptable.  Do NOT use for cryptographic purposes.
  -------------------------------------------------------------------------------
*/

//uint32_t hashlittle( const void *key, size_t length, uint32_t initval)
#ifdef STDCALL
uint32_t __stdcall NHASH( const void *key, size_t *length0, uint32_t *initval0)
#else
uint32_t nhash_( const void *key, int *length0, uint32_t *initval0)
#endif
{
  uint32_t a, b, c;                                        /* internal state */
  size_t length;
  uint32_t initval;
  union {
    const void *ptr;
    size_t i;
  } u;     /* needed for Mac Powerbook G4 */

  length = *length0;
  initval = *initval0;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32_t *k = (const uint32_t *)key;         /* read 32-bit chunks */
    const uint8_t  *k8;

    k8 = 0;                                   //Silence compiler warning
    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
       "k[2]&0xffffff" actually reads beyond the end of the string, but
       then masks off the part it's not allowed to read.  Because the
       string is aligned, the masked-off tail is in the same word as the
       rest of the string.  Every machine with memory protection I've seen
       does it on word boundaries, so is OK with this.  But VALGRIND will
       still catch it and complain.  The masking trick does make the hash
       noticably faster for short strings (like English words).
    */
#ifndef VALGRIND

    switch (length)
    {
      case 12: c += k[2]; b += k[1]; a += k[0]; break;
      case 11: c += k[2] & 0xffffff; b += k[1]; a += k[0]; break;
      case 10: c += k[2] & 0xffff; b += k[1]; a += k[0]; break;
      case 9 : c += k[2] & 0xff; b += k[1]; a += k[0]; break;
      case 8 : b += k[1]; a += k[0]; break;
      case 7 : b += k[1] & 0xffffff; a += k[0]; break;
      case 6 : b += k[1] & 0xffff; a += k[0]; break;
      case 5 : b += k[1] & 0xff; a += k[0]; break;
      case 4 : a += k[0]; break;
      case 3 : a += k[0] & 0xffffff; break;
      case 2 : a += k[0] & 0xffff; break;
      case 1 : a += k[0] & 0xff; break;
      case 0 : return c;              /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    k8 = (const uint8_t *)k;
    switch (length)
    {
      case 12: c += k[2]; b += k[1]; a += k[0]; break;
      case 11: c += ((uint32_t)k8[10]) << 16; /* fall through */
      case 10: c += ((uint32_t)k8[9]) << 8; /* fall through */
      case 9 : c += k8[8];                 /* fall through */
      case 8 : b += k[1]; a += k[0]; break;
      case 7 : b += ((uint32_t)k8[6]) << 16; /* fall through */
      case 6 : b += ((uint32_t)k8[5]) << 8; /* fall through */
      case 5 : b += k8[4];                 /* fall through */
      case 4 : a += k[0]; break;
      case 3 : a += ((uint32_t)k8[2]) << 16; /* fall through */
      case 2 : a += ((uint32_t)k8[1]) << 8; /* fall through */
      case 1 : a += k8[0]; break;
      case 0 : return c;
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const uint16_t *k = (const uint16_t *)key;         /* read 16-bit chunks */
    const uint8_t  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((uint32_t)k[1]) << 16);
      b += k[2] + (((uint32_t)k[3]) << 16);
      c += k[4] + (((uint32_t)k[5]) << 16);
      mix(a, b, c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const uint8_t *)k;
    switch (length)
    {
      case 12: c += k[4] + (((uint32_t)k[5]) << 16);
        b += k[2] + (((uint32_t)k[3]) << 16);
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 11: c += ((uint32_t)k8[10]) << 16; /* fall through */
      case 10: c += k[4];
        b += k[2] + (((uint32_t)k[3]) << 16);
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 9 : c += k8[8];                    /* fall through */
      case 8 : b += k[2] + (((uint32_t)k[3]) << 16);
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 7 : b += ((uint32_t)k8[6]) << 16;  /* fall through */
      case 6 : b += k[2];
        a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 5 : b += k8[4];                    /* fall through */
      case 4 : a += k[0] + (((uint32_t)k[1]) << 16);
        break;
      case 3 : a += ((uint32_t)k8[2]) << 16;  /* fall through */
      case 2 : a += k[0];
        break;
      case 1 : a += k8[0];
        break;
      case 0 : return c;                     /* zero length requires no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const uint8_t *k = (const uint8_t *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((uint32_t)k[1]) << 8;
      a += ((uint32_t)k[2]) << 16;
      a += ((uint32_t)k[3]) << 24;
      b += k[4];
      b += ((uint32_t)k[5]) << 8;
      b += ((uint32_t)k[6]) << 16;
      b += ((uint32_t)k[7]) << 24;
      c += k[8];
      c += ((uint32_t)k[9]) << 8;
      c += ((uint32_t)k[10]) << 16;
      c += ((uint32_t)k[11]) << 24;
      mix(a, b, c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch (length)                  /* all the case statements fall through */
    {
      case 12: c += ((uint32_t)k[11]) << 24;
      case 11: c += ((uint32_t)k[10]) << 16;
      case 10: c += ((uint32_t)k[9]) << 8;
      case 9 : c += k[8];
      case 8 : b += ((uint32_t)k[7]) << 24;
      case 7 : b += ((uint32_t)k[6]) << 16;
      case 6 : b += ((uint32_t)k[5]) << 8;
      case 5 : b += k[4];
      case 4 : a += ((uint32_t)k[3]) << 24;
      case 3 : a += ((uint32_t)k[2]) << 16;
      case 2 : a += ((uint32_t)k[1]) << 8;
      case 1 : a += k[0];
        break;
      case 0 : return c;
    }
  }

  final(a, b, c);
  return c;
}
