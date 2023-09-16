// NeoPixel Ring simple sketch (c) 2013 Shae Erisson
// Released under the GPLv3 license to match the rest of the
// Adafruit NeoPixel library

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#include <time.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

#define PIN_LEDS        9
#define PIN_MOTOR      13

#define PIN_DEBUG_LED 26
#define PIN_BUTTON 10

#define DELAYVAL 10 
#define NUMPIXELS 20 // Popular NeoPixel ring size

#define WAVES 3

#define DIM_TIME 1.0

struct RGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct Wave {
  uint id;
  RGB from[NUMPIXELS];
  RGB to[NUMPIXELS];
  RGB current[NUMPIXELS];
  uint minLeds;
  uint maxLeds;
  uint numLeds;
  uint steps;
  uint step;
};

Wave waves[WAVES];

Adafruit_NeoPixel pixels(NUMPIXELS, PIN_LEDS, NEO_GRB + NEO_KHZ800);

// unsigned long lastUpdate = 0;

// unsigned long previousMillis = 0;
// const long interval = 10;  // 1 second interval
// int state = 0;

bool sleeping = false;
bool buttonPressed = false;
time_t startTime;
double dimmer = 1.0;




void setup() {
  pinMode(PIN_DEBUG_LED, OUTPUT);
  digitalWrite(PIN_DEBUG_LED, HIGH);

  pinMode(PIN_BUTTON, INPUT);

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)

  waves[0].id = 0;
  waves[0].minLeds = 1;
  waves[0].maxLeds = 1;
  initWave(waves[0]);

  waves[1].id = 1;
  waves[1].minLeds = 0;
  waves[1].maxLeds = 1;
  initWave(waves[1]);

  waves[2].id = 2;
  waves[2].minLeds = 0;
  waves[2].maxLeds = 2;
  initWave(waves[2]);
}


void initWave(Wave& wave) {
  for (int i = 0; i < NUMPIXELS; i++) {
    wave.current[i].r = 0;
    wave.current[i].g = 0;
    wave.current[i].b = 0;
    wave.from[i].r = 0;
    wave.from[i].g = 0;
    wave.from[i].b = 0;
  }
  waveRandomizeNextTarget(wave);
}

void resetLayer(RGB (&a)[NUMPIXELS]) {
  for (int i = 0; i < NUMPIXELS; i++) {
    a[i].r = 0;
    a[i].g = 0;
    a[i].b = 0;
  }
}

void HSVtoRGB(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
  float f, p, q, t;
  int hi;
  h = fmodf(h, 360); // Wrap hue angle in degrees
  hi = (int)(h / 60.0f) % 6;
  f = h / 60.0f - hi;
  p = v * (1 - s);
  q = v * (1 - f * s);
  t = v * (1 - (1 - f) * s);

  switch (hi) {
    case 0: r = v * 255; g = t * 255; b = p * 255; break;
    case 1: r = q * 255; g = v * 255; b = p * 255; break;
    case 2: r = p * 255; g = v * 255; b = t * 255; break;
    case 3: r = p * 255; g = q * 255; b = v * 255; break;
    case 4: r = t * 255; g = p * 255; b = v * 255; break;
    case 5: r = v * 255; g = p * 255; b = q * 255; break;
  }
}

void waveSendToSleep(Wave& wave) {
  memcpy(wave.from, wave.current, sizeof(RGB) * NUMPIXELS);
  resetLayer(wave.to);
  wave.steps = 100;
  wave.step = 0;
}

void waveAwake(Wave& wave) {
  waveRandomizeNextTarget(wave);
}

