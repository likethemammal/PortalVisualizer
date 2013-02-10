/*

   This Arduino project was created for the 2013 Music Visualizer Hackathon at
   Gainesville Hackerspace.
 
   It includes code from PICCOLO:

      http://learn.adafruit.com/piccolo/code
      Written by Adafruit Industries.  Distributed under the BSD license -- see
      license.txt for more information.  This paragraph must be included in any
      redistribution.

   ffft library is provided under its own terms -- see ffft.S for specifics.
*/

#include <avr/pgmspace.h>
#include <ffft.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_GFX.h>     // Core graphics library
#include "HackathonDisplay.h" // A graphics controller created just for the hackathon!


// This stuff is all for the FFT, which transforms samples of audio input into
// buckets in a sort of spectrum analyzer (the spectrum[] variable), with each
// bucket representing the sound amplitude at a particular part of the
// frequency range. Each bucket represents about 150 Hz.
// 
// The "host" Arduino (the Arduino that your team brought) does all the audio
// calculation and then sends drawing commands to the "GPU" Arduino, which
// actually drives the real RGB matrix.
#define ADC_CHANNEL 0
int16_t       capture[FFT_N];    // Audio capture buffer
complex_t     bfly_buff[FFT_N];  // FFT "butterfly" buffer
uint16_t      spectrum[FFT_N/2]; // Spectrum output buffer
volatile byte samplePos = 0;     // Buffer position counter

// The display variable is responsible for communicating drawing commands to
// the "GPU" Arduino which is hooked up to the actual RGB matrix. One GPU
// Arduino will be provided for your team to use and will already be wired up
// to the actual RGB matrix.
//
// We did it this way because wiring up the RGB matrix is hard (over 20 wires)
// and because driving the display takes a lot of memory and CPU power.
HackathonDisplay *display;


// This function runs one time when the Arduino is powered on.
void setup() {
  // The serial console is useful for debugging.
  Serial.begin(9600);
  Serial.println("Visualizer started!");

  // Initialize the connection to the GPU Arduino.
  display = new HackathonDisplay();
  display->begin();

  // Initialize the FFT.
  initialize_fft();
}

// Runs repeatedly as long as the Arduino is powered on. This loop should
// contain your custom visualizer code.
void loop() {
  // Grab some audio and turn it into spectrum data!
  capture_and_process_audio();

  uint8_t  x;
  for(x=0; x < 32; x++) {
    float intensity = spectrum[x * 2] / 255.0;
    display->drawLine(x, 0, x, 15, 0);
    display->drawLine(x, 15, x, 15 - (15 * intensity), 255);
  }
}






/* ----------------------------------------------------------------------- */
/* --------------------------- ADVANCED STUFF ---------------------------- */
/* ----------------------------------------------------------------------- */
// You don't have to understand the code below this point, but it is pretty
// interesting to see how it all works!



// Tell the CPU to run the ISR(ADC_vect) function (below) until enough samples
// have been collected to fill up the whole capture[] array. Then, process the
// capture[] array into the spectrum[] array.
void capture_and_process_audio() {
  // Sample some audio
  ADCSRA |= _BV(ADIE);             // Resume sampling interrupt
  while(ADCSRA & _BV(ADIE));       // Wait for audio sampling to finish

  // Process the sample
  fft_input(capture, bfly_buff);   // Samples -> complex #s
  samplePos = 0;                   // Reset sample counter
  fft_execute(bfly_buff);          // Process complex data
  fft_output(bfly_buff, spectrum); // Complex -> spectrum

}

// Configure the CPU to automatically grab audio samples whenever we say so.
// After this function runs, the CPU will start collecting audio samples
// whenever the following is called:
// 
//     ADCSRA |= _BV(ADIE);             // Resume sampling interrupt
void initialize_fft() {
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

// The CPU will run this little block of code very frequently to fill up the
// capture[] array. The capture[] array will later be processed into the
// spectrum[] array.
ISR(ADC_vect) {
  static const int16_t noiseThreshold = 4;
  int16_t              sample         = ADC; // 0-1023
  
  // This is a custom offset required by the breadboard circuit used at the
  // hackathon. This value is related to the uneven voltage divider with 110K
  // of resistance on one side and 100K on the other.
  sample += 45;

  capture[samplePos] =
    ((sample > (512-noiseThreshold)) &&
     (sample < (512+noiseThreshold))) ? 0 :
    sample - 512; // Sign-convert for FFT; -512 to +511

  if(++samplePos >= FFT_N) ADCSRA &= ~_BV(ADIE); // Buffer full, interrupt off
}
