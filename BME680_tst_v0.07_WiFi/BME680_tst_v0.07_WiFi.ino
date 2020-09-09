#include <EEPROM.h>
#include "bsec.h"

#include "NotoSansBold36.h"
#include "NotoSansBold15.h"

#define AA_FONT_LARGE NotoSansBold36
#define AA_FONT_SMALL NotoSansBold15

/* https://icons8.com/icons/set/wifi */
#include "wi-fi-96.h"
#include "wi-fi-connesso-96.h"
#include "wi-fi-off-96.h"

/*
 * Versione con uso della GET
*/
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C
boolean SensorPresent;

//
// https://lastminuteengineers.com/esp32-ota-updates-arduino-ide/
//
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

/* Configure the BSEC library with information about the sensor
    18v/33v = Voltage at Vdd. 1.8V or 3.3V
    3s/300s = BSEC operating mode, BSEC_SAMPLE_RATE_LP or BSEC_SAMPLE_RATE_ULP
    4d/28d = Operating age of the sensor in days
    generic_18v_3s_4d
    generic_18v_3s_28d
    generic_18v_300s_4d
    generic_18v_300s_28d
    generic_33v_3s_4d
    generic_33v_3s_28d
    generic_33v_300s_4d
    generic_33v_300s_28d
*/
const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};

#define STATE_SAVE_PERIOD	UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day

#define I2C_SDA 21
#define I2C_SCL 22

// Helper functions declarations
void checkIaqSensorStatus(void);
void errLeds(void);
void loadState(void);
void updateState(void);

// Create an object of the class Bsec
Bsec iaqSensor;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;

String output;

const char* ssid = "SSID";
const char* password = "PASSWORD";
WebServer server(80);

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23

#define TFT_BL              4   // Display backlight control pin
#define ADC_EN              14  //ADC_EN is the ADC detection enable port
#define ADC_PIN             34
#define BUTTON_1            35
#define BUTTON_2            0

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

uint32_t rotation=3;

/* Flag per abilitare il debug seriale. */
#define DEBUG_FLAG          1

#if DEBUG_FLAG
#define DBGPRINTF(...)  {Serial.printf(__VA_ARGS__);}
#define DBGPRINTFLN(...)  {Serial.printf(__VA_ARGS__); Serial.println("");}
#else
#define DBGPRINTF(...)  {}
#define DBGPRINTFLN(...)  {}
#endif

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 10 seconds (10000)
//unsigned long timerDelay = 10000;
// Set timer to 30 seconds (30000)
unsigned long timerDelay = 30000;

void DisplayEnvironment( void);
void drawGraph( void);
void drawGraphCO2( void);

#define HTML_PAGE_BUFFER    (2048)

String markerAccuracy;
String markerCO2;

