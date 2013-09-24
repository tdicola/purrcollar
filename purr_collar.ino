// Cat Purr Detecting Collar
// Copyright 2013 Tony DiCola (tony@tonydicola.com)

// This code is meant to run on a Teensy 3.0 microcontroller which is hooked up to
// a microphone (such as http://www.adafruit.com/products/1063) and 4 neo pixels
// (from http://www.adafruit.com/products/1260).  The pins for these inputs & outputs
// can be adjusted in the configuration variables below.

// Note that this is a work in progress.  The purr detection algorithm currently
// tries to detect a configured number of significant pulses at a certain frequency 
// within a short window of time.  Four or more pulses at 100hz works within ~5 seconds
// is the current configuration below, but it is still quite susceptible to noise.

#define ARM_MATH_CM4
#include <arm_math.h>
#include <Adafruit_NeoPixel.h>


////////////////////////////////////////////////////////////////////////////////
// CONIFIGURATION 
// These values can be changed to alter the behavior of the collar.
////////////////////////////////////////////////////////////////////////////////

// Purr detection configuration
int SAMPLE_RATE_HZ = 1500;             // Sample rate for purr detection.  This is kept low in order to have
                                       // a tighter range of frequencies in each FFT output bin.
int PURR_LOW_FREQ = 100;               // Bottom of frequency range for purr peaks.
int PURR_HIGH_FREQ = 100;              // Top of frequency range for purr peaks.
int PURR_PEAK_THRESHOLD = 2.75;        // Ratio of purr frequency range avg. magnitude greater than
                                       // all other frequency avg. magnitude to detect a purr peak.
int PURR_PEAK_COUNT = 4;               // Count of purr peaks to detect a purr in a window of time.
const int PURR_WINDOW_SIZE = 30;       // Size of consecutive sample windows to look for purr peaks.  With a
                                       // sample rate of 1500hz and FFT size of 256, a window size of 30 peaks
                                       // roughly measures 5 seconds of total time. (each sample is 1/SAMPLE_RATE_HZ*FFT_SIZE seconds)

// Other configuration
const int FFT_SIZE = 256;              // Size of the FFT.  Realistically can only be at most 256 
                                       // without running out of memory for buffers and other state.
const int AUDIO_INPUT_PIN = 14;        // Input ADC pin for audio data.
const int ANALOG_READ_RESOLUTION = 10; // Bits of resolution for the ADC.
const int ANALOG_READ_AVERAGING = 16;  // Number of samples to average with each ADC reading.
const int POWER_LED_PIN = 13;          // Output pin for power LED (pin 13 to use Teensy 3.0's onboard LED).
const int NEO_PIXEL_PIN = 3;           // Output pin for neo pixels.
const int NEO_PIXEL_COUNT = 4;         // Number of neo pixels.
const int MAX_CHARS = 65;              // Max size of the input command buffer
int PIXEL_PULSE_MS = 3000;             // Time in milliseconds to continue pulsing LEDs after purr is detected.
float PIXEL_FREQ_HZ = 1.5;             // How often the pixel pulse cycles per second.


////////////////////////////////////////////////////////////////////////////////
// INTERNAL STATE
// These shouldn't be modified unless you know what you're doing.
////////////////////////////////////////////////////////////////////////////////

IntervalTimer samplingTimer;
float samples[FFT_SIZE*2];
float magnitudes[FFT_SIZE];
int sampleCounter = 0;
int purrWindow[PURR_WINDOW_SIZE];
int purrWindowPosition = 0;
unsigned long pixelPulseEnd = 0;
boolean debugMode = false;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEO_PIXEL_COUNT, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);
char commandBuffer[MAX_CHARS];


////////////////////////////////////////////////////////////////////////////////
// MAIN SKETCH FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

void setup() {
  // Set up serial port.
  Serial.begin(38400);
  
  // Set up ADC and audio input.
  pinMode(AUDIO_INPUT_PIN, INPUT);
  analogReadResolution(ANALOG_READ_RESOLUTION);
  analogReadAveraging(ANALOG_READ_AVERAGING);
  
  // Turn on the power indicator LED.
  pinMode(POWER_LED_PIN, OUTPUT);
  digitalWrite(POWER_LED_PIN, HIGH);
  
  // Initialize neo pixel library and turn off the LEDs
  pixels.begin();
  pixels.show(); 
  
  // Initialize window of purr peaks to zero
  for (int i = 0; i < PURR_WINDOW_SIZE; ++i) {
    purrWindow[i] = 0;
  }
  
  // Clear the input command buffer
  memset(commandBuffer, 0, sizeof(commandBuffer));
  
  // Begin sampling audio
  samplingBegin();
}

