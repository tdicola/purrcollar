# Cat Purr Detecting Collar

Copyright 2013 Tony DiCola (tony@tonydicola.com)

This is a cat collar which detects when the cat purrs and lights up some LEDs.  An early
demo can be seen [in this video](http://www.youtube.com/watch?v=NSDR7cYQ2L4).  This project
is still a work in progress--see further below for a log of the current progress.

## Hardware

The collar is based on the following hardware:

*	[Teensy 3.0 microcontroller](http://www.pjrc.com/store/teensy3.html)
*	[Small electret microphone](http://www.adafruit.com/products/1063)
*	[Adafruit Flora RGB Neo Pixel LEDs](http://www.adafruit.com/products/1260)

## Software

Note that this is currently a work in progress.  The current purr detection algorithm
is quite susceptible to false positives from audio noise.  Below is a log
of the most recent progress.

### October 3, 2013 

After getting a good recording of a purr and looking at it in a spectrogram it looks like ~21-23 hz is 
really where the main intensity of the purr audio occurs.  Unfortunately there's still quite a lot of noise
detecting pulses in the 20hz range.  More work needs to be done to better isolate the microphone from noise.  You
can also [see the spectrogram of the purr](http://learn.adafruit.com/fft-fun-with-fourier-transforms/cat-purr-detection)
in this [guide I wrote for Adafruit.com](http://learn.adafruit.com/fft-fun-with-fourier-transforms/overview-1).

### September 23, 2013

First version of the code uploaded to github.  Based on some recordings of my cat
purring that I made, I saw pulses at ~100hz as a part of the purr audio signal.  To detect these pulses
the current code samples audio at 1500hz and uses an FFT of size 256 to break down the audio signal 
into its component frequencies.  The purr detection algorithm then looks for 4 or more pulses of 
100hz within a roughly 5 second window of time.  This seems to catch purrs, but unfortunately is
still very susceptible to noise like fur brushing against the microphone, or vibration & noise 
around the house.

### Dependencies

The code depends on [Teensyduino](http://www.pjrc.com/teensy/teensyduino.html) and the 
[Adafruit Neo Pixel library](https://github.com/adafruit/Adafruit_NeoPixel).

## License

This code is released under an MIT license.

Copyright (c) 2013 Tony DiCola

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
