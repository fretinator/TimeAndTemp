/*
 *  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
 *
 *  You need to get streamId and privateKey at data.sparkfun.com and paste them
 *  below. Or just customize this script to talk to other HTTP servers.
 *
 */

#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "time.h"

// LCD constants
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -6 * 60 * 60;
const int   daylightOffset_sec = 3600;
int  remote_offset = 14 * 60 * 60;

// For LCD
char ESC = 0xFE; // Used to issue command
const char CLS = 0x51;
const char CURSOR = 0x45;
const char LINE1_POS = 0x00;
const char LINE2_POS = 0x40;
const char LINE3_POS = 0x14;
const char LINE4_POS = 0x54;

//  THIS IS WHAT THE DATE STRING LOOKS LIKE
// "MON 12:13:31 PM"   -
char timeBuffer[21];
String lastLocalTimeStr;
String lastRemoteTimeStr;

// Button
const int buttonPin = 19;     // the number of the pushbutton pin
int buttonState = 0;         // variable for reading the pushbutton status
int lastState = 0;

// WiFi constants
const char* ssid     = "yourssid";
const char* password = "yourpwd";

// API constants
const char* api_endpoint = "https://api.openweathermap.org/data/2.5/weather?id=4233813&units=imperial&appid=yourapidkey";

#define NULL_STR ""
const String NOT_FOUND = "n/a"; // When searching for a field in the response

// Loop variables
const int WEATHER_RUN_MILLIS_NORMAL = (30 * 60 * 1000); // Check every 30 minutes
const int WEATHER_RUN_MILLIS_ERROR = (30 * 1000); // Wait 30 seconds before retrying
const int TIME_MILLIS = 200; // Update time every 200 ms
const int NTP_UPDATE_MILLIS = 24 * 60 * 60 * 1000; // once a day
bool first_time = true;
int lastMillis = 0;
int weatherMillisToWait = WEATHER_RUN_MILLIS_NORMAL; 
enum RUN_MODE {
  WEATHER_MODE,
  TIME_MODE,
  MAINTENANCE_MODE,
};

const int BUFFER_SIZE = 20;
char numBuffer[BUFFER_SIZE + 1];// One for '\0\

RUN_MODE myRunMode = WEATHER_MODE;

void dumpTimeInfo(String msg, struct tm* timeinfo) {
  Serial.println(msg);
  Serial.print("tm_sec:");
  Serial.println(timeinfo->tm_sec);
  Serial.print("tm_min:");
  Serial.println(timeinfo->tm_min);
  Serial.print("tm_hour:");
  Serial.println(timeinfo->tm_hour);
  Serial.print("tm_mday:");
  Serial.println(timeinfo->tm_mday);
  Serial.print("tm_mon:");
  Serial.println(timeinfo->tm_mon);
  Serial.print("tm_year:");
  Serial.println(timeinfo->tm_year);
  Serial.print("tm_isdst:");
  Serial.println(timeinfo->tm_isdst);
}

int getTimeUpdatePos(String newDate, String oldDate) { 
  if(newDate.length() != oldDate.length()) {
    return 0; // Update everything
  }
  for(int x = 0; x < newDate.length();x++) {
    if(newDate.charAt(x) != oldDate.charAt(x)) {
      return x;
    }
  }

  return -1; // No change
}

String getLocalTimeStr()
{
  struct tm timeinfo;
  
  if(!getLocalTime(&timeinfo)){
    return "";
  }

  //dumpTimeInfo("Local Time: ", &timeinfo);

  strftime(timeBuffer, sizeof(timeBuffer), "%a %I:%M:%S %p", &timeinfo);
  return  "  " + String(timeBuffer);
}


