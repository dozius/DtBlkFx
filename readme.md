# DtBlkFx Audio FX Plug-in

DtBlkFx is a Fast-Fourier-Transform (FFT) based VST plug-in.

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

# License
DtBlkFx is freely distributable and is covered by the terms of the GNU licensing
agreement.

This is a fork of the original source by Darrell Tam. As with the original, this
fork is also not ready for wide public consumption. I have only made very small
changes thus far to enable x64 builds on windows. If you desire to have a copy
of the modified source you can contact me here on GitHub or via
[email](mailto:dozius@gmail.com).
