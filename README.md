# TfL Piccadilly line announcement card

Tools for reading a KeTech PCC card, the announcement store used by the digital voice
announcer on London Underground 1973 Tube Stock.

Nothing about the format is published, so both the container and the audio codec had to be
worked out from the card. `extract.py` recovers the announcements, the audio fragments, and
the text and metadata, and checks the result against checksums stored on the card itself.

## What is on the card

| | |
|---|---|
| Card image | 16 MiB, live data to 4.26 MiB |
| Announcements | 320, totalling 27.1 minutes |
| Audio fragments | 482, shared between announcements |
| Text strings | 320 |
| Audio format | APT-X100, 64 kbit/s, 16 kHz mono |

Announcements are assembled from shared fragments, and 280 of the 320 use more than one.
That is how the system builds a phrase like "Heathrow Terminal 4 station is closed. This
train will terminate at Heathrow Terminals 1, 2 and 3" out of stock pieces.

## Usage

Build the decoder once:

```sh
make -C aptx100
```

Then extract everything:

```sh
python3 extract.py "copy file.pcc" --out extracted
```

This writes:

```
extracted/
  announcements/   one WAV per announcement, fragments joined in order
  fragments/       one WAV per unique audio fragment
  manifest.json    ids, text, fragment composition, tags, durations
  index.txt        one line per announcement
  text.txt         every text object on the card
```

The output is committed to the repository rather than rebuilt on demand, because the card it
came from won't last forever.

Add `--no-audio` for metadata only. That takes about a second and needs no decoder.

To inspect the container without extracting anything:

```sh
python3 pcc.py list "copy file.pcc"
python3 pcc.py info "copy file.pcc"
```

## Integrity

Every payload page carries a CRC-16/XMODEM in its index entry, and every audio fragment
declares its own page count in its metadata record. `extract.py` checks both:

```
CRC failures   0        (of 48,756 payload pages)
page counts    742/742  match their declared value
```

Both checks cover page extraction and ordering, so a clean run means the bytes handed to the
decoder are the bytes on the card.

## How it works

`pcc.py` reads the container. The card is an array of 256-byte pages. Objects are index
pages holding 6-byte child entries, chained through bytes 4-5, and announcement records are
tag-based ASCII giving a message id, a text pointer, and the fragments to play.

`aptx100/` holds the audio decoder. It is third-party LGPL source, not original work; see
[`aptx100/README.md`](aptx100/README.md) for where it came from and how it is licensed.

`docs/audio-format.md` covers the payload format: the bit layout, how it was determined, the
auxiliary data channel, and the ways a decode can go wrong.

### The two settings to get right

APT-X100 can take the low bit of a sub-band away from the audio and use it for a low-rate
auxiliary data channel. A PCC card uses both of the bands it offers.

The HF band gives up its low bit for the whole stream, so word bit 13 is set in every word
and is not audio. Run the decoder with the band-3 bit correction enabled. Without it, those
bits decode as audio: about 2.9% of samples clip, and roughly 76% of the output energy lands
in 4-6 kHz, which no speech recording does.

The MLF band gives up its low bit over the last 11 words of every 128-word page, where the
encoder writes a constant identifying its configuration. Left uncorrected this puts a 1.6 dB
sawtooth at the page rate of 31.25 Hz through the decode. `extract.py` corrects both.

## Repository layout

```
extract.py            full extraction: audio, text, metadata, integrity checks
pcc.py                container reader and a small CLI
aptx100/              APT-X100 decoder (third-party, LGPL) plus a build harness
docs/audio-format.md  payload format documentation
extracted/            extraction output: audio, manifest, index and text listing
"copy file.pcc"       the card image
"PL audio files.pdf"  KeTech functional specification for the Night Tube DVA
```

Only `aptxdec` and the usual Python and macOS clutter are ignored.
