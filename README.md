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
| Announcements | 320 records, 316 with audio, totalling 27.1 minutes |
| Audio fragments | 213, of which 193 are used by an announcement |
| Text strings | 320 |
| Audio format | APT-X100, 64 kbit/s, 16 kHz mono |

APT-X100 is a fixed 4:1 audio codec from the late 1980s, built for broadcast links and
playback hardware. It splits the signal into four frequency bands, codes each band as the
error against a running prediction, and packs the four results into a single 16-bit word for
every four input samples. It has no headers and no frames, which is what lets the card store
each piece of speech once and splice the pieces into announcements. For an introduction to
the codec and the payload it produces, see
[What APT-X100 is](docs/audio-format.md#what-apt-x100-is).

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
  fragments/       one WAV per audio fragment, named frag_<index page>_<id>
  manifest.json    ids, text, fragment composition, tags, durations
  index.txt        one line per announcement
  text.txt         every text object on the card
```

Each fragment WAV is decoded on its own, so a fragment that normally follows another differs
from its in-context rendering for the first few hundred milliseconds, while the decoder's
state converges. The announcement WAVs are decoded as one stream, the way the player plays
them, and are the rendering to trust.

The output is committed to the repository rather than rebuilt on demand, because the card it
came from won't last forever.

Add `--no-audio` for metadata only. That takes about a second and needs no decoder.

To inspect the container without extracting anything:

```sh
python3 pcc.py list "copy file.pcc"
python3 pcc.py info "copy file.pcc"
```

`pcc.py extract` writes the raw APT-X100 payloads and a container-level manifest instead,
without decoding anything.

## Integrity

Every payload page carries a CRC-16/XMODEM in its index entry, and every audio fragment
declares its own page count in its metadata record. `extract.py` checks both and reports
what it found:

```
audio objects  213 (16113 payload pages, 193 referenced)
CRC failures   0
page counts    213/213 match their declared value
```

Both checks cover page extraction and ordering, so a clean run means the bytes handed to the
decoder are the bytes on the card.

## How it works

`pcc.py` reads the container. The card is an array of 256-byte pages. Objects are index
pages holding 6-byte child entries, chained through bytes 4-5, and announcement records are
tag-based ASCII giving a message id, a text pointer, and the fragments to play.

Audio objects are enumerated from the master directory at page 0. An object's index continues
onto further pages that carry the same type byte as its first, so a scan for pages that look
like an object returns those continuation pages too. See
[Directories](docs/audio-format.md#directories).

`aptx100/` holds the audio decoder. It is third-party LGPL source, not original work; see
[`aptx100/README.md`](aptx100/README.md) for where it came from and how it is licensed.

`docs/audio-format.md` covers the payload format: the bit layout, how it was determined, the
auxiliary data channel, and the ways a decode can go wrong.

### The two settings to get right

APT-X100 can take the low bit of a sub-band away from the audio and use it for a low-rate
auxiliary data channel. A PCC card uses both of the bands it offers.

The HF band gives up its low bit for the whole stream, so word bit 13 is set in every word
and is not audio. Run the decoder with the band-3 bit correction enabled. Without it, those
bits decode as audio: a typical spoken announcement clips 2.7% of its samples and puts 78% of
its energy in 4-6 kHz, which no speech recording does.

The MLF band gives up its low bit over the last 11 words of every 128-word page, where the
encoder writes a constant identifying its configuration. Left uncorrected this puts a 2.2 dB
sawtooth at the page rate of 31.25 Hz through the decode. `extract.py` corrects both.
