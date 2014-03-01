/*  RemoteUI for Arduino UNO (R) v1.0 by MrModd
 *  Copyright (C) 2013  Federico "MrModd" Cosentino (http://mrmodd.it/)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
//#include <SD.h> //microSD Library
#include <SPI.h> //Serial Programming Interface Library
#include <Wire.h> //I2C Library
#include <OneWire.h> //Dallas One-Wire protocol Library
#include <EEPROM.h> //Integrated EEPROM
#include <DS1307.h> //RTC Library

#define LED          13 //On board LED

#define SD_CS         4 //Chip select SD Card
#define TFT_CS       10 //Chip select TFT Display
#define TFT_DC        8 //Data/command TFT
#define TFT_RST      -1 //Reset for TXT connected to Arduino reset circuit

#define TEMP_SENS     9 //Temperature sensor from Tiny RTC
#define SQUARE_WAVE   2 //Square wave from Tiny RTC (Port 2 is also interrupt enabled)

#define MAX_LENGTH   20 //Max length of the entries received by serial console

//Do not modify next definitions
#define BUTTON_DOWN   0
#define BUTTON_LEFT   1
#define BUTTON_UP     2
#define BUTTON_RIGHT  3
#define BUTTON_SELECT 4
#define BUTTON_NONE   5

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
OneWire temp(TEMP_SENS); //Object for One-Wire temperature sensor
byte temp_addr[8]; //Address of the temperature sensor (if any)
char **entries; //Entries for the menu
uint8_t entries_size; //Number of entries on the list
uint8_t time; //Used for update every 10 iterations of the main loop
uint8_t index; //Index of the selected entry
unsigned int scrsaver;

void splashScreen(void);
void screenSaver(void);
boolean getTimeDate(char time[9], char date[11], int *weekday);
void getEntries(void);
void updateSelectedEntry(int8_t move);
void clearBox(void);
void drawStruct(void);
void fillStruct(void);
void printTime(void);
void doAction(void);
void tinyRTC(void);
void serialSetTime(void);
void changeOrientation(uint8_t m);
//void serialPrintTime(void);
uint8_t readButton(void);

void setup(void)
{
  Serial.begin(9600);

  //Init variables
  entries_size = 0;
  index = 0;
  time = 10;
  scrsaver = 600;

  //Init ports
  pinMode(SQUARE_WAVE, INPUT_PULLUP);

  //Init screen
  tft.initR(INITR_REDTAB);
  byte a = EEPROM.read(0);
  tft.setRotation(a);
  tft.setTextWrap(false);

  //Init RTC
  DS1307.begin();
  DS1307.sqw1();

  //Init Temp sensor
  temp.search(temp_addr);

  //Initialization completed
  Serial.println("Ready!");

  //Show splash screen
  splashScreen();
  //Get entries from computer
  getEntries();
  //delay(2000);

  //Draw main view
  drawStruct();
  //Fill the box
  fillStruct();
}

void loop(void)
{
  if (time >= 9) {
    printTime();
    //serialPrintTime();
    time = 0;
  }
  else {
    time++;
  }
  if (scrsaver > 0) {
    scrsaver--;
  }
  else {
    screenSaver();
    drawStruct();
    fillStruct();
    delay(100);
    scrsaver = 600;
  }
  uint8_t b = readButton();
  if (b == (BUTTON_DOWN + tft.getRotation()) % 4) {
    updateSelectedEntry(1);
    delay(100);
  }
  else if (b == (BUTTON_UP + tft.getRotation()) % 4) {
    updateSelectedEntry(-1);
    delay(100);
  }
  /*else if (b == (BUTTON_LEFT + tft.getRotation()) % 4) {
    //Do Nothing...
  }
  else if (b == (BUTTON_RIGHT + tft.getRotation()) % 4) {
    //Do Nothing...
  }*/
  else if (b == BUTTON_SELECT) {
    doAction();
  }
  delay(100);
}

/*
 * ======================================
 * ============= FUNCTIONS ==============
 * ======================================
 */

/* Show the spash screen */
void splashScreen(void)
{
  //Print base structure
  drawStruct();
  tft.setTextColor(ST7735_CYAN);
  tft.setTextSize(3);
  tft.setCursor(tft.width()/2 - 52, tft.height()/2 - 10);
  tft.print("MrModd");
  tft.setTextSize(1);
}