String getRemoteTimeStr()
{
  struct tm timeinfo;
  int dstAdjust = 0;
  
  if(!getLocalTime(&timeinfo)){
    return "";
  }

  if(timeinfo.tm_isdst) {
    dstAdjust = -1;
  }

  // Now add 14 hours
  time_t remoteTime = mktime(&timeinfo) + remote_offset + dstAdjust;

  struct tm* remoteInfo = localtime(&remoteTime);

  //dumpTimeInfo("Local Time: ", remoteInfo);
  
  strftime(timeBuffer, sizeof(timeBuffer), "%a %I:%M:%S %p", remoteInfo);
  return "  " + String(timeBuffer);
}

void updateLocalTimeFromNet() {
  printScreen("Synching with NNTP",NULL_STR,NULL_STR,NULL_STR);
  delay(100);

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  firstTimeDisplay();
}

void firstTimeDisplay() {
  String localTime = getLocalTimeStr();
  String remoteTime = getRemoteTimeStr();

  lastLocalTimeStr = localTime;
  lastRemoteTimeStr = remoteTime;
  
  printScreen("Bellville: ", localTime, "Hagonoy:", remoteTime);
}

// Only update what has changed to lessen flicker
void updateTimeDisplay() {
  String localTimeStr, remoteTimeStr;
  
  // Get latest time updates
  localTimeStr = getLocalTimeStr();
  remoteTimeStr = getRemoteTimeStr();

  if(localTimeStr == "" || remoteTimeStr == "") {
    Serial.println("Error retrieving time");
    return;
  }

  int localUpdatePos = getTimeUpdatePos(localTimeStr, lastLocalTimeStr);
  int remoteUpdatePos = getTimeUpdatePos(remoteTimeStr, lastRemoteTimeStr);
   
  if(localUpdatePos != -1) {
   
    updateScreen(LINE2_POS + localUpdatePos, localTimeStr.substring(localUpdatePos).c_str());
  }

  if(remoteUpdatePos != -1) {
    updateScreen(LINE4_POS + remoteUpdatePos, remoteTimeStr.substring(remoteUpdatePos).c_str());
  }

  lastLocalTimeStr = localTimeStr;
  lastRemoteTimeStr = remoteTimeStr; 
}

// Make numbers easier to read
String convertNum(float num) {
  snprintf(numBuffer, BUFFER_SIZE, "%.1f", num);
  numBuffer[BUFFER_SIZE] = '\0'; // Prevent overflow, buffer is BUFFER_SIZE + 1

  return String(numBuffer);
}

void updateScreen(const char updatePos, const char* textToUpdate) {
    Serial2.write(ESC);
    Serial2.write(CURSOR);
    Serial2.write(updatePos);
    Serial2.print(textToUpdate);
}

void setupWiFi() {
  // We start by connecting to a WiFi network
  printScreen("Connecting to ", ssid, NULL_STR, NULL_STR);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial2.write(ESC);
      Serial2.write(CURSOR);
      Serial2.write(LINE3_POS);
      Serial2.print("'");
  }

  printScreen("WiFi connected", "IP address: ", WiFi.localIP().toString(), NULL_STR);
  delay(1000);
}

void printScreen(String line1, String line2,
  String line3, String line4) {
  // Clear screen
  Serial2.write(ESC);
  Serial2.write(CLS);
  
  // Print line1 if present
  if(line1 != NULL_STR) {
    Serial2.print(line1);
  }

  // Print line2 if present
  if(line2 != NULL_STR) {
    Serial2.write(ESC);
    Serial2.write(CURSOR);
    Serial2.write(LINE2_POS);
    Serial2.print(line2);
  }

  // Print line3 if present
  if(line3 != NULL_STR) {
    Serial2.write(ESC);
    Serial2.write(CURSOR);
    Serial2.write(LINE3_POS);
    Serial2.print(line3);
  }
  
  // Print line4 if present
  if(line4 != NULL_STR) {
    Serial2.write(ESC);
    Serial2.write(CURSOR);
    Serial2.write(LINE4_POS);
    Serial2.print(line4);
  }
}

