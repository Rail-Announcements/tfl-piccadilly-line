# KeTech PCC audio payload format

These notes describe the format of a KeTech PCC announcement card, as used by the London
Underground 1973 Tube Stock digital voice announcer. Nothing about the format is published,
so everything here was worked out from the card itself. Both the container and the audio
codec are solved.

## Container

The card is a flat array of 256-byte pages. A page's first byte gives its type:

| Type | Meaning |
|---|---|
| `0x04` | Audio payload |
| `0x05` | Audio object |
| `0x06` | Announcement or metadata record |
| `0x07` | Directory |
| `0x0B` | Text |

An object's index page holds 6-byte entries starting at offset 7. Bytes 4 and 5 chain to the
next index page. Each entry is:

```
[ type ][ page_lo ][ page_hi ][ 0x00 ][ crc_lo ][ crc_hi ]
```

The trailing 16-bit value is a CRC-16/XMODEM (poly `0x1021`, init `0x0000`, no reflection,
no final xor) over the page's full 256 bytes, stored little-endian. All 48,756 payload pages
on the reference card verify, so page extraction and ordering are byte-correct and the
payload is stored raw.

Payload pages are audio from first byte to last, with no header or trailer to strip, and
page numbers advance monotonically.

Unused pages are erased to `0xFF` throughout. On the reference card, 48,075 of the 65,536
pages are fully erased, and live data extends to byte 4,470,016 of the 16 MiB image.

### Directories

Type `0x07` pages form the master index. There are 13 of them, chained through bytes 4-5 in
the same way as any other index, and between them they list every audio object on the card
as a type `0x05` child. The first directory page also carries a couple of type `0x02` and
`0x03` children whose purpose is unknown.

Walking the directory chain is an alternative to scanning every page for audio objects. It
gives the same set.

### Text objects

Type `0x0B` pages hold a single string. The layout is seven zero bytes, then a tag byte
`0x41`, then a length byte, then that many bytes of Latin-1 text:

```
0b 00 00 00 00 00 00 41 09 4d 69 6e 64 20 47 61 70 0d
                     │  │  └─ "Mind Gap\r"
                     │  └──── length, 9
                     └─────── tag 'A'
```

Two of the 320 strings end in a carriage return. That is card data, not a formatting
artefact, so keep it.

### Records

Announcement and metadata records are tag-based ASCII: a tag byte, printable characters, and
a NUL. Observed tags:

| Tag | Meaning |
|---|---|
| `0x01` | Message or object id, 4 hex digits |
| `0x02` | Alternate id, 4 hex digits |
| `0x05` | Payload page count of an audio object, 4 hex digits |
| `0x06` | Record marker, always `R` |
| `0x0a` | Address, 8 hex digits |
| `0x0b` | Flag, always `1` |
| `0x0c` | Small value, 2 hex digits |
| `0x0e` | Text reference, `A` followed by 8 hex digits |

Each audio object has exactly one `0x06` child holding its metadata. The declared page count
in tag `0x05` matches the actual number of payload pages for all 742 fragment references on
the reference card, which gives a second integrity check independent of the CRCs.

Announcements reference their fragments through `0x0d 0x0a` records containing an 8-digit
address. That address points at a 6-byte directory entry, not at the audio object itself.

## Audio payload

The payload is a stream of 16-bit words at 4000 words per second, decoding to 16 kHz mono at
64 kbit/s. Each word carries one sample for each of four QMF sub-bands, read big-endian and
stored as sign-magnitude:

| Field | Band | Bits | Width |
|---|---|---|---|
| LF | 0-2 kHz | 0-6 | 7 |
| MLF | 2-4 kHz | 7-10 | 4 |
| MHF | 4-6 kHz | 11-12 | 2 |
| HF | 6-8 kHz | 13-15 | 3 |