void screenSaver(void)
{
  tft.fillScreen(ST7735_BLACK);
  boolean invert = false;
  uint8_t sleep = 100;
  
  while (1) {
    if (sleep == 0) {
      sleep = 100;
      invert = !invert;
      tft.invertDisplay(invert);
      delay(10);
    }
    else {
      sleep--;
      delay(10);
    }
    if (readButton() != BUTTON_NONE) {
      tft.invertDisplay(false);
      return;
    }
  }
}
    

boolean getTimeDate(char time[9], char date[11], int *weekday)
{
  int RTCValues[7];
  DS1307.getDate(RTCValues);
  if (RTCValues[2] <= 31 && RTCValues[2] >= 1) { //If the day of the month is valid (RTC is connected)
    snprintf(date, 11, "%02d/%02d/20%02d", RTCValues[2], RTCValues[1], RTCValues[0]);
    snprintf(time, 9, "%02d:%02d:%02d", RTCValues[4], RTCValues[5], RTCValues[6]);
    *weekday = RTCValues[3];
    return true;
  }
  return false;
}

/* Obtain entries from serial console */
void getEntries(void)
{
  char buffer[MAX_LENGTH];
  uint8_t i = 0;
  uint8_t timeout = 200;
  char c;

  entries = (char**)malloc(sizeof(char*)*3);
  if (entries == NULL) {
    entries_size = 0;
    return; /* error */
  }
  entries_size = 3;
  entries[0] = const_cast<char *>("Tiny RTC info");
  entries[1] = const_cast<char *>("Set time (serial)");
  entries[2] = const_cast<char *>("Change orientation");
  
  while (timeout > 0) {
    if (Serial.available()) {
      timeout = 200;
      c = Serial.read();
      if (c != '\n') {
        buffer[i++] = c;
      }
      else {
        buffer[i] = '\0';
        entries = (char **)realloc(entries, sizeof(char*)*(++entries_size));
        if (entries == NULL) {
          entries_size = 0;
          return; //error
        }
        entries[entries_size - 1] = (char *)malloc(sizeof(char)*(strlen(buffer) + 1));
        if (entries[entries_size - 1] == NULL) {
          entries_size--;
          return; //error
        }
        strcpy(entries[entries_size - 1], buffer);
        //Empty buffer
        while (i > 0) buffer[i--] = 0;
      }
      if (i == MAX_LENGTH) {
        //Cannot store this entry: too long
        //Empty buffer
        while(i > 0) buffer[i--] = 0;
        //Discard all other bytes of the line
        while(Serial.available() && Serial.read() !='\n');
      }
    }
    else {
      timeout--;
      delay(10);
    }
  }
}

/* Move can be:
 *   0 if you want to highlight the current entry (draw the box around it)
 *   1 if you want to go onward by 1 entry
 *  -1 if you want to go backward by 1 entry
 */
void updateSelectedEntry(int8_t move)
{
  if (entries_size == 0) {
    return;
  }
  
  if (move == 1) {
    tft.drawRect(3, 12 + 11*(index % (((tft.height()-4 - 23) / 11)+1)), tft.width()-6, 11, ST7735_BLACK);
    index++;
    if (index >= entries_size) {
      index = 0;
      clearBox();
      fillStruct();
    }
    else if ((index % (((tft.height()-4 - 23) / 11)+1)) == 0) {
      clearBox();
      fillStruct();
    }
    else {
      tft.drawRect(3, 12 + 11*(index % (((tft.height()-4 - 23) / 11)+1)), tft.width()-6, 11, ST7735_RED);
    }
  }
  if (move == -1) {
    tft.drawRect(3, 12 + 11*(index % (((tft.height()-4 - 23) / 11)+1)), tft.width()-6, 11, ST7735_BLACK);
    if (index == 0) {
      index = entries_size - 1;
      clearBox();
      fillStruct();
    }
    else if ((index % (((tft.height()-4 - 23) / 11)+1)) == 0) {
      index--;
      clearBox();
      fillStruct();
    }
    else {
      index--;
      tft.drawRect(3, 12 + 11*(index % (((tft.height()-4 - 23) / 11)+1)), tft.width()-6, 11, ST7735_RED);
    }
  }
  if (move == 0) {
    tft.drawRect(3, 12 + 11*(index % (((tft.height()-4 - 23) / 11)+1)), tft.width()-6, 11, ST7735_RED);
  }
}

/* Clear the part of the screen where the entries take place */
void clearBox(void)
{
  tft.fillRect(3, 12, tft.width()-6, tft.height()-15, ST7735_BLACK);
}

