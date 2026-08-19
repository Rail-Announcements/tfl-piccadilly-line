# KeTech PCC audio payload format

These notes describe the format of a KeTech PCC announcement card, as used by the London
Underground 1973 Tube Stock digital voice announcer. Nothing about the format is published,
so everything here was worked out from the card itself. Both the container and the audio
codec are solved.

## What APT-X100 is

The rest of this document assumes the codec, so this section covers it from the start: what
APT-X100 is, how it turns audio into 16-bit words, and which of its design choices shaped the
way the card stores announcements.

APT-X100 compresses audio at a fixed 4:1. Every four 16-bit PCM samples become one 16-bit
word, on any material, with no variation from one moment to the next. At the 16 kHz sampling
rate a PCC card uses, 256 kbit/s of PCM goes in and 64 kbit/s comes out.

The codec came out of research at Queen's University Belfast in the 1980s and was
commercialised by Audio Processing Technology; its founding patents (GB8803390, filed 1988)
have expired. It was built for live links and playback hardware rather than for files:
studio-to-transmitter radio links, ISDN lines, and cinema, where DTS film soundtracks carried
APT-X100 on CD-ROM. The aptX codecs used over Bluetooth are its descendants, and keep the
four-band structure and the 7/4/2/3 bit allocation described here, though not the tables.

It is not the kind of codec MP3 and AAC are. There is no psychoacoustic model, no transform
into frequency bins, no frames, no headers, and no variable bit rate. APT-X100 saves bits by
predicting the waveform and coding only what the prediction misses, rather than by discarding
detail a listener is unlikely to notice. What that buys is a short fixed delay, a constant
output rate, cheap fixed-point arithmetic, and a stream that survives being cut, joined and
corrupted, all of which suit a link or an announcer better than a lower bit rate would.

### How the encoder builds a word

Each group of four input samples goes through four stages.

1. **Split into bands.** A quadrature mirror filter (QMF) tree divides 0-8 kHz into four 2 kHz
   bands, each running at a quarter of the input rate. Four input samples therefore produce
   exactly one sample in each band, which is where the payload's 4000 words per second comes
   from. Speech puts most of its energy in the lowest band, and the split is what lets the
   encoder spend its bits there.
2. **Predict.** Each band runs its own ADPCM loop: adaptive differential pulse-code
   modulation, meaning it codes the difference between the signal and a prediction of it
   rather than the signal itself. A predictor estimates the next sample from the samples
   already coded, and only the error in that estimate gets coded, which takes fewer bits
   because the error is smaller than the sample. The predictor has the shape used in ITU-T
   G.726, the telephone ADPCM standard: two poles, meaning it feeds its own recent output
   back in, and a bank of zeros, a weighted sum of the recent coded errors. Both ends adjust
   the weights as they go, from the codes alone.
3. **Quantise.** The error is measured against a step size and rounded to a code of two to
   seven bits. The step size is never transmitted. Both ends enlarge it after large codes and
   shrink it after small ones, so the quantiser follows the level of the signal on its own.
   That is backward adaptation, and it is why the format carries no side information.
4. **Pack.** The four codes go into one 16-bit word in sign-magnitude form, with the most bits
   going to the band that carries the most energy.

The four bands are not treated alike:

| Band | Frequency | Bits | Magnitudes | Predictor zeros | Step-size range |
|---|---|---|---|---|---|
| LF | 0-2 kHz | 7 | 64 | 24 | 11 octaves |
| MLF | 2-4 kHz | 4 | 8 | 12 | 13 octaves |
| MHF | 4-6 kHz | 2 | 2 | 6 | 14 octaves |
| HF | 6-8 kHz | 3 | 4 | 12 | 14 octaves |

One bit of every code is the sign, so a 7-bit LF code selects one of 64 magnitudes and a
2-bit MHF code selects one of two. Every band uses two poles alongside the zeros listed here,
and a band with more zeros predicts from a longer stretch of its own history: LF looks back
24 sub-band samples, MHF only 6.

