#include "arduino_secrets.h"
#include "thingProperties.h"
#include <FastLED.h>

#define NUM_LEDS 10 // 50
#define LED_PIN 2
#define MIC_PIN A2
#define LED_TYPE WS2811
#define ORDER BRG
#define FPS 30

CRGB led[NUM_LEDS];
CRGB currColor;
uint8_t maxBrightness;

// Pulse Variables
uint8_t pulseCount = 1;

/** 
 * Notes: 
 * beat8() generates a pattern that resembles %. 0-255 then back to 0
 * beatsin8() generates a pattern that resembles a sin wave
 */
void setup() {
  Serial.begin(9600);
  for(unsigned long const serialBeginTime = millis(); !Serial && (millis() - serialBeginTime > 5000); ) { }
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found

  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  pinMode(MIC_PIN, INPUT);
  FastLED.addLeds<LED_TYPE, LED_PIN, ORDER>(led, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(12, 3500); // Limit power draw to 42 W, my power supply is rated for 60
}

void loop() {
  ArduinoCloud.update();

  if (fade) { fadeColor(); }
  if (pulse) { pulseColor(); }
  if (gamerLights) { swirlRainbow(2); }
  if (mic) { soundPulse(); }
  
  FastLED.show();
}

// ------------- Color Functions -------------
void onLightColorChange() { // Callback from event recieved by alexa on light strip status changed
  Serial.println("Color Change Recieved from Alexa");
  Serial.println(lightColor.getSwitch());
  byte r, g, b;
  lightColor.getValue().getRGB(r, g, b);
  Serial.println(r);
  Serial.println(g);
  Serial.println(b);
  Serial.println(lightColor.getBrightness());

  if (lightColor.getSwitch()) {
    currColor = CRGB(r, g, b);
    maxBrightness = lightColor.getBrightness() * 2.55;
  } else {
    currColor = CRGB::Black;
  }
  
  updateStrip();
}

// ------------- Fade Functions -------------
void onFadeChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Fade Change Recieved from Alexa");
  Serial.println(fade);

  fade ? fadeOn() : updateStrip();
}

void fadeColor() {
  EVERY_N_MILLISECONDS(50) {
    FastLED.setBrightness(beatsin8(FPS) * maxBrightness / 255); // Scale the brightness to the max
  }
}

void fadeOn() {
  pulse = false;
  gamerLights = false;
  mic = false;
}

// ------------- Pulse Functions -------------
void onPulseChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Pulse Change Recieved from Alexa");
  Serial.println(pulse);

  pulse ? pulseOn() : updateStrip();
}

void pulseColor() {
//  EVERY_N_MILLISECONDS(50) {
//    for (int i = NUM_LEDS - 1; i > 0; i--) { // Has to go back to front, otherwise chain reaction will leave whole strip as the color
//      led[i] = led[i - 1];
//    }
//    pulseCount++;
//    sendPulse(2);
    fadeToBlackBy(led, NUM_LEDS, 32);
    int pos = beat8(FPS) % NUM_LEDS;
    led[pos] = currColor;
//  }
}

void sendPulse(int numPulses) {
  if (pulseCount % (NUM_LEDS / numPulses) == 0) {
    led[0] = currColor;
  } else {
    led[0].fadeToBlackBy(32);
  }
}

void startPulse() {
  pulseCount = 1;
  led[0] = currColor;
}

void pulseOn() {
  fade = false;
  gamerLights = false;
  mic = false;
  startPulse();
}

// ------------- Gamer Functions -------------
void onGamerLightsChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Gamer Lights Change Recieved from Alexa");
  Serial.println(gamerLights);

  gamerLights ? gamerOn() : updateStrip();
}

void swirlRainbow(uint8_t animSpeed) {
  EVERY_N_MILLISECONDS(50) {
    // IMPORTANT: As of FastLED 3.003.003, fill rainbow has a random red pixel around hue = 60.
    // This is from a compiler optimization issue.
    // The workaround is to change the lines setting hsv.sat from 240 to 255 in colorutils.ccp
    fill_rainbow(led, NUM_LEDS, beat8(FPS), 255 / NUM_LEDS);
  }
}

void gamerOn() {
  fade = false;
  pulse = false;
  mic = false;
}

// ------------- Mic Functions -------------
void onMicChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Mic Change Recieved from Alexa");
  Serial.println(mic);

  mic ? micOn() : updateStrip();
}

// https://en.wikipedia.org/wiki/Logistic_function
const uint8_t L = 255;
const float k = -0.01;
const uint8_t x_nought = 120;
float calcSCurve(uint16_t vol) {
  return (L / (1 + exp(k * (vol - x_nought))));
}

void soundPulse() {
  EVERY_N_MILLISECONDS(50) {
    uint16_t vol = analogRead(MIC_PIN);
//    Serial.println(vol);
    fill_solid(led, NUM_LEDS, CHSV(beat8(FPS), 255, calcSCurve(vol)));
  }
}

void micOn() {
  fade = false;
  pulse = false;
  gamerLights = false;
}

// ------------- Common Functions -------------
void updateStrip() {
  FastLED.setBrightness(maxBrightness);
  for (int i = 0; i < NUM_LEDS; i++) {
    led[i] = currColor;
    FastLED.show();
    FastLED.delay(1);
  }
}
