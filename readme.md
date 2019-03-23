# DtBlkFx Audio FX Plug-in

DtBlkFx is a Fast-Fourier-Transform (FFT) based VST plug-in.

[![Download Latest](https://img.shields.io/badge/download-latest-green.svg)](https://github.com/dozius/dtblkfx/releases/latest)
[![Github All Releases](https://img.shields.io/github/downloads/dozius/dtblkfx/total.svg)](https://github.com/dozius/dtblkfx/releases/latest)
[![Donate](https://img.shields.io/badge/donate-paypal-blue.svg)](https://www.paypal.me/cisc)

This is a fork of the original DtBlkFx by Darrell Tam. The original source was
not ready for public consumption. I have spent a considerable amount of time
getting this ready for a Github release.

Despite a major overhaul of the project structure, I have only made very small
changes to the source itself to enable x64 builds on windows. There is still a
long way to go to making it clean and easy to hack on.

That being said, anyone with a reasonable amount C++ experience shouldn't find
it too difficult to get started. I am glad to accept pull requests and any other
help I can get.

## Usage examples

* Precision parametric equalizing with sharp-roll off
    * Set the frequencies so accurately that you can adjust individual harmonics
      of a sound
    * Frequency resolutions of up to 0.7 Hz
* Harmonic based (or comb) filtering
    * Set a fundamental frequency and adjust the level of it and its harmonics -
      you can even remove the pitched component of a voice
    * Active harmonic tracking - let DtBlkFx automatically track a sound and
      adjust the level of it's harmonics
* Various types of noise control
    * Change the "contrast" between loud and soft frequencies
    * Adjust only those frequencies below or above a particular threshold
    * Clip frequencies above a particular threshold
    * Sound smearing (phase randomizing)
* Frequency shifting
    * Harmonic shifting by a fixed number of notes
    * Non-harmonic shifting by a fixed frequency
    * Active harmonic repitch - the pitch of your sound is monitored and shifted
      to a destination note (or matched to another channel)
* Various methods of mixing left and right channels
    * Standard Vocoding (frequency enveloping) - make your trumpet rap, string
      section sing or synthesizer talk
    * Harmonic based vocoding - harmonics in one channel are power-matched to
      those in the other (or some predefined waveforms) for a new vocoding sound
    * Convolution-like mixing
    * 2 new mixing algorithms
* Frequency masking
    * A harmonic or threshold mask may be set for any effect (apart from
      vocoding) - for example only shift frequencies that are below the
      threshold

You can select up to 8 of the above effects to be run in series! Combining the
effects in this way allows you to make completely new and surprising sounds.