The allocation is not monotonic: HF gets three bits where MHF gets two. On a PCC card the
codec then takes HF's low bit for its data channel, which leaves 7/4/2/2 for audio, and takes
MLF's low bit as well over part of each page. See [The stolen HF bit](#the-stolen-hf-bit).

### How the decoder reads one back

The decoder mirrors the encoder. It splits the word into four codes, multiplies each code's
reconstruction level by that band's current step size, adds the predictor's estimate to get
the sub-band sample, updates the step size and the predictor from the code exactly as the
encoder did, and runs the four band samples back through the QMF tree to produce four output
samples. The encoder decodes its own output as it goes, so both ends adapt from identical
numbers.

Nothing in that loop is checked against anything transmitted, so a decoder that starts in the
wrong state produces the wrong output until the state recovers. It does recover, because
every adaptation in the codec leaks: the step-size accumulator by a factor of 16310/16384 per
sub-band sample, the predictor's poles by 16320/16384 and 16256/16384, and its zeros by
255/256. Those are time constants of 130 to 260 sub-band samples, 30 to 65 ms at this rate,
so state inherited from the wrong place fades instead of persisting. Full recovery takes
longer than one time constant, because the predictor's coefficients have to converge as well
as its step size. A fragment decoded from a cold start comes within 1% of the same fragment
decoded in context after 300 to 500 ms.

### What the design means for a card