void setupScreen() {
  // Initialize serial
  Serial2.begin(9600);
  
  // Initialize LCD module
  Serial2.write(ESC);
  Serial2.write(0x41);
  Serial2.write(ESC);
  Serial2.write(0x51);
  
  // Set Contrast
  Serial2.write(ESC);
  Serial2.write(0x52);
  Serial2.write(40);
  
  // Set Backlight
  Serial2.write(ESC);
  Serial2.write(0x53);
  Serial2.write(8);
  
  Serial2.print("NKC Electronics");
  
  // Set cursor line 2, column 0
  Serial2.write(ESC);
  Serial2.write(CURSOR);
  Serial2.write(LINE2_POS);
  
  Serial2.print("20x4 Serial LCD");
  
  delay(1000);
}

void setup()
{
  Serial.begin(115200);
  pinMode(buttonPin, INPUT);
  lastState = digitalRead(buttonPin);
  timeBuffer[20]='\0';
  setupScreen();
  setupWiFi();
}

void runTime(bool firstTime) {
  if(firstTime) {
    updateLocalTimeFromNet();
  } else {
    updateTimeDisplay();
  }
}

int runWeather() {
  bool hasError = false;

  int err = 0;
  //WiFiClient wclient;
  HTTPClient http;
  
  // Send request
  //http.useHTTP10(true);
  http.begin(api_endpoint);
 
  err = http.GET();

  if(err !=200) {
    printScreen("Error calling API", String(err), NULL_STR, NULL_STR);
    hasError = true;
  } else {

    String response = http.getString();
  
    if(response.length() < 20) {
      printScreen("BAD RESPONSE", response.c_str(), NULL_STR, NULL_STR);
      hasError = true;
    } else {
      //Setup JSON Object
      // From JSON assistant:  620+398 = 1018  I'll add some fudge to this
      const size_t capacity = 2048;
      DynamicJsonDocument doc(capacity);

  

      // Extract values
      DeserializationError error = deserializeJson(doc, response);
      
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        hasError = true;
          } else {
          float temp = doc["main"]["temp"].as<float>();
          float humidity = doc["main"]["humidity"].as<float>();
          float wind = doc["wind"]["speed"].as<float>();
          String desc = doc["weather"][0]["description"].as<String>();      
          
          desc.trim();
    
          
        printScreen(
          "Temp: " + convertNum(temp),
          "Humidity: " + convertNum(humidity), 
         "Wind: " + convertNum(wind),  
          desc);
      }
    }
  }
  
  // Disconnect
  http.end();

  if(hasError) {
    return WEATHER_RUN_MILLIS_ERROR;
  } else {
    return WEATHER_RUN_MILLIS_NORMAL;
  }
}

void onButtonClick() {

  RUN_MODE lastRunMode = myRunMode;
  myRunMode = MAINTENANCE_MODE;
  RUN_MODE newRunMode = (WEATHER_MODE == lastRunMode) ? TIME_MODE : WEATHER_MODE;
  if(TIME_MODE == newRunMode) {
    runTime(true);
  } else {
    runWeather();
  }
  
  myRunMode = newRunMode; 
}

void loop()
{
  int curMillis = millis();

    // read the state of the pushbutton value:
  buttonState = digitalRead(buttonPin);

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState == HIGH) {
    if(lastState == LOW) {
      onButtonClick();
      Serial.print("Changing run mode:");
    }
  } 

  lastState = buttonState;
  delay(100); // for bounce of button
 
  if(myRunMode == WEATHER_MODE) {
     if(0 == lastMillis || // First Time
          curMillis < lastMillis || // Millis rolled over to 0
          (curMillis - lastMillis) > weatherMillisToWait) {
        weatherMillisToWait = runWeather();
        lastMillis = curMillis;
     }
  } else {
    if(myRunMode == TIME_MODE) {
      if(0 == lastMillis ||
          curMillis < lastMillis || // Millis rolled over to 0
          (curMillis - lastMillis) > TIME_MILLIS) {
        runTime(false);                   
        lastMillis = curMillis;
      }
    }
  } // else we are in maintenance mode due to button click 
}
