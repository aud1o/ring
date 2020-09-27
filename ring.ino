#include <Adafruit_NeoPixel.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

ClickEncoder *encoder;
int16_t last, value;

void timerIsr() {
  encoder->service();
}

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define IDLE_DELAY   30
#define WIRE_ADDRESS  8 
#define NEO_PIN       6
#define NUMPIXELS    24
#define FIVE_SECONDS_MS 2000

Adafruit_NeoPixel pixels(NUMPIXELS, NEO_PIN, NEO_GRBW + NEO_KHZ800);

uint8_t currentVolume = 0;
uint16_t timeSinceChange = 0;
uint8_t ringIsOff = 0;

int cooldown = 0;
String volsource = "";

boolean newData = false;
const byte numChars = 6;          // maximum Serial message length
char receivedChars[numChars];     // an array to store the received data

float volume = 0;

void setup() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.setTextSize(4);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.display();
  delay(500); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  
  pixels.begin();
  Serial.begin(9600);
  encoder = new ClickEncoder(A1, A0, A2);

  Timer1.initialize(480);
  Timer1.attachInterrupt(timerIsr);
  
  last = -1;
}

void loop() {
  value += encoder->getValue();

  if (cooldown > 0) cooldown --;
  
  if (value != last and newData == false) {
    Serial.println("rotary");
    newData = true;
     if (volume >= 0 and volume <= 48) {
      if (value > last && volume < 48) volume ++;
      if (value < last && volume > 0) volume --;
    }
    last = value;
    Serial.print("volume:");
    Serial.println(int(volume * 100/48));
    cooldown = 100;
    volsource = "rotary";
  }
  
  if (cooldown == 0 and value == last and Serial.available() and newData == false) {
    recvWithEndMarker();
    volume = int(atoi(receivedChars) * 0.48);
    volsource = "serial";
  }
  
  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    if (b == ClickEncoder::Clicked) {
      display.invertDisplay(true);
      delay(20);
      display.invertDisplay(false);
    }
    if (b == ClickEncoder::DoubleClicked) Serial.println("doubleclick");
    if (b == ClickEncoder::Pressed) Serial.println("press");
    if (b == ClickEncoder::Held) Serial.println("hold");
    if (b == ClickEncoder::Released) Serial.println("release");
  }
  
  if (newData == true) {
    newData = false;

    if (volsource == "rotary") {
      currentVolume = round((volume / 48) * pixels.numPixels());
      renderPixels(0, 0, 0, 20);
    }
    if (volsource == "serial") {
      for (uint8_t i = 0; i < pixels.numPixels(); i++) {
        if(round((volume / 48) * pixels.numPixels()) > currentVolume) currentVolume++;
        else if(round((volume / 48) * pixels.numPixels()) < currentVolume) currentVolume--;
        else { break; }
        delayAndListen(50);
        renderPixels(0, 0, 0, 20);
      }
    }
    timeSinceChange = 0;
    ringIsOff = 0;
  } else {
    if (!ringIsOff && timeSinceChange >= FIVE_SECONDS_MS) {
      fadeOut(0, 0, 0, 20);
      ringIsOff = 1;
    } else {
      timeSinceChange += IDLE_DELAY;
    }

    delayAndListen(IDLE_DELAY);
    return;
  }
}

void renderPixels(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  display.clearDisplay();
  int percent = round(4.16 * currentVolume);
  float x = 55;
  if (percent > 10) x = 41;
  if (percent == 100) x = 27;
  display.setCursor(x, 20);     // Start at top-left corner
  display.println(percent);
  display.display();

  pixels.clear();

  for (uint8_t i = 0; i < pixels.numPixels(); i++) {
    if (currentVolume > i) {
      pixels.setPixelColor(i, pixels.Color(r, g, b, w));
    } else {
      pixels.setPixelColor(i, pixels.Color(0, 0, 0, 0));
    }
  }

  pixels.show();
}

// This could probably made a bit smoother with each color
// having their own relative step
void fadeOut(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  for(;r > 0 || g > 0 || b > 0 || w > 1;) {
    if (r > 0) r--;
    if (g > 0) g--;
    if (b > 0) b--;
    if (w > 1) w--;

    renderPixels(r, g, b, w);

    delayAndListen(20);
  }
}

void delayAndListen(int times) {
  for (int i = 0; i < times; i++) {
    value += encoder->getValue();
  
    if (value != last) {
      newData = true;
      break;
    } else {
      delay(1);
    }
  }
}

void recvWithEndMarker() {
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();
    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      receivedChars[ndx] = '\0'; // terminate the string
      ndx = 0;
      newData = true;
    }
  }
}