/* Draw the main structure of the screen */
void drawStruct(void)
{
  //Clear screen
  tft.fillScreen(ST7735_BLACK);

  //Draw rectangle for time and date
  tft.fillRect(0, 0, tft.width(), 9, ST7735_BLUE);

  //Draw the box
  tft.drawRect(0, 9, tft.width(), tft.height()-9, ST7735_YELLOW);
  tft.drawRect(2, 11, tft.width()-4, tft.height()-13, ST7735_YELLOW);
}

/* Write all the visible entries on the screen */
void fillStruct(void)
{
  //Each entry is 9px height (considering also 1px margin on the top and 1px on the bottom)
  //The rectangle highlights the currently selected entry and is 11px height
  uint8_t i;
  uint8_t start = (index / (((tft.height()-4 - 23) / 11)+1))*(((tft.height()-4 - 23) / 11)+1);

  //Print the contents
  tft.setTextColor(ST7735_WHITE);
  for(i = 0; start < entries_size && i <= ((tft.height()-4 - 23) / 11); i++) {
    tft.setCursor(5, 14 + 11*i);
    tft.print(entries[start++]);
  }
  updateSelectedEntry(0);
}

/* Print time and date on the screen */
void printTime(void)
{
  //Clear line
  tft.fillRect(0, 0, tft.width(), 9, ST7735_BLUE);

  //Get time and date
  char date[11], time[9];
  int weekday;
  if (getTimeDate(time, date, &weekday)) { //If the day of the month is valid (RTC is connected)
    tft.setTextColor(ST7735_GREEN);

    //Print date
    tft.setCursor(1, 1);
    tft.print(date);

    //Print time
    tft.setCursor(tft.width()-48, 1);
    tft.print(time);
  }
}

/* Execute the action of the entry */
void doAction(void)
{
  switch (index) {
  case 0:
    tinyRTC();
    clearBox();
    fillStruct();
    break;
  case 1:
    clearBox();
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(5, 14);
    tft.print("Now set time");
    tft.setCursor(5, 24);
    tft.print("on the serial");
    tft.setCursor(5, 34);
    tft.print("console...");
    tft.setCursor(5, 94);
    tft.print("Press SELECT");
    tft.setCursor(5, 104);
    tft.print("to quit.");
    serialSetTime();
    clearBox();
    fillStruct();
    break;
  case 2:
    changeOrientation(tft.getRotation() + 1);
    break;
  default:
    Serial.println(entries[index]);
    clearBox();
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(5, 14);
    tft.print("Command sent.");
    delay(2000);
    clearBox();
    fillStruct();
    break;
  }
}

void tinyRTC(void)
{
  clearBox();
  tft.fillRect(0, 0, tft.width(), 9, ST7735_BLUE);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 14);
  tft.print("Tiny RTC info:");
  tft.setCursor(5, 34);
  tft.print("Time: ");
  tft.setCursor(5, 44);
  tft.print("Date: ");
  tft.setCursor(5, 54);
  tft.print("Day : ");
  tft.setCursor(5, 64);
  tft.print("Temp: ");
  tft.setCursor(5, 94);
  tft.print("Press SELECT");
  tft.setCursor(5, 104);
  tft.print("to quit.");

  boolean selected = false;
  uint8_t refresh = 90;
  temp.reset();
  temp.select(temp_addr);
  temp.write(0x44,1);
  delay(100);

  while (!selected) {
    if (refresh == 0) {

      //Get time and date
      char date[11], time[9];
      int weekday;

      //Get temp
      byte data[9];
      uint8_t i;
      temp.reset();
      temp.select(temp_addr);
      temp.write(0xBE);
      for (i = 0; i < 9; i++) { // we need 9 bytes
        data[i] = temp.read();
      }
      temp.reset();
      temp.select(temp_addr);
      temp.write(0x44,1);
      int raw = (data[1] << 8) | data[0];
      byte cfg = (data[4] & 0x60);
      if (cfg == 0x00) raw = raw << 3;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw << 2; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw << 1; // 11 bit res, 375 ms
      // default is 12 bit resolution, 750 ms conversion time

      if (!getTimeDate(time, date, &weekday)) { //If the day of the month is invalid (RTC is not connected)
        tft.fillRect(39, 34, tft.width()-42, 39, ST7735_BLACK);
        tft.setCursor(39, 34);
        tft.print("N.A.");
        tft.setCursor(39, 44);
        tft.print("N.A.");
        tft.setCursor(39, 54);
        tft.print("N.A.");
        tft.setCursor(39, 64);
        tft.print("N.A.");
      }
      else {
        tft.fillRect(39, 34, tft.width()-42, 39, ST7735_BLACK);
        tft.setCursor(39, 34);
        tft.print(time);
        tft.setCursor(39, 44);
        tft.print(date);
        tft.setCursor(39, 54);
        tft.print(fromNumberToWeekDay(weekday));
        tft.setCursor(39, 64);
        tft.print((float)raw / 16.0);
        tft.print(" Celsius");
      }
      refresh = 90;
      delay(10);
    }
    else {
      refresh--;
      delay(10);
    }
    if (readButton() == BUTTON_SELECT) {
      selected = true;
    }
  }
}