void handleRoot2() 
{
  char temp[HTML_PAGE_BUFFER];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  if ( SensorPresent)
  {
    snprintf(temp, HTML_PAGE_BUFFER,
    
      "<!DOCTYPE html>\
      <html>\
      <head>\
      <meta http-equiv='refresh' content='5'/>\
      <style>\
      #airquality {\
        font-family: 'Trebuchet MS', Arial, Helvetica, sans-serif;\
        border-collapse: collapse;\
        width: 100%;\
      }\
      #airquality td, #airquality th {\
        border: 1px solid #ddd;\
        padding: 8px;\
      }\
      #airquality tr:nth-child(even){background-color: #f2f2f2;}\
      #airquality tr:hover {background-color: #ddd;}\
      #airquality th {\
        padding-top: 12px;\
        padding-bottom: 12px;\
        text-align: left;\
        background-color: #cccccc;\
        color: #000088;\
      }\
      </style>\
      </head>\
      <body>\
      <table id='airquality'>\
        <tr>\
          <th>Grandezza</th>\
          <th>Sensore BME680</th>\
          <th>Sensore BME280</th>\
        </tr>\
        <tr>\
          <td>Temperatura</td>\
          <td>%2.1f &deg;C</td>\
          <td>%2.1f &deg;C</td>\
        </tr>\
        <tr>\
          <td>Umidit&agrave;</td>\
          <td>%2.0f%%</td>\
          <td>%2.0f%%</td>\
        </tr>\
        <tr>\
          <td>Pressione</td>\
          <td>%4.0f hP</td>\
          <td>%4.0f hP</td>\
        </tr>\
      </table>\
      <hr>\
      <table id='airquality'>\
      <tr>\
        <th>Accuracy</th>\
        <th>Value</th>\
      </tr>\
      <tr>\
        <td>IAQ [0-3]</td>\
        <td>%d </td>\
      </tr>\
      <tr>\
        <td>CO2</td>\
        <td>%d </td>\
      <tr>\
      </table>\
      <hr>\
      <img src=\"/test.svg\"/>\
      <hr>\                  
      <table id='airquality'>\
      <tr>\
        <th>Uptime:</th>\
      </tr>\
      <tr>\
        <td>%02d:%02d:%02d</td>\
      </tr>\
      </table>\
      </body>\
      </html>",  iaqSensor.temperature, bme.readTemperature(),\
                  iaqSensor.humidity, bme.readHumidity(),\
                  (iaqSensor.pressure/100), (bme.readPressure()/100),\
                  iaqSensor.iaqAccuracy, iaqSensor.co2Accuracy, hr, min % 60, sec % 60 );
  } else {
    snprintf(temp, HTML_PAGE_BUFFER,
    
      "<!DOCTYPE html>\
      <html>\
      <head>\
      <meta http-equiv='refresh' content='5'/>\
      <style>\
      #airquality {\
        font-family: 'Trebuchet MS', Arial, Helvetica, sans-serif;\
        border-collapse: collapse;\
        width: 100%;\
      }\
      #airquality td, #airquality th {\
        border: 1px solid #ddd;\
        padding: 8px;\
      }\
      #airquality tr:nth-child(even){background-color: #f2f2f2;}\
      #airquality tr:hover {background-color: #ddd;}\
      #airquality th {\
        padding-top: 12px;\
        padding-bottom: 12px;\
        text-align: left;\
        background-color: #cccccc;\
        color: #000088;\
      }\
      </style>\
      </head>\
      <body>\
      <table id='airquality'>\
        <tr>\
          <th>Grandezza</th>\
          <th>Sensore BME680</th>\
          <th>Sensore BME280</th>\
        </tr>\
        <tr>\
          <td>Temperatura</td>\
          <td>%2.1f &deg;C</td>\
          <td><b>N/A</b></td>\
        </tr>\
        <tr>\
          <td>Umidit&agrave;</td>\
          <td>%2.0f%%</td>\
          <td><b>N/A</b></td>\
        </tr>\
        <tr>\
          <td>Pressione</td>\
          <td>%4.0f hP</td>\
          <td><b>N/A</b></td>\
        </tr>\
      </table>\
      <hr>\
      <table id='airquality'>\
      <tr>\
        <th>Accuracy</th>\
        <th>Value</th>\
      </tr>\
      <tr>\
        <td>IAQ [0-3]</td>\
        <td>%d </td>\
      </tr>\
      <tr>\
        <td>CO2</td>\
        <td>%d </td>\
      <tr>\
      </table>\
      <hr>\
      <img src=\"/test.svg\"/>\
      <hr>\                        
      <table id='airquality'>\
      <tr>\
        <th>Uptime:</th>\
      </tr>\
      <tr>\
        <td>%02d:%02d:%02d</td>\
      </tr>\
      </table>\
      </body>\
      </html>",  iaqSensor.temperature,\
              iaqSensor.humidity,\
              (iaqSensor.pressure/100),\
              iaqSensor.iaqAccuracy, iaqSensor.co2Accuracy, hr, min % 60, sec % 60 );
  }    
  /* */
  server.send(200, "text/html", temp);
}
  

void handleRoot() 
{
  char temp[HTML_PAGE_BUFFER];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  if ( SensorPresent)
  {
    snprintf(temp, HTML_PAGE_BUFFER,
  
             "<html>\
    <head>\
      <meta http-equiv='refresh' content='5'/>\
      <title>Indoor Air Quality</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
      </style>\
    </head>\
    <body>\
      <h1>Indoor Air Quality</h1>\
      <table>\
      <tr><td>Sensore <b>BME680</b></td><td>Sensore <b>BME280</b></td></tr>\
      <tr><td>T: %2.1f C</td><td>T: %2.1f C</td></tr>\
      <tr><td>H: %2.0f%%</td><td>H: %2.0f%%</td></tr>\
      <tr><td>P: %4.0f hP</td><td>P: %4.0f hP</td></tr>\
      </table>\
      <p>IAQ Accuracy: %d [0-3]</p>\
      <p>CO2 Accuracy: %d </p>\
      <hr>\
      <p>Uptime: %02d:%02d:%02d</p>\
    </body>\
  </html>", iaqSensor.temperature, bme.readTemperature(),\
            iaqSensor.humidity, bme.readHumidity(),\
            (iaqSensor.pressure/100), (bme.readPressure()/100),\
            iaqSensor.iaqAccuracy, iaqSensor.co2Accuracy, hr, min % 60, sec % 60 );
  } else {
    snprintf(temp, HTML_PAGE_BUFFER,
  
             "<html>\
    <head>\
      <meta http-equiv='refresh' content='5'/>\
      <title>Indoor Air Quality</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
      </style>\
    </head>\
    <body>\
      <h1>Indoor Air Quality</h1>\
      <table>\
      <tr><td>Sensore BME680</td><td>Sensore BME280</td></tr>\
      <tr><td>T: %2.1f C</td><td>T: N/A</td></tr>\
      <tr><td>H: %2.0f%%</td><td>H: N/A</td></tr>\
      <tr><td>P: %4.0f hP</td><td>P: N/A</td></tr>\
      </table>\
      <p>IAQ Accuracy: %d [0-3]</p>\
      <p>CO2 Accuracy: %d </p>\
      <hr>\
      <img src=\"/test.svg\" />\
      <hr>\
      <p>Uptime: %02d:%02d:%02d</p>\
    </body>\
  </html>", iaqSensor.temperature,\
            iaqSensor.humidity,\
            (iaqSensor.pressure/100),\
            iaqSensor.iaqAccuracy, iaqSensor.co2Accuracy, hr, min % 60, sec % 60 );                
  }
  /* */
  server.send(200, "text/html", temp);
}

void handleNotFound() 
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

// Entry point for the example
void setup(void)
{
  /* ************************************************* */
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1); // 1st address for the length
  /* ************************************************* */
 
  /* ************************************************* */
  Serial.begin(115200);
  delay(1000);
  /* ************************************************* */
  
  /* ************************************************* */
  Wire.begin(I2C_SDA, I2C_SCL);
  /* ************************************************* */
 
  /* ************************************************* */
  tft.init();
  tft.setRotation(rotation);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  
  tft.loadFont(AA_FONT_SMALL); // Load another different font
  
  if (TFT_BL > 0) 
  {                                           // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
      pinMode(TFT_BL, OUTPUT);                // Set backlight pin to output mode
      digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
  }

  tft.setRotation(rotation);
  tft.fillScreen(TFT_BLACK);
  
  delay(2000);
  /* ************************************************* */

  /* ************************************************* */  
  tft.setSwapBytes(true);
  tft.pushImage((tft.width()/2)-(WiFiWidth/2), (tft.height()/2)-(WiFiHeight/2), WiFiWidth, WiFiHeight, WiFiNoConn);

  WiFi.begin(ssid, password);
  DBGPRINTFLN("Connecting");
  while(WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    DBGPRINTF(".");
  }
  tft.setSwapBytes(true);
  tft.pushImage((tft.width()/2)-(WiFiConnessoWidth/2), (tft.height()/2)-(WiFiConnessoHeight/2), WiFiConnessoWidth, WiFiConnessoHeight, WiFiConnesso);
  DBGPRINTFLN("");
  DBGPRINTFLN("Connected to WiFi network with IP Address: %s", WiFi.localIP().toString().c_str());

  WiFi.setTxPower(WIFI_POWER_MINUS_1dBm);
  
  DBGPRINTFLN("TxPower: %d dBm",WiFi.getTxPower());
  
  server.on("/", handleRoot2);
  server.on("/test.svg", drawGraph);
  server.on("/test2.svg", drawGraphCO2);
  
  server.onNotFound(handleNotFound);
  
  server.begin();

  DBGPRINTFLN("HTTP server started");

  delay(2000);

  /* ************************************************* */

  /* ************************************************* */  
  tft.loadFont(AA_FONT_SMALL); // Load another different font
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN,TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  char buff[64];
  sprintf(buff,"IP: %s | AirQuality1", WiFi.localIP().toString().c_str());
  tft.drawString(buff, 0, tft.height()-15);  
  tft.drawFastHLine(2, tft.height()-17, tft.width()-4, TFT_GREEN);

  /* ************************************************* */

  /* ************************************************* */  
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("AirQuality1");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  //
  // Ho usato md5sum sotto il folder: C:\Octave\Octave-4.0.0\bin
  // Per generare l'hash ho eseguito il programma ed inserito la password per poi terminare
  // con due volte <Ctrl^D>
  // md5sum <enter>
  // <password><Ctrl^D><Ctrl^D>
  // A questo punto il programma ha generato l'hash direttamente dopo la password inserita.
  // ATTENZIONE a copiare solo l'hash senza prendere i caratteri della password inserita.
  //
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      DBGPRINTFLN("Start updating %s", type.c_str());
    })
    .onEnd([]() {
      DBGPRINTFLN("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      DBGPRINTFLN("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      DBGPRINTFLN("Error[%u]: ", error);
      String typeErr;
      if (error == OTA_AUTH_ERROR) typeErr="Auth Failed";
      else if (error == OTA_BEGIN_ERROR) typeErr="Begin Failed";
      else if (error == OTA_CONNECT_ERROR) typeErr="Connect Failed";
      else if (error == OTA_RECEIVE_ERROR) typeErr="Receive Failed";
      else if (error == OTA_END_ERROR) typeErr="End Failed";
      DBGPRINTFLN(typeErr.c_str());
    });

  ArduinoOTA.begin();
  
  /* ************************************************* */

  /* ************************************************* */    
  iaqSensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  DBGPRINTFLN("%s",output.c_str());
  checkIaqSensorStatus();

  iaqSensor.setConfig(bsec_config_iaq);
  checkIaqSensorStatus();

  loadState();

  bsec_virtual_sensor_t sensorList[9] = 
  {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_STATIC_IAQ,
  };

  iaqSensor.updateSubscription(sensorList, 9, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();

  // Print the header
  output = "Timestamp [ms], raw temperature [°C], pressure [hPa], raw relative humidity [%], gas [Ohm], IAQ, IAQ accuracy, temperature [°C], relative humidity [%], CO2 [ppm], static IAQ, static IAQ accuracy, CO2 accuracy";
  DBGPRINTFLN("%s",output.c_str());

  if (!bme.begin(0x76)) 
  {
    DBGPRINTFLN("Could not find a valid BME280 sensor, check wiring!");
    SensorPresent=false;
    output = "BME280 Fail!";
  } else {
    SensorPresent=true;
    output = "BME280 Init Done!";
  }
  /* */
  DBGPRINTFLN("%s",output.c_str());

  if ( SensorPresent)
  {
    /* 
    Serial.println("-- Indoor Navigation Scenario --");
    Serial.println("normal mode, 16x pressure / 2x temperature / 1x humidity oversampling,");
    Serial.println("0.5ms standby period, filter 16x");    
    */
    bme.setSampling(Adafruit_BME280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BME280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BME280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BME280::SAMPLING_X1,     /* humidity */
                    Adafruit_BME280::FILTER_X16,      /* Filtering. */
                    Adafruit_BME280::STANDBY_MS_0_5); /* Standby time. */
  }

  lastTime = millis();
  lastTime -= timerDelay;  
}

// Function that is looped forever
void loop(void)
{
  unsigned long time_trigger = millis();

  ArduinoOTA.handle();  
  server.handleClient();
  
  if (iaqSensor.run())  // If new data is available
  {  
    ArduinoOTA.handle();  
    output = String(time_trigger);
    output += ", " + String(iaqSensor.rawTemperature) + "°C";
    output += ", " + String(iaqSensor.pressure/100) + "hP";
    output += ", " + String(iaqSensor.rawHumidity) + "%";
    output += ", " + String(iaqSensor.gasResistance);
    output += ", " + String(iaqSensor.iaq);
    output += ", " + String(iaqSensor.iaqAccuracy);
    output += ", " + String(iaqSensor.temperature) + "°C";
    output += ", " + String(iaqSensor.humidity) + "%";
    output += ", " + String(iaqSensor.co2Equivalent) + "ppm";
    output += ", " + String(iaqSensor.staticIaq);
    output += ", " + String(iaqSensor.staticIaqAccuracy);
    output += ", " + String(iaqSensor.co2Accuracy);
    
    DBGPRINTFLN("%s",output.c_str());
    
    updateState();
    server.handleClient();
    ArduinoOTA.handle();  
  } else {
    checkIaqSensorStatus();
    server.handleClient();
    ArduinoOTA.handle();  
  }
  /* */
  if ((millis() - lastTime) > timerDelay) 
  {
    ArduinoOTA.handle();  
    DisplayEnvironment();
    lastTime = millis();  
    server.handleClient();
  }
  ArduinoOTA.handle();  
}

void DisplayEnvironment( void)
{
  char buff[64];
  /* ************************************************************** */
  tft.loadFont(AA_FONT_SMALL); // Load another different font
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN,TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  
  sprintf(buff,"IP: %s | AirQuality1", WiFi.localIP().toString().c_str());
  tft.drawString(buff, 0, tft.height()-15);  
  tft.drawFastHLine(2, tft.height()-17, tft.width()-4, TFT_GREEN);
  /* ************************************************************** */
  
  /* ************************************************************** */
  tft.loadFont(AA_FONT_LARGE); // Load another different font 
  tft.setTextColor(TFT_GREEN,TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  if ( SensorPresent)
    sprintf(buff,"T: %2.1fC", bme.readTemperature());
  else
    sprintf(buff,"T: %2.1fC", iaqSensor.temperature);
  
  tft.drawString(buff, 0, 0);  
  /* ************************************************************** */

  /* ************************************************************** */
  tft.loadFont(AA_FONT_LARGE); // Load another different font 
  tft.setTextColor(TFT_GREEN,TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  if ( SensorPresent)
    sprintf(buff,"H: %2.0f%%", bme.readHumidity());
  else
    sprintf(buff,"H: %2.0f%%", iaqSensor.humidity);
  
  tft.drawString(buff, 0, 38);  
  /* ************************************************************** */

  /* ************************************************************** */
  tft.loadFont(AA_FONT_LARGE); // Load another different font 
  tft.setTextColor(TFT_GREEN,TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  if ( SensorPresent)
    sprintf(buff,"P: %4.0fhP", (bme.readPressure()/100));
  else
    sprintf(buff,"P: %4.0fhP", (iaqSensor.pressure/100));
  
  tft.drawString(buff, 0, 38*2);    
  /* ************************************************************** */

  uint32_t clr;
  markerAccuracy="";
  /* ************************************************************** */
  if ( iaqSensor.staticIaqAccuracy)
  {
    if ( iaqSensor.staticIaq < 50)
    {
      clr=TFT_GREEN;
      markerAccuracy="  <polygon points=\"175,0 205,0 190,30\" fill=\"rgb(0, 228, 0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
    } else {
      if ( iaqSensor.staticIaq < 100) 
      {
        clr=TFT_DARKGREEN;  //TFT_YELLOW;  
        markerAccuracy="  <polygon points=\"325,0 355,0 340,30\" fill=\"rgb(146,208,80)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
      } else {
        if ( iaqSensor.staticIaq < 150)
        {
          clr=TFT_YELLOW;    
          markerAccuracy="  <polygon points=\"475,0 505,0 490,30\" fill=\"rgb(255,255,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
        } else {
          if ( iaqSensor.staticIaq < 200)
          {
            clr=TFT_ORANGE;   
            markerAccuracy="  <polygon points=\"625,0 655,0 640,30\" fill=\"rgb(255,126,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n"; 
          } else {
            if (  iaqSensor.staticIaq < 250)
            {
              clr=TFT_RED;
              markerAccuracy="  <polygon points=\"775,0 805,0 790,30\" fill=\"rgb(255,0,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
            } else {
              if ( iaqSensor.staticIaq < 350)
              {
                clr=TFT_VIOLET;    
                markerAccuracy="  <polygon points=\"925,0 955,0 940,30\" fill=\"rgb(153,0,76)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
              } else {
                clr=TFT_BROWN;  
                markerAccuracy="  <polygon points=\"1075,0 1105,0 1090,30\" fill=\"rgb(102,51,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
              }
            }
          }
        }
      }
    }
    tft.fillCircle((tft.width()/8)*7, (36+(36/2)), (30/2), clr);
    tft.loadFont(AA_FONT_SMALL); // Load another different font 
    tft.setTextColor(TFT_BLACK,clr);
    tft.setTextDatum(CC_DATUM);  
    tft.drawString("IAQ",(tft.width()/8)*7, (36+(36/2)));
  }
  /* ************************************************************** */

  markerCO2="";
  /* ************************************************************** */
  if ( iaqSensor.co2Accuracy)
  {  
    if ( iaqSensor.co2Equivalent < 600)
    {
      clr=TFT_GREEN;
      markerCO2 = "  <polygon points=\"175,0 205,0 190,30\" fill=\"rgb(0, 228, 0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
    } else {
      if ( iaqSensor.co2Equivalent < 800) 
      {
        clr=TFT_GREENYELLOW;  
        markerCO2 = "  <polygon points=\"325,0 355,0 340,30\" fill=\"rgb(146,208,80)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
      } else {
        if ( iaqSensor.co2Equivalent < 1500)
        {
          clr=TFT_YELLOW; 
          markerCO2 = "  <polygon points=\"475,0 505,0 490,30\" fill=\"rgb(255,255,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n"; 
        } else {
          if ( iaqSensor.co2Equivalent < 1800)
          {
            clr=TFT_ORANGE; 
            markerCO2 = "  <polygon points=\"625,0 655,0 640,30\" fill=\"rgb(255,126,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
          } else {
            clr=TFT_RED;    
            markerCO2 = "  <polygon points=\"775,0 805,0 790,30\" fill=\"rgb(255,0,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"/>\n";
          }
        }
      }
    }
    tft.fillCircle((tft.width()/8)*7, (36/2), (30/2), clr);
    tft.loadFont(AA_FONT_SMALL); // Load another different font   
    tft.setTextColor(TFT_BLACK,clr);
    tft.setTextDatum(CC_DATUM);  
    tft.drawString("CO2",(tft.width()/8)*7, (36/2));  
  }
  /* ************************************************************** */  
}

// Helper function definitions
void checkIaqSensorStatus(void)
{
  if (iaqSensor.status != BSEC_OK) 
  {
    if (iaqSensor.status < BSEC_OK) 
    {
      output = "BSEC error code : " + String(iaqSensor.status);
      DBGPRINTFLN("%s",output.c_str());
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BSEC warning code : " + String(iaqSensor.status);
      DBGPRINTFLN("%s",output.c_str());
    }
  }

  if (iaqSensor.bme680Status != BME680_OK) 
  {
    if (iaqSensor.bme680Status < BME680_OK) 
    {
      output = "BME680 error code : " + String(iaqSensor.bme680Status);
      DBGPRINTFLN("%s",output.c_str());
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BME680 warning code : " + String(iaqSensor.bme680Status);
      DBGPRINTFLN("%s",output.c_str());
    }
  }
  iaqSensor.status = BSEC_OK;
}

void errLeds(void)
{
#if 0
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
#endif  
}

void loadState(void)
{
  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) 
  {
    // Existing state in EEPROM
    DBGPRINTFLN("Reading state from EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) 
    {
      bsecState[i] = EEPROM.read(i + 1);
      DBGPRINTFLN("0x%X", bsecState[i]);
    }

    iaqSensor.setState(bsecState);
    checkIaqSensorStatus();
  } else {
    // Erase the EEPROM with zeroes
    DBGPRINTFLN("Erasing EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);

    EEPROM.commit();
  }
}

void updateState(void)
{
  bool update = false;
  /* Set a trigger to save the state. Here, the state is saved every STATE_SAVE_PERIOD with the first state being saved once the algorithm achieves full calibration, i.e. iaqAccuracy = 3 */
  if (stateUpdateCounter == 0) 
  {
    if (iaqSensor.iaqAccuracy >= 3) 
    {
      update = true;
      stateUpdateCounter++;
    }
  } else {
    /* Update every STATE_SAVE_PERIOD milliseconds */
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis()) 
    {
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update) 
  {
    iaqSensor.getState(bsecState);
    checkIaqSensorStatus();

    DBGPRINTFLN("Writing state to EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) 
    {
      EEPROM.write(i + 1, bsecState[i]);
      DBGPRINTFLN("0x%X", bsecState[i]);
    }

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
  }
}

void drawGraph( void) 
{
  String out = "\n";
  
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"1300\" height=\"150\">\n";
  //out += "<svg class=\"chart\" width=\"1300\" height=\"150\" aria-labelledby=\"Air Quality Index\" role=\"img\">\n";
  out += "  <title id=\"title\">A bar chart about Air Quality Index</title>\n";
  out += markerAccuracy;
  out += "  <g class=\"bar\">\n";
  out += "    <text x=\"5\" y=\"39\" dy=\".35em\" fill=\"rgb(0,0,0)\">IAQ Index</text> \n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"100\" fill=\"rgb(0, 228, 0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"105\" y=\"39\" dy=\".35em\">Excellent</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"250\" fill=\"rgb(146,208,80)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"255\" y=\"39\" dy=\".35em\">Good</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"400\" fill=\"rgb(255,255,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"405\" y=\"39\" dy=\".35em\">Lightly polluted</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"550\" fill=\"rgb(255,126,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"555\" y=\"39\" dy=\".35em\">Moderately polluted</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"700\" fill=\"rgb(255,0,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"705\" y=\"39\" dy=\".35em\">Heavily polluted</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"850\" fill=\"rgb(153,0,76)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"855\" y=\"39\" dy=\".35em\" fill=\"rgb(255,255,255)\">Severely polluted</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"1000\" fill=\"rgb(102,51,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"1005\" y=\"39\" dy=\".35em\" fill=\"rgb(255,255,255)\">Extremely polluted</text>\n";
  out += "  </g>  \n";
  out += "</svg>\n";
  
  server.send(200, "image/svg+xml", out);
}

void drawGraphCO2( void) 
{
  String out = "\n";
  
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"1300\" height=\"150\">\n";
  //out += "<svg class=\"chart\" width=\"1300\" height=\"150\" aria-labelledby=\"CO2 ppm\" role=\"img\">\n";
  out += "  <title id=\"title\">A bar chart about CO2 ppm</title>\n";
  out += markerCO2;
  out += "  <g class=\"bar\">\n";
  out += "    <text x=\"5\" y=\"39\" dy=\".35em\" fill=\"rgb(0,0,0)\">CO2 ppm</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"100\" fill=\"rgb(0, 228, 0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"105\" y=\"39\" dy=\".35em\">Typical Indoor</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"250\" fill=\"rgb(146,208,80)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"255\" y=\"39\" dy=\".35em\">Poor air</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"400\" fill=\"rgb(255,255,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"405\" y=\"39\" dy=\".35em\">POOR AIR !!!</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"550\" fill=\"rgb(255,126,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"555\" y=\"39\" dy=\".35em\">WARNING !!!</text>\n";
  out += "    <rect width=\"150\" height=\"20\" y=\"30\" x=\"700\" fill=\"rgb(255,0,0)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\"></rect>\n";
  out += "    <text x=\"705\" y=\"39\" dy=\".35em\">WARNING !!!</text>\n";
  out += "  </g>\n";
  out += "</svg>\n";

  server.send(200, "image/svg+xml", out);
}  