- **Nothing in the stream describes itself.** No header gives the sampling rate, the channel
  count, or the bit allocation, because the equipment at each end of a link is configured for
  them. All of it had to be recovered from the audio instead; the sample rate came from a
  test tone the card carries, in [Audio payload](#audio-payload).
- **Position converts to time.** The rate is a constant 8000 bytes per second, so a 256-byte
  page holds 128 words, 512 output samples and 32 ms of audio, and a fragment's length gives
  its duration exactly. `extract.py` reports durations from that arithmetic.
- **The stream can be cut and joined at any word.** Every word decodes the same way as every
  other, which is what lets the card hold 213 fragments and build 316 announcements out of
  them, and lets `extract.py` concatenate a fragment's pages, or a whole announcement's
  fragments, and decode the result in one pass. The card leans on this: announcement `0385` is
  a single 30-second fragment referenced ten times, which is how a five-minute test signal
  fits in 30 seconds of storage. A join costs the decoder its convergence time, so an
  announcement decoded as one stream, the way the player plays it, is the authentic rendering,
  and a fragment decoded on its own differs at the start whenever it normally follows another
  fragment.
- **The delay is fixed.** The reference implementation reports a codec delay of 122 samples,
  7.6 ms at 16 kHz, and `aptxdec` does not remove it. Allow for that offset when aligning a
  decode against a reference recording.

## Container

The card is a flat array of 256-byte pages. A page's first byte gives its type:

| Type | Meaning |
|---|---|
| `0x05` | Audio object |
| `0x06` | Announcement or metadata record |
| `0x07` | Directory |
| `0x0B` | Text |

Payload pages are the exception: they hold audio from the first byte, so they carry no type
byte and no page on the card starts with `0x04`. That value is an entry kind, used inside an
index to say that the page it points at is payload.

An object's index page holds 6-byte entries starting at offset 7. Bytes 4 and 5 chain to the
next index page. Each entry is:

```
[ type ][ page_lo ][ page_hi ][ 0x00 ][ crc_lo ][ crc_hi ]
```

The trailing 16-bit value is a CRC-16/XMODEM (poly `0x1021`, init `0x0000`, no reflection,
no final xor) over the page's full 256 bytes, stored little-endian. All 16,113 payload pages
on the reference card verify, so page extraction and ordering are byte-correct and the
payload is stored raw.

Within an object, payload page numbers advance monotonically, but they are often not
consecutive: 123 of the 213 audio objects have their payload split across separate runs, so
an object has to be read through its index rather than as one span.

Unused pages are erased to `0xFF` throughout. On the reference card, 48,075 of the 65,536
pages are fully erased, and live data extends to byte 4,470,016 of the 16 MiB image.

### Directories

Type `0x07` pages form the master index. There are 13 of them, starting at page 0 and chained
through bytes 4-5 in the same way as any other index. Between them they list every audio
object on the card as a type `0x05` child (213 on the reference card) and every announcement
record as a type `0x06` child (318 of the 320). Page 0 also carries one type `0x02` child
pointing at page 1 and one type `0x03` child pointing at page 2. Both of those are a single
payload page of silence that verifies against the CRC stored with it: every word of page 1 is
`0x0000` and every word of page 2 is `0x2000`, and both carry the reserved bit-7 pattern in
words 117 to 127. Page 2 is silence as the encoder writes it on this card, with the HF
auxiliary bit set; page 1 is the same stream without it. Decoding either gives digital
silence. What the player uses them for is unknown.

The directory is the only reliable enumeration of the audio objects. An object's index runs
onto further pages through the chain in bytes 4-5, and every one of those continuation pages
starts with `0x05` exactly as the object's first page does. Walking from a continuation page
yields the tail of that object's payload list, with entries and CRCs that all verify, so a
scan for pages that start with `0x05` returns each object once plus one entry for each of its
continuation pages, and counts the payload behind them more than once. The reference card
holds 213 objects across 482 index pages.

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
in tag `0x05` matches the actual number of payload pages for all 213 objects, and for all 742
fragment references, which gives a second integrity check independent of the CRCs.

Announcements reference their fragments through `0x0d 0x0a` records containing an 8-digit
address. That address points at a 6-byte directory entry, not at the audio object itself.

The reference card has 531 pages of this type: 320 announcement records and 213 object
metadata records, with two pages serving as both. Those two, `Mind Gap` and `Keep
Belongings`, carry a text reference and an id like an announcement and a payload page count
like object metadata, and they are the two announcement records the directory does not list
as a `0x06` child.

Four of the 320 announcement records reference no audio: those two, `RASTI Performance Test`,
and one record whose text is a row of `@` characters. That is why the tools report 320
announcements but 316 with audio. Of the 213 audio objects, 193 are referenced by an
announcement and 20 are reachable only through the directory.

## Audio payload

The payload is a stream of 16-bit words at 4000 words per second, decoding to 16 kHz mono at
64 kbit/s. Nothing in the stream says so, so the rate comes from a signal the card carries for
its own purposes: announcement `0386 Speaker Impedance Test` is a steady 997 Hz tone, a
standard audio test frequency. Decoded at 4000 words per second it measures 997.25 Hz, which
puts the rate within 0.03% of 16 kHz. Each word carries one code for each of the four QMF
sub-bands — a quantised prediction error rather than a sample — read big-endian and stored as
sign-magnitude:

| Field | Band | Bits | Width | Sign bit |
|---|---|---|---|---|
| LF | 0-2 kHz | 0-6 | 7 | 6 |
| MLF | 2-4 kHz | 7-10 | 4 | 10 |
| MHF | 4-6 kHz | 11-12 | 2 | 12 |
| HF | 6-8 kHz | 13-15 | 3 | 15 |

Within each field the most significant bit is the sign, set for negative, and the rest of the
field is the magnitude:

```
bit    15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
        s   m   a   s   m   s   m   m   m   s   m   m   m   m   m   m
       └── HF ──┘  └ MHF ┘  └─── MLF ────┘  └────────── LF ──────────┘
```

`s` marks a sign bit, `m` a magnitude bit, and `a` the bit the auxiliary data channel takes
from HF on this card. For the coding these fields come out of, see
[What APT-X100 is](#what-apt-x100-is).

The reference implementation confirms this exactly: it demultiplexes with `aptxVal & 0x7F`,
`(aptxVal >> 7) & 0xF`, `(aptxVal >> 11) & 3` and `(aptxVal >> 13) & 7`. Reading the words
little-endian, or the fields as two's complement, breaks all four bands.

### The stolen HF bit

This is the detail that is easiest to miss.

APT-X100 can carry a low-rate auxiliary data channel, and it does so by taking the low bit
of a sub-band away from the audio. On a PCC card the HF field's low bit, word bit 13, is
taken. That is why it is set in every word, and why the HF field only ever holds odd values.
Tell the decoder, so that it reads HF as 2 bits rather than 3.

`chmode 1` does that. Without it, those bits decode as audio: a spoken announcement clips
around 2.7% of its samples and puts 78% of its energy in 4-6 kHz, which no speech recording
does. Corrected, it puts 98% of its energy in 0-2 kHz and clips almost nothing, at most 0.01%
of samples.

Bit 13 carries no recoverable payload; it is always 1. Clearing it across a whole clip
changes the decoded output not at all, because a correctly configured decoder never reads it.

### Bit 7 is audio, except in the aux window

Word bit 7 is MLF's low bit, and for most of a page it carries audio rather than data.
Corrupting it measurably changes the decode, where corrupting bit 13 does nothing. Each
figure below is the ratio of an unmodified decode of announcement `000F` to the difference
the change makes to it, so a smaller number means a larger disturbance:

| Change | Signal-to-difference ratio |
|---|---|
| Clear bit 13 everywhere | identical output |
| Clear bit 7 in words 0-116 | 18.5 dB |
| Set bit 7 in words 0-116 | −3.1 dB |
| Clear bit 0 (LF's low bit) everywhere | 10.0 dB |

Setting bit 7 does more damage than clearing it, and more than clearing LF's low bit in every
word of the stream.

The last 11 words of every 128-word page are the exception. In words 117 to 127, bit 7 is
fixed to the pattern `1 1 0 1 1 1 0 1 1 1 1`, identical on all 16,113 payload pages on the
reference card. A page is 256 bytes, which is exactly 128 words, so the window lines up with
the page boundary.

Two pages outside the audio entirely carry the same pattern: the directory's `0x02` and
`0x03` children are single payload pages of silence, and both hold it in words 117 to 127.

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

The uncorrected decode has a measurable page-locked defect, and one of the card's own test
signals shows it. Announcement `0387 MLSSA Performance Test` is 60 seconds of noise-like
signal whose character does not change from one moment to the next, which is what makes it
useful here: nothing in it is synchronous with a 256-byte page. Fold it at the 512-sample page
period, averaging every sample that falls at the same offset into a page, and a clean decode
gives a flat profile. The stock decode is not flat, and the defect is not confined to the
window: energy spikes immediately after each one and decays across the page until the next,
which is a sawtooth at the page rate of 31.25 Hz.

Each row below folds announcement `0387` into a 512-sample profile of mean square by phase.
Peak-to-trough is the ratio of the highest phase to the lowest over the unsmoothed profile.
Ripple is the 31.25 Hz
component of that profile peak-to-peak, as a percentage of its mean. Window vs rest compares
the mean square of the 44 samples the window produces against the other 468.

| Decode | Peak-to-trough | Ripple at 31.25 Hz | Window vs rest |
|---|---|---|---|
| Stock, bit 7 read as audio | +2.22 dB | 6.50% | 0.923 |
| `auxwin 1`, words 117-127 | +1.15 dB | 0.24% | 1.006 |
| `auxwin 2`, words 117-126 | +1.41 dB | 0.70% | 0.997 |

The correction raises the window's energy toward the page mean rather than lowering it, so
the flattening is not an artefact of attenuating the window.

`auxwin 1` is the better reading. `bitcorr_ch1` covers 11 words while the index fills only
10, so word 127 is reserved but carries no index bit. Its low bit is nevertheless 1 on all
16,113 pages, which no audio bit would be, and correcting it measures flatter.

On a spoken announcement the change sits about 25 dB below the signal, and 90 to 94% of it
falls in MLF, which is where a defect in that band's least significant bit belongs.

Beyond the reserved window, bit 7 carries no data channel. Across a fragment those values
look random. P(1) is 0.495, no bit position is constant across pages, and the packed bytes
hold no ASCII or other structure.

### Codec structure

[What APT-X100 is](#what-apt-x100-is) covers the shape of the codec: four-band sub-band ADPCM
at 4:1, a backward prediction loop per band, and a backward adaptive quantiser whose step size
follows the magnitude of recent codes.

A few notes on the tables that drive it, under the names the reference implementation gives
them:

- `QTZ_DATA_1` holds the decoder's `{reconstruction level, step increment}` pairs, sized 2, 4,
  8 and 64 magnitudes for the four bands. The reconstruction level is scaled by the current
  step size to give the sub-band error; the increment is what that code adds to the step-size
  accumulator, negative for small codes and positive for large ones. LF's increments run
  −15, −15, −7 … 0 … +1, +3, +4 … +405, +512.
- The step-size accumulator is a base-2 logarithm with 256 counts to the doubling: its top
  byte selects a power of two, and the next five bits index the 32-entry mantissa table
  `ENC_IDX_1000F4EC`. Between increments it leaks by 16310/16384, and it is clamped to 2816
  for LF, 3328 for MLF and 3584 for the other two. Those clamps are the 11 to 14 octaves of
  step-size range listed under
  [How the encoder builds a word](#how-the-encoder-builds-a-word).
- `QTZ_DATA_2` holds the encoder's decision thresholds, the same four sizes. To pick a code,
  the encoder binary-searches the table, comparing the error against each threshold scaled by
  the current step size.
- The LF threshold table contains `… 8628, 8992, 9683, 9752, 10150 …`. Thresholds through that
  range step by about 370, but the interval between magnitudes 36 and 37 is 69 wide and the
  one before it is 691: the entry 9683 sits about 320 higher than the sequence calls for,
  which widens magnitude 36's decision region and leaves almost nothing for 37. The data
  shows the consequence. Counting LF codes across all 16,113 payload pages gives 22,635 for
  magnitude 35, 36,428 for 36, 3,228 for 37 and 17,230 for 38. Interpolating between 35 and
  38 puts 37 at about 18,900, so it is depleted 5.8 times over, equally in both sign
  polarities, and 36 carries an excess of 15,761 against a deficit of 15,643 on 37: the
  missing mass is on the neighbour the widened region took it from. The matching
  reconstruction levels are smooth through the same range (`… 8808, 9176, 9556, 9948 …`), so
  it looks like an error in one entry of the original table, preserved by every conforming
  encoder.

The patent (EP 0398973B1, priority GB8803390, 1988) specifies a different bit allocation of
8:4:2:2 for a 24 kHz-bandwidth variant. That allocation does not fit this stream. The
reference implementation demultiplexes 7/4/2/3, and the card agrees: the auxiliary channel's
fixed pattern sits at bit 7, which is MLF's low bit under 7/4/2/3 but LF's sign bit under
8:4:2:2. An auxiliary channel takes a low bit, not a sign bit, and reading bit 7 as
MLF's low bit is what flattens the page-rate ripple measured in
[Bit 7 is audio, except in the aux window](#bit-7-is-audio-except-in-the-aux-window).

## Decoding

Use `aptx100/`, which wraps a working APT-X100 implementation. A PCC payload needs the
decoder run big-endian and mono, with the band-3 bit correction enabled and the band-1
correction applied over the aux window:

```sh
aptxdec payload.bin out.wav 1 1 1 -1 1
```

The arguments are `msb`, `channels`, `chmode`, `buffers` and `auxwin`; see
[`aptx100/README.md`](../aptx100/README.md) for what each one does. `extract.py` runs this
command for every fragment and announcement on the card.
