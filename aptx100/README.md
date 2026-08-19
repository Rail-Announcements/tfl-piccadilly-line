# APT-X100 decoder

The audio on a PCC card is APT-X100. This directory holds a working decoder for it.

## Licence and attribution

`libaptx100/` is not original work. It is Copyright (c) 2014-2026 Maxim V. Anisiutkin,
redistributed under the LGPL as declared by the upstream project. See `NOTICE` for the full
attribution and `COPYING.LESSER` for the licence text. Nothing under `libaptx100/` has been
modified.

`main.cpp`, `Makefile` and this README were written for this project.

## Where this came from

`libaptx100/` is third-party source, taken unmodified from the foobar2000 input plugin
[`foo_input_apt-x100`](https://sourceforge.net/projects/dvdadecoder/files/foo_input_apt-x100/).
The download page describes the release as binaries only, but the archive also carries the
full C++ source under `src/`.

The plugin exists because cinema DTS used APT-X100 for film audio, so the film-restoration
community needed a way to read `.AUD` and `.AUE` discs. Its identifier names and structure
offsets read like a recovery of the original library rather than a clean-room rewrite. It is
the only complete APT-X100 implementation available in the open.

`libaptx100/` is kept exactly as upstream ships it, which is what keeps the attribution in
`NOTICE` accurate. Only three of its files are compiled: `aptx100/aptx100.cpp`,
`aptx100/qmf.cpp` and `aptx100/mmx.cpp`. The encoder and binding paths go unused.

`main.cpp` reads a raw PCC payload, runs the decoder, and writes a WAV.

## Building

```
make
```

This produces `../aptxdec`, which `extract.py` calls.

## Usage

```
aptxdec <payload.bin> <out.wav> [msb] [channels] [chmode] [buffers] [auxwin]
```

A PCC payload needs the arguments `1 1 1 -1 1`:

| Argument | Value | Why |
|---|---|---|
| `msb` | 1 | The stream is big-endian, so each 16-bit word is byte-swapped first. |
| `channels` | 1 | PCC audio is mono. |
| `chmode` | 1 | Enables the band-3 bit correction. |
| `buffers` | -1 | No auxiliary data buffer is needed for playback. |
| `auxwin` | 1 | Band-1 correction over the aux window. See below. |

`auxwin` selects how the band-1 auxiliary window is handled. `1` sets `bitcorr_ch1` over
words 117-127 of each page, which is the correction a PCC stream calls for, and is what
`extract.py` uses. `2` corrects words 117-126 instead, leaving word 127 as audio.

The other two values are for comparison. `-1` decodes through `aptxDecode` with no
correction, and `0` runs that same uncorrected decode through a local copy of the per-word
loop, reproducing it bit for bit. Use `0` rather than `-1` as the baseline when measuring,
so both sides of the comparison come from the same code path.

`chmode 1` is not optional. APT-X100 can carry an auxiliary data channel, and it does so by
taking the low bit of a sub-band away from the audio. A PCC card does that with band 3, so
word bit 13 is data rather than audio and the decoder has to skip it. Leave the correction
off and about 2.9% of samples clip, with roughly 76% of the output energy in 4-6 kHz. Turn
it on and nothing clips, with about 96% of the energy in 0-2 kHz.

For the measurements behind that, see
[The stolen HF bit](../docs/audio-format.md#the-stolen-hf-bit).

## Why not use libopenaptx

[libopenaptx](https://github.com/pali/libopenaptx) implements the later Bluetooth-era aptX.
It shares the 7/4/2/3 bit allocation but not the quantiser tables, and its tables do not fit
this data.
