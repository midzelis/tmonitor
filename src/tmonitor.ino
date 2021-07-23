

#include <Arduino.h>

// This #include statement was automatically added by the Particle IDE.
#include <Adafruit_BME280_RK.h>

// This #include statement was automatically added by the Particle IDE.
#include <Adafruit_SensorRK.h>



SYSTEM_THREAD(ENABLED);

/*********************************************************************
This is an example for our Monochrome OLEDs based on SSD1306 drivers

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/category/63_98

This example is for a 128x64 size display using I2C to communicate
3 pins are required to interface (2 I2C and one reset)

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen must be included in any redistribution
*********************************************************************/

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "MCP9808.h"
#include <ClosedCube_SHT31D.h>

#define OLED_RESET D4
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_BME280 bme; // I2C
#define SEALEVELPRESSURE_HPA (1013.25)

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

// int random(int maxRand) {
//     return rand() % maxRand;
// }

#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
static const unsigned char logo16_glcd_bmp[] =
{ 0B00000000, 0B11000000,
  0B00000001, 0B11000000,
  0B00000001, 0B11000000,
  0B00000011, 0B11100000,
  0B11110011, 0B11100000,
  0B11111110, 0B11111000,
  0B01111110, 0B11111111,
  0B00110011, 0B10011111,
  0B00011111, 0B11111100,
  0B00001101, 0B01110000,
  0B00011011, 0B10100000,
  0B00111111, 0B11100000,
  0B00111111, 0B11110000,
  0B01111100, 0B11110000,
  0B01110000, 0B01110000,
  0B00000000, 0B00110000 };

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));
uint32_t serial = 0;
MCP9808 mcp = MCP9808();

SHT31D_CC::ClosedCube_SHT31D sht31d;
bool sensorReady = false;

void doreset() {
  Serial.println("Resetting");
  SHT31D_CC::SHT31D_ErrorCode resultSoft = sht31d.softReset();
  Serial.print("Soft Reset return code: ");
  Serial.println(resultSoft);

  SHT31D_CC::SHT31D_ErrorCode resultGeneral = sht31d.generalCallReset();
  Serial.print("General Call Reset return code: ");
  Serial.println(resultGeneral);
}

void periodicStart() {
  // while (sht31d.periodicStart(SHT31D_CC::REPEATABILITY_HIGH, SHT31D_CC::FREQUENCY_10HZ) != SHT31D_CC::NO_ERROR) {
  //   Serial.println("[ERROR] Cannot start periodic mode");
  //   display.println("Cannot start periodic mode");
  //   display.display();
  //   delay(1000);
  //   Serial.println("Attempting reset");
  //
  //   doreset();
  // }
}

void setup() {
  Serial.begin(9600);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C, FALSE); // initialize with the I2C addr 0x3D (for the 128x64)
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  display.clearDisplay();

  display.println("init");
  display.display();

  // delay(5000);

  while (!mcp.begin()) {
    Serial.println("MCP9808 not found");
    display.println("MCP9808 NOT found");
    display.display();
    delay(500);
  }
  display.println("MCP9808 found");
  display.display();
  mcp.setResolution(MCP9808_SLOWEST);

  sensorReady = bme.begin();
  if (!sensorReady) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    display.println("BME280 NOT found");
    display.display();
  }
  display.println("BME280 found");
  display.display();
  // bme.setSampling(Adafruit_BME280::MODE_FORCED);
  bme.setSampling(
    Adafruit_BME280::MODE_FORCED,
    Adafruit_BME280::SAMPLING_X16,
    Adafruit_BME280::SAMPLING_X16,
    Adafruit_BME280::SAMPLING_X16,
    Adafruit_BME280::FILTER_OFF);

  sht31d.begin(0x44);

  serial = sht31d.readSerialNumber();
  display.print("SHT3x Serial #");
  display.println(serial);
  display.display();

  periodicStart();

  display.println("done");
  display.display();
  delay(500);
}

const unsigned long PUBLISH_PERIOD_MS = 60000;
const unsigned long UPDATE_PERIOD_MS = 1000;
unsigned long lastUpdate = millis();
char buf[64];
bool inverted = false;
unsigned long lastInvert = 0;