The reference implementation confirms this exactly: it demultiplexes with `aptxVal & 0x7F`,
`(aptxVal >> 7) & 0xF`, `(aptxVal >> 11) & 3` and `(aptxVal >> 13) & 7`. Reading the words
little-endian, or the fields as two's complement, breaks all four bands.

### The stolen HF bit

This is the detail that is easiest to miss.

APT-X100 can carry a low-rate auxiliary data channel, and it does so by taking the low bit
of a sub-band away from the audio. On a PCC card the HF field's low bit, word bit 13, is
taken. That is why it is set in every word, and why the HF field only ever holds odd values.
Tell the decoder, so that it reads HF as 2 bits rather than 3.

`chmode 1` does that. Without it, those bits decode as audio: about 2.9% of samples clip and
roughly 76% of the output energy lands in 4-6 kHz. No speech recording looks like that. With
the correction there is no clipping, and about 96% of the energy sits in 0-2 kHz.

Bit 13 carries no recoverable payload; it is always 1. Clearing it across a whole clip
changes the decoded output not at all, because a correctly configured decoder never reads it.

### Bit 7 is audio, except in the aux window

Word bit 7 is MLF's low bit, and for most of a page it carries audio rather than data.
Corrupting it measurably changes the decode, where corrupting bit 13 does nothing:

| Change | Difference from an unmodified decode |
|---|---|
| Clear bit 13 everywhere | no change at all |
| Clear bit 7 in words 0-116 | 15.4 dB |
| Set bit 7 in words 0-116 | −1.9 dB |
| Clear bit 0 (LF's low bit) everywhere | 8.7 dB |

The last 11 words of every 128-word page are the exception. In words 117 to 127, bit 7 is
fixed to the pattern `1 1 0 1 1 1 0 1 1 1 1`, identical on all 48,756 payload pages on the
reference card. A page is 256 bytes, which is exactly 128 words, so the window lines up with
the page boundary.

The encoder wrote it, not the card writer. APT-X100 reserves band 1's low bit in the last 11
words of each 128-word block for its auxiliary channel, and `enc_10003101` in the reference
implementation fills 10 of them with a constant identifying the buffer configuration, most
significant bit first. Read that way, words 117 to 126 give `1101110111`, which is 887. That
is `ENC_IDX_1000F56C[0]` exactly, the table entry for `buffers = 0`. A stray pattern would
hit one of the table's nine entries about 0.9% of the time. Word 127 is reserved too, and is
always 1.

MLF therefore loses one bit of resolution in 8.6% of words, but the bits were never audio.
Across the window the encoder quantises MLF to 3 bits and shifts the code left, which leaves
the low bit free for the index: `std_enc_aptxQuantizeBank(..., 4 - bitcorr_ch1, ...) <<
bitcorr_ch1`. Decoding it correctly means reading MLF in those words as a 3-bit code through
the narrower quantiser bank, which is what `std_aptxChannelDecode` does when `bitcorr_ch1`
is set. Masking bit 7 and decoding as 4 bits is a different operation and not a substitute.

Passing `buffers 0` to hand the window to the decoder does not work: this implementation
looks for the index in words 118 to 127 rather than 117 to 126, so the aux parser fails to
lock and `aptxDec` returns a zeroed buffer. Driving `bitcorr_ch1` directly does work, which
is what `aptxdec`'s `auxwin` argument exists for. `extract.py` runs `auxwin 1`, correcting
all 11 reserved words.

The uncorrected decode has a measurable page-locked defect, and the card's own MLSSA signal
shows it. Fold those 60 seconds of stationary noise at the 512-sample page period and a
clean decode gives a flat profile, because nothing in a stationary signal is synchronous
with a page. The stock decode is not flat, and the defect is not confined to the window:
energy spikes immediately after each one and decays across the page until the next, which is
a sawtooth at the page rate of 31.25 Hz.

| Decode | Peak-to-trough | Ripple at 31.25 Hz | Window vs rest |
|---|---|---|---|
| Stock, bit 7 read as audio | +1.62 dB | 6.49% | 0.923 |
| `auxwin 1`, words 117-127 | +0.41 dB | 0.24% | 1.006 |
| `auxwin 2`, words 117-126 | +0.67 dB | 0.69% | 0.997 |

The correction raises the window's energy toward the page mean rather than lowering it, so
the flattening is not an artefact of attenuating the window.

`auxwin 1` is the better reading. `bitcorr_ch1` covers 11 words while the index fills only
10, so word 127 is reserved but carries no index bit. Its low bit is nevertheless 1 on all
48,756 pages, which no audio bit would be, and correcting it measures flatter.

On a spoken announcement the change sits 25 dB below the signal, and 92 to 96% of it falls
in MLF, which is where a defect in that band's least significant bit belongs. It is audible
on A/B.

Beyond the reserved window, bit 7 carries no data channel. Across a fragment those values
look random. P(1) is 0.495, no bit position is constant across pages, and the packed bytes
hold no ASCII or other structure.

### Codec structure

APT-X100 is four-band sub-band ADPCM at 4:1 compression. Each band runs a backward linear
prediction loop, and the resulting error signal is requantised by a backward adaptive
Laplacian quantiser whose step size adapts to the magnitude of recent error samples. The
four code words are multiplexed into one 16-bit word representing four 16-bit PCM samples.

A few notes on the reference implementation's tables:

- `QTZ_DATA_2` holds the reconstruction levels, sized 2, 4, 8 and 64 magnitudes for the
  four bands.
- `QTZ_DATA_1` holds `{threshold, increment}` pairs. The increments run −15, −15, −7 … 0 …
  +1, +3, +7 … +55, the bounded multiplier for the step-size accumulator, negative for small
  codes and positive for large ones.
- The LF reconstruction table contains `… 8628, 8992, 9683, 9752, 10150 …`. The interval
  between magnitudes 36 and 37 is only 69 wide where its neighbours are about 370, so
  magnitude 37 is almost never selected. The data shows it: across the whole card, LF
  magnitude 37 is 7.7 times depleted relative to its neighbours, in both sign polarities,
  with the missing mass landing on magnitude 36. It looks like an error in the original
  table, preserved by every conforming encoder.

The patent (EP 0398973B1, priority GB8803390, 1988) specifies a different bit allocation of
8:4:2:2 for a 24 kHz-bandwidth variant. That allocation does not fit this stream. Tested
against the card's 997 Hz tone it recovers essentially no signal, while 7:4:2:3 scores 31 dB
above a shuffled-code control.

## Decoding

Use `aptx100/`, which wraps a working APT-X100 implementation. A PCC payload needs the
decoder run big-endian and mono, with the band-3 bit correction enabled:

```sh
aptxdec payload.bin out.wav 1 1 1 -1
```

`extract.py` does this for every fragment and announcement on the card.

## Ground truth on the card

The card carries its own test signals, which are useful for validating a decoder:

- `0386 Speaker Impedance Test` is a steady 997 Hz tone. Under the 4000 words per second
  reading it measures 997.19 Hz, which pins the sample rate to within 0.02%.
- `0387 MLSSA Performance Test` is 60 seconds of stationary noise-like signal.
- `0385 System Sound Test` is five minutes long and repeats on a 30-second loop.

If you score a decoder on the tone, always run the same decode on shuffled codes as a
control. A resonant predictor can manufacture a clean tone out of noise, so only the gap
between the real and shuffled scores means anything.

Two warnings about metrics. Speech recognition confidence is insensitive to tonal balance,
to dynamic range, and to noise in pauses, so it cannot judge band levels. And "how far the
pauses sit below the speech" is not a safe objective: driving it rewards an expander, which
buys pause depth by expanding quiet speech. Real recordings of these announcements have a
speech-internal envelope range of about 12 dB, and anything well above that is distorting
the voice rather than decoding it.