/* Read time and date from the serial console and update the RTC */
void serialSetTime(void)
{
  boolean abort = false;
  uint8_t i = 0;
  char c;

  //Clear line
  tft.fillRect(0, 0, tft.width(), 9, ST7735_BLUE);

  char dateTime[22];
  Serial.println("Please enter date and time in the format \"YYYY-MM-DD HH:MM:SS D\",");
  Serial.println("Where D is the number of the day of week (0 = Sunday, 6 = Saturday).");
  Serial.println("Example: 2011-04-23 02:25:27 6");
  Serial.println("If you want to quit, just send \\n or some wrong bytes.");

  while (!abort && i < 21) {
    if (Serial.available()) {
      c = Serial.read();
      dateTime[i] = c;
      i++;
      if (c == '\n') {
        abort = true;
      }
    }
    if (readButton() == BUTTON_SELECT) {
      Serial.println("Input interrupted by user from the device.");
      return;
    }
  }
  dateTime[i] = '\0';

  uint8_t year = 10 * (dateTime[2] - 48) + (dateTime[3] - 48);
  uint8_t month = 10 * (dateTime[5] - 48) + (dateTime[6] - 48);
  uint8_t dayOfMonth = 10 * (dateTime[8] - 48) + (dateTime[9] - 48);
  uint8_t dayOfWeek = (dateTime[20] - 48);
  uint8_t hour = 10 * (dateTime[11] - 48) + (dateTime[12] - 48);
  uint8_t minute = 10 * (dateTime[14] - 48) + (dateTime[15] - 48);
  uint8_t second = 10 * (dateTime[17] - 48) + (dateTime[18] - 48);

  if (year >= 0 && year < 100 && month > 0 && month < 13 && dayOfMonth > 0 && dayOfMonth < 32 && dayOfWeek >= 0 && dayOfWeek < 7 &&
    hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60) {
    DS1307.setDate(year, month, dayOfMonth, dayOfWeek, hour, minute, second);
    Serial.println("Done!");
  }
  else {
    Serial.println("Invalid data: aborted!");
  }

  //Empty serial buffer
  while(Serial.available()) {
    Serial.read();
  }
}

/* Change orientation and write the new one on the EEPROM */
void changeOrientation(uint8_t m)
{
  tft.setRotation(m);
  EEPROM.write(0, tft.getRotation());
  tft.fillScreen(ST7735_BLACK);
  drawStruct();
  fillStruct();
}

/* Print date and time on serial console */
/*void serialPrintTime(void)
{
  //Get time and date
  char dateTime[20];
  int RTCValues[7];
  DS1307.getDate(RTCValues);
  if (RTCValues[2] <= 31 && RTCValues[2] >= 1) { //If the day of the month is valid (RTC is connected)
    sprintf(dateTime, "%02d/%02d/20%02d %02d:%02d:%02d", RTCValues[2], RTCValues[1], RTCValues[0], RTCValues[4], RTCValues[5], RTCValues[6]);

    //Print time and date
    Serial.print(dateTime);
    Serial.print(" - day of week: ");
    Serial.println(fromNumberToWeekDay(RTCValues[3]));
  }
}*/

/* Read analog input and determine the type of input on the analog joystick */
uint8_t readButton(void)
{
  float a = analogRead(3);

  a *= 5.0;
  a /= 1024.0;
  if (a < 0.2) return BUTTON_DOWN;
  if (a < 1.0) return BUTTON_RIGHT;
  if (a < 1.5) return BUTTON_SELECT;
  if (a < 2.0) return BUTTON_UP;
  if (a < 3.2) return BUTTON_LEFT;
  else return BUTTON_NONE;
}