unsigned long lastPublish = millis();
char eventbuf[1024];
const char *FIELD_SEPARATOR = ";";
const char *EVENT_NAME = "tempSensor";

float celsiusToFahrenheit(float celsius) {
  return celsius * 9.0 / 5.0 + 32.0;
}

void printResult(String text, SHT31D_CC::SHT31D result) {
  if (result.error == SHT31D_CC::NO_ERROR) {
    Serial.print(text);
    Serial.print(": T = ");
    Serial.print(result.t);
    Serial.print(" *C ");
    Serial.print(celsiusToFahrenheit(result.t));
    Serial.println(" *F");
    Serial.print("Humidity = ");
    Serial.print(result.rh);
    Serial.println("%");
  } else {
    Serial.print(text);
    Serial.print(": [ERROR] Code #");
    Serial.println(result.error);

  }
}

struct BME_TEMP {
  float t;
  float p;
  float rh;
  float altitude;
};

struct MCP_TEMP {
  float t;
};

void printValues(BME_TEMP bme_temp, MCP_TEMP mcp_temp, SHT31D_CC::SHT31D sht_temp) {

  Serial.print("Temperature = ");
  Serial.print(bme_temp.t);
  Serial.print(" *C ");
  Serial.print(celsiusToFahrenheit(bme_temp.t));
  Serial.println(" *F");

  Serial.print("Pressure = ");

  Serial.print(bme_temp.p);
  Serial.println(" hPa");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme_temp.altitude);
  Serial.println(" m");

  Serial.print("Humidity = ");
  Serial.print(bme_temp.rh);
  Serial.println(" %");

  Serial.println();
  Serial.print("sht31d Serial #");
  Serial.println(serial);
  printResult("Periodic Mode", sht_temp);
  Serial.println();
  Serial.print("MCP Temp: ");

  Serial.print(mcp_temp.t, 4);
  Serial.print(" *C ");
  Serial.print(celsiusToFahrenheit(mcp_temp.t));
  Serial.println(" *F");

  Serial.println();
}

