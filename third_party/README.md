# Third-party code

## RNNoise 0.2

Noise suppression with a recurrent neural network, by Jean-Marc Valin
(Xiph.Org, Mozilla and Amazon), BSD license (see `rnnoise/COPYING`).

Source: <https://github.com/xiph/rnnoise/releases/tag/v0.2>

Only the files needed by the library are kept: `rnnoise/include/rnnoise.h`
and the library part of `rnnoise/src` (without the training tools and the
examples). The files are unchanged. They are compiled as C, with the default
model built in, and linked into `VBRecorder.exe`.
