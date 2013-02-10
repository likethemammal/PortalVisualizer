/*

  This was modified from PICCOLO:

    PICCOLO is a tiny Arduino-based audio visualizer...a bit like
    Spectro, but smaller, with microphone input rather than line-in.

    Hardware requirements:
     - Most Arduino or Arduino-compatible boards (ATmega 328P or better).
     - Adafruit Bicolor LED Matrix with I2C Backpack (ID: 902)
     - Adafruit Electret Microphone Amplifier (ID: 1063)
     - Optional: battery for portable use (else power through USB)
    Software requirements:
     - elm-chan's ffft library for Arduino

    Connections:
     - 3.3V to mic amp+ and Arduino AREF pin <-- important!
     - GND to mic amp-
     - Analog pin 0 to mic amp output
     - +5V, GND, SDA (or analog 4) and SCL (analog 5) to I2C Matrix backpack

    Written by Adafruit Industries.  Distributed under the BSD license --
    see license.txt for more information.  This paragraph must be included
    in any redistribution.

    ffft library is provided under its own terms -- see ffft.S for specifics.
*/

// IMPORTANT: FFT_N should be #defined as 128 in ffft.h.  This is different
// than Spectro, which requires FFT_N be 64 in that file when compiling.

#include <avr/pgmspace.h>
#include <ffft.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_GFX.h>   // Core graphics library
#include "HackathonDisplay.h"


#define ADC_CHANNEL 0

int16_t       capture[FFT_N];    // Audio capture buffer
complex_t     bfly_buff[FFT_N];  // FFT "butterfly" buffer
uint16_t      spectrum[FFT_N/2]; // Spectrum output buffer
volatile byte samplePos = 0;     // Buffer position counter

HackathonDisplay *display;


void setup() {
  Serial.begin(9600);
  Serial.println("lol");
  display = new HackathonDisplay();
  display->begin();


  // Init ADC free-run mode; f = ( 16MHz/prescaler ) / 13 cycles/conversion 
  ADMUX  = ADC_CHANNEL; // Channel sel, right-adj, use AREF pin
  ADCSRA = _BV(ADEN)  | // ADC enable
           _BV(ADSC)  | // ADC start
           _BV(ADATE) | // Auto trigger
           _BV(ADIE)  | // Interrupt enable
           _BV(ADPS2) | _BV(ADPS1); // | _BV(ADPS0); // 128:1 / 13 = 9615 Hz
  ADCSRB = 0;                // Free run mode, no high MUX bit
  DIDR0  = 1 << ADC_CHANNEL; // Turn off digital input for ADC pin
  TIMSK0 = 0;                // Timer0 off

  sei(); // Enable interrupts
}

void loop() {
  uint8_t  x;

  ADCSRA |= _BV(ADIE);             // Resume sampling interrupt
  while(ADCSRA & _BV(ADIE)); // Wait for audio sampling to finish
  fft_input(capture, bfly_buff);   // Samples -> complex #s
  samplePos = 0;                   // Reset sample counter
  fft_execute(bfly_buff);          // Process complex data
  fft_output(bfly_buff, spectrum); // Complex -> spectrum

  //display->clearDisplay();
  for(x=0; x < 32; x++) {
    float intensity = spectrum[x * 2] / 255.0;
    display->drawLine(x, 0, x, 15, 0);
    display->drawLine(x, 15, x, 15 - (15 * intensity), 255);
  }
}

ISR(ADC_vect) { // Audio-sampling interrupt
  static const int16_t noiseThreshold = 4;
  int16_t              sample         = ADC; // 0-1023
  
  sample += 45;

  capture[samplePos] =
    ((sample > (512-noiseThreshold)) &&
     (sample < (512+noiseThreshold))) ? 0 :
    sample - 512; // Sign-convert for FFT; -512 to +511

  if(++samplePos >= FFT_N) ADCSRA &= ~_BV(ADIE); // Buffer full, interrupt off
}