void loop() {
  // Calculate FFT if a full sample is available.
  if (samplingIsFull()) {
    // Run FFT on sample data.
    arm_cfft_radix4_instance_f32 fft_inst;
    arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);
    arm_cfft_radix4_f32(&fft_inst, samples);
    // Calculate magnitude of complex numbers output by the FFT.
    arm_cmplx_mag_f32(samples, magnitudes, FFT_SIZE);
    
    // Output magnitudes in debug mode.
    if (debugMode) {
      Serial.println("MAGNITUDES");
      for (int i = 0; i < FFT_SIZE; ++i) {
        Serial.println(magnitudes[i]);
      }
    }
    
    // Run purr detection logic.
    purrLoop();
    
    // Restart audio sampling.
    samplingBegin();
  }
  
  // Update LED pixels.
  pixelsLoop();
  
  // Parse any pending commands.
  parserLoop();
}


////////////////////////////////////////////////////////////////////////////////
// UTILITY FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

// Compute the average magnitude of a target frequency window vs. all other frequencies.
void windowMean(float* magnitudes, int lowBin, int highBin, float* windowMean, float* otherMean) {
    *windowMean = 0;
    *otherMean = 0;
    for (int i = 1; i < FFT_SIZE/2; ++i) {
      if (i >= lowBin && i <= highBin) {
        *windowMean += magnitudes[i];
      }
      else {
        *otherMean += magnitudes[i];
      }
    }
    *windowMean /= (highBin - lowBin) + 1;
    *otherMean /= (FFT_SIZE / 2 - (highBin - lowBin));
}

// Convert a frequency to the appropriate FFT bin it will fall within.
int frequencyToBin(float frequency) {
  float binFrequency = float(SAMPLE_RATE_HZ) / float(FFT_SIZE);
  return int(frequency / binFrequency);
}


////////////////////////////////////////////////////////////////////////////////
// PURR DETECTION FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

void purrLoop() {
  // Compute the average magnitude for the purr frequency vs. other frequencies.
  float purrMean, otherMean;
  windowMean(magnitudes, frequencyToBin(PURR_LOW_FREQ), frequencyToBin(PURR_HIGH_FREQ), &purrMean, &otherMean);
    
  // Detect peaks in the purr frequency and add them to the window of peak counts.
  int isPeak = 0;
  if (otherMean != 0.0 && purrMean / otherMean >= PURR_PEAK_THRESHOLD) {
    isPeak = 1;
  }
  purrWindow[purrWindowPosition] = isPeak;
    
  // Maintain purrWindow as a circular buffer.
  purrWindowPosition += 1;
  if (purrWindowPosition >= PURR_WINDOW_SIZE) {
    purrWindowPosition = 0;
  }
    
  // Sum how many peaks are in the window of peak counts.
  int peakSum = 0;
  for (int i = 0; i < PURR_WINDOW_SIZE; ++i) {
    peakSum += purrWindow[i];
  }
    
  // Detect a purr if enough peaks in the purr frequency happen in the window of recent samples.
  if (peakSum >= PURR_PEAK_COUNT) {
    purrDetected();
  }
}

void purrDetected() {
  pixelPulseEnd = millis() + PIXEL_PULSE_MS;  
}


////////////////////////////////////////////////////////////////////////////////
// SAMPLING FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

void samplingCallback() {
  // Read from the ADC and store the sample data
  samples[sampleCounter] = (float32_t)analogRead(AUDIO_INPUT_PIN);
  // Complex FFT functions require a coefficient for the imaginary part of the input.
  // Since we only have real data, set this coefficient to zero.
  samples[sampleCounter+1] = 0.0;
  // Update sample buffer position and stop after the buffer is filled
  sampleCounter += 2;
  if (sampleCounter >= FFT_SIZE*2) {
    samplingTimer.end();
  }
}

void samplingBegin() {
  // Reset sample buffer position and start callback at necessary rate.
  sampleCounter = 0;
  samplingTimer.begin(samplingCallback, 1000000/SAMPLE_RATE_HZ);
}