void waveRandomizeNextTarget(Wave& wave) {

  memcpy(wave.from, wave.to, sizeof(RGB) * NUMPIXELS);
  resetLayer(wave.to);

  // Decide how many LEDs to light up
  wave.numLeds = wave.minLeds;
  if (wave.maxLeds > wave.minLeds) {
    wave.numLeds += rand() % (wave.maxLeds - wave.minLeds + 1); 
  }

  wave.step = 0;
  wave.steps = rand() % 1000 + 100;

  for (int i = 0; i < wave.numLeds; i++) {
    int index = rand() % NUMPIXELS;

    float hue = static_cast<float>(rand()) / RAND_MAX * 360.0f;
    float saturation = 0.9f + static_cast<float>(rand()) / RAND_MAX * 0.1f;
    float brightness = 0.6f + static_cast<float>(rand()) / RAND_MAX * 0.4f; // Brightness range from 0.2 to 1.0

    uint8_t r, g, b;
    HSVtoRGB(hue, saturation, brightness, r, g, b);

    wave.to[index].r = r;
    wave.to[index].g = g;
    wave.to[index].b = b;    
  }


}

void waveDoStep(Wave& wave) {
  wave.step++;
  if (wave.step >= wave.steps) {
    if(sleeping) {
      wave.step--;
      return;
    } else {
      waveRandomizeNextTarget(wave);
    }
  }

  // Calculate sine-based interpolation factor
  double t = 0.5 * (1 - std::cos(M_PI * static_cast<double>(wave.step) / static_cast<double>(wave.steps)));
  // 
  for (int i = 0; i < NUMPIXELS; i++) {
    wave.current[i].r = (wave.from[i].r * (1 - t) + wave.to[i].r * t) * dimmer;
    wave.current[i].g = (wave.from[i].g * (1 - t) + wave.to[i].g * t) * dimmer;
    wave.current[i].b = (wave.from[i].b * (1 - t) + wave.to[i].b * t) * dimmer;
  }

}

void applyMixedWavesToPixels() {
  for (int i = 0; i < NUMPIXELS; ++i) {
    int sum_r = 0;
    int sum_g = 0;
    int sum_b = 0;

    for (uint w = 0; w < WAVES; w++) {
      sum_r += waves[w].current[i].r;
      sum_g += waves[w].current[i].g;
      sum_b += waves[w].current[i].b;
    }

    // Clip to 0-255 range
    sum_r = sum_r > 255 ? 255 : sum_r;
    sum_g = sum_g > 255 ? 255 : sum_g;
    sum_b = sum_b > 255 ? 255 : sum_b;

    // Apply color to pixel
    pixels.setPixelColor(i, pixels.Color(sum_r, sum_g, sum_b));
  }
  pixels.show();
}




void loop() {
  
  if (digitalRead(PIN_BUTTON)) {
    if (!buttonPressed) {      
      buttonPressed = true;
      // event: button pressed
      sleeping = !sleeping;
      if(sleeping) {
        for (uint i = 0; i < WAVES; i++) {
          waveSendToSleep(waves[i]);
        }
      } else {
        for (uint i = 0; i < WAVES; i++) {
          startTime = time(NULL);
          waveAwake(waves[i]);
        }
      }
    }
  } else {
    if(buttonPressed) {
      buttonPressed = false;
      // event: button released
    }
  }
  digitalWrite(PIN_DEBUG_LED, sleeping ? HIGH : LOW);
  

  // Get the current Unix timestamp and calculate elapsed time in minutes
  time_t currentTime = time(NULL);
  double elapsedMinutes = (double)(currentTime - startTime) / 60.0;

  // Calculate brightness multiplier based on elapsed time
  dimmer = 2.0f - (elapsedMinutes / (DIM_TIME / 2.0f));
  dimmer = dimmer > 1.0 ? 1.0 : dimmer;
  dimmer = dimmer < 0.0 ? 0.0 : dimmer;
  if(dimmer <= 0 && !sleeping) {
    sleeping = true;
    for (uint i = 0; i < WAVES; i++) {
      waveSendToSleep(waves[i]);
    }    
  }

  for (uint i = 0; i < WAVES; i++) {
    waveDoStep(waves[i]);
  } 
  applyMixedWavesToPixels();

  delay(20);

}
