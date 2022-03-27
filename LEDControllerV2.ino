#include "arduino_secrets.h"
#include "thingProperties.h"
#include <FastLED.h>

#define NUM_LEDS 10 // 50
#define LED_PIN 2
#define MIC_PIN A2
#define LED_TYPE WS2811
#define ORDER BRG
#define FPS 30

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
  for(unsigned long const serialBeginTime = millis(); !Serial && (millis() - serialBeginTime > 2000); ) { }
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found

  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::CONNECT, onIoTConnect);
  ArduinoCloud.addCallback(ArduinoIoTCloudEvent::DISCONNECT, onIoTDisconnect);

  pinMode(MIC_PIN, INPUT);
  FastLED.addLeds<LED_TYPE, LED_PIN, ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(12, 3500); // Limit power draw to 42 W, my power supply is rated for 60
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
  Serial.println(lightColor.getSwitch());
  byte r, g, b;
  lightColor.getValue().getRGB(r, g, b);
  Serial.println(r);
  Serial.println(g);
  Serial.println(b);
  Serial.println(lightColor.getBrightness());

  if (lightColor.getSwitch()) {
    currColor = CRGB(r, g, b);
  } else {
    currColor = CRGB::Black;
  }
  maxBrightness = scale(lightColor.getBrightness(), 100, 255); // Scales brightness from 0-100 to 0-255
  updateStrip();
}

// ------------- Pulse Functions -------------
void onPulseChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Pulse Change Recieved from Alexa");
  Serial.println(pulse);

  pulse ? pulseOn() : updateStrip();
}

void sendPulse(bool (*conditionalFunc)(), CRGB color) {
  for (int i = NUM_LEDS - 1; i > 0; i--) { // Has to go back to front, otherwise chain reaction will leave whole strip as the color
    leds[i] = leds[i - 1];
  }

  leds[0].fadeToBlackBy(64); // Creates a fading trail

  if (conditionalFunc()) { // Send a pulse if condition is met
    leds[0] = color;
  }

  FastLED.delay(50);
}

bool pulseTimer() {
  EVERY_N_MILLISECONDS(1000) {
    return true;
  }
  return false;
}

void pulseColor() { 
  sendPulse(&pulseTimer, currColor);
}

void pulseOn() {
  fade = false;
  gamerLights = false;
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
  uint16_t vol = analogRead(MIC_PIN);
//  Serial.println(vol);
  fill_solid(leds, NUM_LEDS, CHSV(beat8(FPS), 255, calcSCurve(vol)));
}

void micOn() {
  fade = false;
  pulse = false;
  gamerLights = false;
}

// ------------- Fade Functions -------------
void onFadeChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Fade Change Recieved from Alexa");
  Serial.println(fade);

  fade ? fadeOn() : updateStrip();
}

bool pulseSound() {
  return analogRead(MIC_PIN) > x_nought;
}

DEFINE_GRADIENT_PALETTE( heatmap ) {
0,     0,   255,  0,   //green
64,    0,   255,  0,   //green
128,   255, 255,  0,   //yellow
192,   255,  0,   0,   //red
255,   255,  0,   0 }; //red
CRGBPalette16 palette = heatmap;

void fadeColor() {
  uint16_t vol = analogRead(MIC_PIN);
  uint8_t maxHeatIndex = calcSCurve(vol); // Will never reach all active leds bc will never be 255
  int mid = NUM_LEDS >> 1; // Divide by 2
  uint16_t activeLEDs = scale(maxHeatIndex, 255, mid); // Scale volume to number of leds

  // Display from mid to activeLEDs in each direction
  for (int i = 0; i < activeLEDs; i++) {
    uint8_t heatIndex = scale(i, activeLEDs, maxHeatIndex);
    CRGB color = ColorFromPalette( palette, heatIndex);
    leds[mid + i] = color;
    leds[mid - i] = color;
  }
  fadeToBlackBy(leds, NUM_LEDS, 5);
}

void fadeOn() {
  pulse = false;
  gamerLights = false;
  mic = false;
}

// ------------- Gamer Functions -------------
void onGamerLightsChange() { // Represented as a "Smart Switch", Boolean
  Serial.println("Gamer Lights Change Recieved from Alexa");
  Serial.println(gamerLights);

  gamerLights ? gamerOn() : updateStrip();
}

void swirlRainbow() {
  // IMPORTANT: As of FastLED 3.003.003, fill rainbow has a random red pixel around hue = 60.
  // This is from a compiler optimization issue.
  // The workaround is to change the lines setting hsv.sat from 240 to 255 in colorutils.ccp
  fill_rainbow(leds, NUM_LEDS, beat8(FPS), 255 / NUM_LEDS);
}

void gamerOn() {
  fade = false;
  pulse = false;
  mic = false;
}

// ------------- Common Functions -------------
void updateStrip() {
  FastLED.setBrightness(maxBrightness);
  if (!fade && !mic && !gamerLights) { // Functions that dont depend on currColor
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = currColor;
      FastLED.delay(25);
    }
  }
}

float scale(int value, int scaleFrom, int scaleTo) {
  return (value * scaleTo) / scaleFrom;
}
