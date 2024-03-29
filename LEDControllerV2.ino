#include "arduino_secrets.h"
#include "thingProperties.h"
#include <FastLED.h>

#define NUM_LEDS 300
#define LED_PIN 2
#define MIC_PIN A2
#define LED_TYPE WS2812
#define ORDER GRB
#define FPS 5

CRGB leds[NUM_LEDS];
CRGB currColor;
uint8_t maxBrightness;

/** 
 * Notes: 
 * beat8() generates a pattern that resembles %. 0-255 then back to 0
 * beatsin8() generates a pattern that resembles a sin wave
 */
void setup() {
  Serial.begin(115200);

  // Use LED_BUILTIN to indicate when connected to Arduino IoT Cloud
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // On when disconnected, off when connected

  // Decrease resolution to 8 bits (0-255), max resolution of nano 33 iot is 12 bits, default is 10
  analogReadResolution(8);

  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  for(unsigned long const serialBeginTime = millis(); !Serial && (millis() - serialBeginTime > 2000); ) { }

  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::CONNECT, onIoTConnect);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::DISCONNECT, onIoTDisconnect);

  // Setup LEDs
  pinMode(MIC_PIN, INPUT);
  FastLED.addLeds<LED_TYPE, LED_PIN, ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // Limit power draw to 42 W, my power supply is rated for 60
  FastLED.setMaxPowerInVoltsAndMilliamps(12, 3500);
}

void onIoTConnect(){
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Board successfully connected to Arduino IoT Cloud");
}

void onIoTDisconnect(){
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Board disconnected from Arduino IoT Cloud");
}

void loop() {
  ArduinoCloud.update();

  if (fade) { fadeColor(); }
  else if (pulse) { pulseColor(); }
  else if (gamerLights) { swirlRainbow(); }
  else if (mic) { soundPulse(); }
  
  FastLED.show();
}

// ------------- Color Functions -------------
void onLightColorChange() { // Callback from event recieved by alexa on light strip status changed
  Serial.println("Color Change Recieved from Alexa");
  
  uint8_t r, g, b;
  lightColor.getValue().getRGB(r, g, b);

  Serial.println(lightColor.getSwitch());
  Serial.println(r);
  Serial.println(g);
  Serial.println(b);
  Serial.println(lightColor.getBrightness());

  currColor = lightColor.getSwitch() ? CRGB(r, g, b) : CRGB::Black;
  maxBrightness = scale(lightColor.getBrightness(), 100, UINT8_MAX);
  updateStrip();
}

// ------------- Pulse Functions -------------
void onPulseChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Pulse Change Recieved from Alexa");
  Serial.println(pulse);
  onChange(&pulse);
}

void pulseColor() {
  // Has to go back to front, otherwise chain reaction will leave whole strip as the color
  for (int i = NUM_LEDS - 1; i > 0; i--) {
    leds[i] = leds[i - 1];
  }

  leds[0].fadeToBlackBy(UINT8_MAX >> 1); // Dim by 1/2 per call

  EVERY_N_MILLISECONDS(1000 / FPS) {
    leds[0] = currColor;
  }
}

// ------------- Mic Functions -------------
void onMicChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Mic Change Recieved from Alexa");
  Serial.println(mic);
  onChange(&mic);
}

void soundPulse() {
  fill_solid(leds, NUM_LEDS, CHSV(beat8(FPS), UINT8_MAX, getVol()));
}

// ------------- Fade Functions -------------
void onFadeChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Fade Change Recieved from Alexa");
  Serial.println(fade);
  onChange(&fade);
}

DEFINE_GRADIENT_PALETTE( heatmap ) {
0,     0,   255,  0,   //green
64,    0,   255,  0,   //green
128,   255, 255,  0,   //yellow
192,   255,  0,   0,   //red
255,   255,  0,   0 }; //red
CRGBPalette16 palette = heatmap;

void fadeColor() {
  uint8_t maxHeatIndex = getVol();
  int mid = NUM_LEDS >> 1;
  // Scale volume to half the led number
  uint16_t activeLEDs = scale(maxHeatIndex, UINT8_MAX, mid);

  // Display from mid to activeLEDs in each direction
  for (int i = 0; i < activeLEDs; i++) {
    uint8_t heatIndex = scale(i, activeLEDs, maxHeatIndex);
    CRGB color = ColorFromPalette(palette, heatIndex);
    leds[mid + i] = color;
    leds[mid - i] = color;
  }
  fadeToBlackBy(leds, NUM_LEDS, UINT8_MAX >> 2); // Dim by 1/4 per call
}

// ------------- Gamer Functions -------------
void onGamerLightsChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Gamer Lights Change Recieved from Alexa");
  Serial.println(gamerLights);
  onChange(&gamerLights);
}

void swirlRainbow() {
  // IMPORTANT: As of FastLED 3.003.003, fill rainbow has a random red pixel around hue = 60.
  // This is from a compiler optimization issue.
  // The workaround is to change the lines setting hsv.sat from 240 to 255 in colorutils.ccp

  // Two rainbows across the strip
  fill_rainbow(leds, NUM_LEDS, beat8(FPS), UINT8_MAX / (NUM_LEDS >> 1));
}

// ------------- Common Functions -------------
void updateStrip() {
  FastLED.setBrightness(maxBrightness);

  // Functions that dont depend on currColor
  if (!fade && !mic && !gamerLights) {
    for(CRGB& led : leds) {
      led = currColor;
    }
  }
}

// If switch is ON, deactivate all others
CloudSwitch* cswitches[4] = { &pulse, &fade, &gamerLights, &mic };
void onChange(CloudSwitch* ptr) {
  for(CloudSwitch*& cswitch : cswitches) {
    *cswitch = (cswitch == ptr && *ptr == true);
  }
  updateStrip();
}

// Scales value from 0-scaleFrom to 0-scaleTo
// Ex. scale(50, 100, 700) would return 350
float scale(int value, int scaleFrom, int scaleTo) {
  return (value * scaleTo) / scaleFrom;
}

// https://en.wikipedia.org/wiki/Logistic_function
const uint8_t L = UINT8_MAX;
const float k = 0.02;
const uint8_t x_nought = 2;
float calcSCurve(uint8_t vol) {
  return L / (1 + exp(x_nought - k * vol));
}

// Assumes a analogReadResolution of 8 bits
uint8_t getVol() {
  uint8_t vol = calcSCurve(analogRead(MIC_PIN));
  Serial.println(vol);
  return vol;
}