boolean samplingIsFull() {
  return sampleCounter >= FFT_SIZE*2;
}


////////////////////////////////////////////////////////////////////////////////
// COMMAND PARSING FUNCTIONS
// These functions allow parsing simple commands input on the serial port.
//
// All commands must end with a semicolon and new line character.
//
// Available commands are:
// DEBUG START;
//  - Output the FFT magnitudes for debugging purposes.
// DEBUG STOP;
//  - Stop outputting FFT magnitudes.
// GET <variable name>;
//  - Get the value of a variable, for example "GET SAMPLE_RATE_HZ;" returns the
//    sample rate.
// SET <variable name> <value>;
//  - Set the value of a variable.  For example "SET SAMPLE_RATE_HZ 2000;" will
//    set the sample rate to 2000hz.  After a value is updated it will be echoed
//    back out to the serial port.
// 
////////////////////////////////////////////////////////////////////////////////

void parserLoop() {
  // Process any incoming characters from the serial port
  while (Serial.available() > 0) {
    char c = Serial.read();
    // Add any characters that aren't the end of a command (semicolon) to the input buffer.
    if (c != ';') {
      c = toupper(c);
      strncat(commandBuffer, &c, 1);
    }
    else
    {
      // Parse the command because an end of command token was encountered.
      parseCommand(commandBuffer);
      // Clear the input buffer
      memset(commandBuffer, 0, sizeof(commandBuffer));
    }
  }
}

// Macro used in parseCommand function to simplify parsing get and set commands for a variable
#define GET_AND_SET(variableName) \
  else if (strcmp(command, "GET " #variableName) == 0) { \
    Serial.println(variableName); \
  } \
  else if (strstr(command, "SET " #variableName " ") != NULL) { \
    variableName = (typeof(variableName)) atof(command+(sizeof("SET " #variableName " ")-1)); \
    Serial.println(variableName); \
  }

void parseCommand(char* command) {
  if (strcmp(command, "DEBUG START") == 0) {
    debugMode = true;
  }
  else if (strcmp(command, "DEBUG STOP") == 0) {
    debugMode = false;
  }
  else if (strcmp(command, "GET FFT_SIZE") == 0) {
    // Only allow reading FFT_SIZE
    Serial.println(FFT_SIZE);
  }
  // Handlers for variables that can be read and written at run time.
  GET_AND_SET(SAMPLE_RATE_HZ)
  GET_AND_SET(PURR_LOW_FREQ)
  GET_AND_SET(PURR_HIGH_FREQ)
  GET_AND_SET(PURR_PEAK_THRESHOLD)
  GET_AND_SET(PURR_PEAK_COUNT)
  GET_AND_SET(PIXEL_PULSE_MS)
  GET_AND_SET(PIXEL_FREQ_HZ)
}

////////////////////////////////////////////////////////////////////////////////
// NEO PIXEL FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

void pixelsLoop() {
  unsigned long time = millis();
  if (time < pixelPulseEnd) {
    pixelsPulse(time);
  }
  else
  {
    pixels.setPixelColor(0, 0);
    pixels.setPixelColor(1, 0);
    pixels.setPixelColor(2, 0);
    pixels.setPixelColor(3, 0);
    pixels.show();
  }
}

void pixelsPulse(unsigned long time) {
  // Generate a signal that goes from 0 to 511 at a pixel pulse frequency  
  float steps = time/((1000.0/PIXEL_FREQ_HZ)/512.0);
  int pos = int(steps) % 512;
  for(int i = 0; i < pixels.numPixels(); ++i) {
    // Offset each pixel so they are out of phase and the color wave is evenly spread out.
    int offset = (512 / pixels.numPixels()) * i;
    // Set each pixel color to a rainbow color based on the 0 to 511 signal
    pixels.setPixelColor(i, pixelsRainbow((pos+offset)%512));
  }
  pixels.show();
}

// Return a color that pulses between rainbow shades based on an input of 0 to 511
unsigned int pixelsRainbow(int pos) {
  // Offset each color component by 170 so they are out of phase with each other. 
  return pixels.Color(pixelsSmoothStep(pos), pixelsSmoothStep((pos+170)%512), pixelsSmoothStep((pos+340)%512));
}

// Function to go smoothly from 0 to 255 back to 0 from an input of 0 to 511
int pixelsSmoothStep(int pos) {
  return pos > 255 ? 511 - pos : pos;
}