void displaytemp() {
  bool doit = false;
  if (millis() - lastUpdate >= UPDATE_PERIOD_MS) {
Serial.println("Doing update loop");
    doit = true;
		lastUpdate = millis();
    bme.takeForcedMeasurement();
    Serial.print(".");
    BME_TEMP bme_temp = {
      bme.readTemperature() - 1.0f,
      bme.readPressure() / 100.0,
      bme.readHumidity(),
      bme.readAltitude(SEALEVELPRESSURE_HPA)
    };
    Serial.print(".");
    // SHT31D_CC::SHT31D sht_temp = sht31d.periodicFetchData();
    SHT31D_CC::SHT31D sht_temp = sht31d.readTempAndHumidity(SHT31D_CC::REPEATABILITY_HIGH, SHT31D_CC::MODE_CLOCK_STRETCH, 150);
    Serial.print(".");
    if (sht_temp.error != SHT31D_CC::NO_ERROR) {
      sht31d.periodicStop();
      Serial.print("***");
      doreset();
      periodicStart();
      sht_temp = sht31d.periodicFetchData();
    }
    MCP_TEMP mcp_temp = {
      mcp.getTemperature()
    };
    Serial.print(".");
		display.clearDisplay();

    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);

    //snprintf(buf, sizeof(buf), "%.2f C", temp);
    //display.println(buf);

    snprintf(buf, sizeof(buf), "%.2f F", celsiusToFahrenheit(sht_temp.t));
    display.println(buf);

    snprintf(buf, sizeof(buf), "%.2f %% RH", sht_temp.rh);
    display.println(buf);

    snprintf(buf, sizeof(buf), "%.2f hPa", bme_temp.p);
    display.println(buf);

    printValues(bme_temp, mcp_temp, sht_temp);
    // Serial.println(millis());
    // Serial.println(lastPublish);
    // Serial.println( millis() - lastPublish);
    // Serial.println( PUBLISH_PERIOD_MS );
    // snprintf(eventbuf,
    //   sizeof(eventbuf), "%.02f %s %.02f %s %.02f %s %.02f %s %.02f %s %.02f %s %.02f %s %.02f %s %.02f",
    //   bme_temp.t, FIELD_SEPARATOR, bme_temp.rh, FIELD_SEPARATOR, bme_temp.p, FIELD_SEPARATOR,
    //   mcp_temp.t, FIELD_SEPARATOR, (float)0, FIELD_SEPARATOR, (float)0, FIELD_SEPARATOR,
    //   sht_temp.t, FIELD_SEPARATOR, sht_temp.rh, FIELD_SEPARATOR, (float)0);
    // Serial.print("Event: ");
    // Serial.println(eventbuf);
    if (millis() - lastPublish >= PUBLISH_PERIOD_MS) {
      lastPublish = millis();
      snprintf(eventbuf,
        sizeof(eventbuf), "%.02f %s %.02f %s %.02f %s %.02f %s %.02f %s %.02f %s %.02f %s %.02f %s %.02f",
        bme_temp.t, FIELD_SEPARATOR, bme_temp.rh, FIELD_SEPARATOR, bme_temp.p, FIELD_SEPARATOR,
        mcp_temp.t, FIELD_SEPARATOR, (float) 0, FIELD_SEPARATOR, (float) 0, FIELD_SEPARATOR,
        sht_temp.t, FIELD_SEPARATOR, sht_temp.rh, FIELD_SEPARATOR, (float) 0);
      Serial.print("Publishing: ");
      Serial.println(eventbuf);
      Particle.publish(EVENT_NAME, eventbuf, PRIVATE);
    }
  }
  if (millis() - lastInvert >= 30000) {
    Serial.print("inverting");
    lastInvert = millis();
    inverted = !inverted;
    display.invertDisplay(inverted);
    doit = true;
    Serial.print("inverted");
  }
  if (doit) {
    display.display();
  }
}
void ledtest() {
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C, FALSE);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done

  display.display(); // show splashscreen
  delay(2000);
  display.clearDisplay();   // clears the screen and buffer

  // draw a single pixel
  display.drawPixel(10, 10, WHITE);
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw many lines
  testdrawline();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw rectangles
  testdrawrect();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw multiple rectangles
  testfillrect();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw mulitple circles
  testdrawcircle();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw a white circle, 10 pixel radius
  display.fillCircle(display.width()/2, display.height()/2, 10, WHITE);
  display.display();
  delay(2000);
  display.clearDisplay();

  testdrawroundrect();
  delay(2000);
  display.clearDisplay();

  testfillroundrect();
  delay(2000);
  display.clearDisplay();

  testdrawtriangle();
  delay(2000);
  display.clearDisplay();

  testfilltriangle();
  delay(2000);
  display.clearDisplay();

  // draw the first ~12 characters in the font
  testdrawchar();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw scrolling text
  testscrolltext();
  delay(2000);
  display.clearDisplay();

  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Hello, world!");
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println(3.141592);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print("0x"); display.println(0xDEADBEEF, HEX);
  display.display();
  delay(2000);

  // miniature bitmap display
  display.clearDisplay();
  display.drawBitmap(30, 16,  logo16_glcd_bmp, 16, 16, 1);
  display.display();

  // invert the display
  display.invertDisplay(true);
  delay(1000);
  display.invertDisplay(false);
  delay(1000);

  // draw a bitmap icon and 'animate' movement
  testdrawbitmap(logo16_glcd_bmp, LOGO16_GLCD_HEIGHT, LOGO16_GLCD_WIDTH);

}

void loop() {


    displaytemp();

}




void testdrawbitmap(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  uint8_t icons[NUMFLAKES][3];

  // initialize
  for (uint8_t f=0; f< NUMFLAKES; f++) {
    icons[f][XPOS] = random(display.width());
    icons[f][YPOS] = 0;
    icons[f][DELTAY] = random(5) + 1;

    Serial.print("x: ");
    Serial.print(icons[f][XPOS], DEC);
    Serial.print(" y: ");
    Serial.print(icons[f][YPOS], DEC);
    Serial.print(" dy: ");
    Serial.println(icons[f][DELTAY], DEC);
  }


  while (1) {
    // draw each icon
    for (uint8_t f=0; f< NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], logo16_glcd_bmp, w, h, WHITE);
    }
    display.display();
    delay(200);

    // then erase it + move it
    for (uint8_t f=0; f< NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS],  logo16_glcd_bmp, w, h, BLACK);
      // move it
      icons[f][YPOS] += icons[f][DELTAY];
      // if its gone, reinit
      if (icons[f][YPOS] > display.height()) {
	icons[f][XPOS] = random(display.width());
	icons[f][YPOS] = 0;
	icons[f][DELTAY] = random(5) + 1;
      }
    }
   }
}


void testdrawchar(void) {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  for (uint8_t i=0; i < 168; i++) {
    if (i == '\n') continue;
    display.write(i);
    if ((i > 0) && (i % 21 == 0))
      display.println();
  }
  display.display();
}

void testdrawcircle(void) {
  for (int16_t i=0; i<display.height(); i+=2) {
    display.drawCircle(display.width()/2, display.height()/2, i, WHITE);
    display.display();
  }
}

void testfillrect(void) {
  uint8_t color = 1;
  for (int16_t i=0; i<display.height()/2; i+=3) {
    // alternate colors
    display.fillRect(i, i, display.width()-i*2, display.height()-i*2, color%2);
    display.display();
    color++;
  }
}

void testdrawtriangle(void) {
  for (int16_t i=0; i<min(display.width(),display.height())/2; i+=5) {
    display.drawTriangle(display.width()/2, display.height()/2-i,
                     display.width()/2-i, display.height()/2+i,
                     display.width()/2+i, display.height()/2+i, WHITE);
    display.display();
  }
}

void testfilltriangle(void) {
  uint8_t color = WHITE;
  for (int16_t i=min(display.width(),display.height())/2; i>0; i-=5) {
    display.fillTriangle(display.width()/2, display.height()/2-i,
                     display.width()/2-i, display.height()/2+i,
                     display.width()/2+i, display.height()/2+i, WHITE);
    if (color == WHITE) color = BLACK;
    else color = WHITE;
    display.display();
  }
}

void testdrawroundrect(void) {
  for (int16_t i=0; i<display.height()/2-2; i+=2) {
    display.drawRoundRect(i, i, display.width()-2*i, display.height()-2*i, display.height()/4, WHITE);
    display.display();
  }
}

void testfillroundrect(void) {
  uint8_t color = WHITE;
  for (int16_t i=0; i<display.height()/2-2; i+=2) {
    display.fillRoundRect(i, i, display.width()-2*i, display.height()-2*i, display.height()/4, color);
    if (color == WHITE) color = BLACK;
    else color = WHITE;
    display.display();
  }
}

void testdrawrect(void) {
  for (int16_t i=0; i<display.height()/2; i+=2) {
    display.drawRect(i, i, display.width()-2*i, display.height()-2*i, WHITE);
    display.display();
  }
}

void testdrawline() {
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, WHITE);
    display.display();
  }
  for (int16_t i=0; i<display.height(); i+=4) {
    display.drawLine(0, 0, display.width()-1, i, WHITE);
    display.display();
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(0, display.height()-1, i, 0, WHITE);
    display.display();
  }
  for (int16_t i=display.height()-1; i>=0; i-=4) {
    display.drawLine(0, display.height()-1, display.width()-1, i, WHITE);
    display.display();
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i=display.width()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, i, 0, WHITE);
    display.display();
  }
  for (int16_t i=display.height()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, 0, i, WHITE);
    display.display();
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, WHITE);
    display.display();
  }
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(display.width()-1, 0, i, display.height()-1, WHITE);
    display.display();
  }
  delay(250);
}

void testscrolltext(void) {
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10,0);
  display.clearDisplay();
  display.println("scroll");
  display.display();

  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrolldiagright(0x00, 0x07);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x07);
  delay(2000);
  display.stopscroll();
}
