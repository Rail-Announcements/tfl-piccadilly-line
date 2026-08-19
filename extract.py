"""Extract everything from a KeTech PCC card: audio, text, and metadata.

Audio is decoded with the reference APT-X100 implementation (see docs/audio-format.md),
so the output contains no fitted parameters and no dynamics processing.

Usage:
    python3 extract.py "copy file.pcc" --out extracted --decoder ./aptxdec
"""

import argparse
import json
import os
import re
import subprocess
import sys

import pcc

TAG_ID = 0x01
TAG_ALT_ID = 0x02
TAG_COUNT = 0x05
TAG_RECORD = 0x06
TAG_ADDRESS = 0x0A
TAG_FLAG = 0x0B
TAG_SMALL = 0x0C
TAG_TEXT_REF = 0x0E

TAG_NAMES = {
    TAG_ID: "id",
    TAG_ALT_ID: "alt_id",
    TAG_COUNT: "payload_pages",
    TAG_RECORD: "record",
    TAG_ADDRESS: "address",
    TAG_FLAG: "flag",
    TAG_SMALL: "small",
    TAG_TEXT_REF: "text_ref",
}

TAG_RE = re.compile(rb"([\x01-\x1f])([\x20-\x7e]{1,60})\x00")
FRAGMENT_RE = re.compile(rb"\x0d\x0a([0-9A-F]{8})\x00")


def crc16_xmodem(data):
    """CRC-16/XMODEM, the checksum stored in each index entry."""
    reg = 0
    for b in data:
        reg ^= b << 8
        for _ in range(8):
            reg = ((reg << 1) ^ 0x1021) & 0xFFFF if reg & 0x8000 else (reg << 1) & 0xFFFF
    return reg


def index_entries(card, first_page):
    """Yield (kind, page, crc) for every child of an object, following the page chain."""
    visited = set()
    number = first_page
    # A zero link ends the chain, so the test comes after the page is read: page 0 is a
    # valid start, and holds the master directory.
    while number not in visited:
        visited.add(number)
        page = card.page(number)
        for offset in range(7, pcc.PAGE - 5, 6):
            kind = page[offset]
            if kind in (0x00, 0xFF):
                continue
            yield kind, page[offset + 1] | (page[offset + 2] << 8), \
                page[offset + 4] | (page[offset + 5] << 8)
        number = page[4] | (page[5] << 8)
        if not number:
            break


def tags_of(page):
    """Parse the tag-based ASCII record in a type 0x06 page."""
    out = {}
    for match in TAG_RE.finditer(bytes(page)):
        name = TAG_NAMES.get(match.group(1)[0], f"tag_{match.group(1)[0]:02x}")
        out.setdefault(name, []).append(match.group(2).decode("latin-1"))
    return out


def audio_objects(card):
    """Map each audio object page to its payload pages, CRC status and metadata.

    Objects come from the master directory. Scanning every page for one that starts with
    TYPE_AUDIO finds the continuation pages of each object's index chain as well, and those
    walk as a suffix of the object they belong to rather than as objects of their own.
    """
    out = {}
    for number in card.audio_pages():
        payload, meta, bad = [], {}, 0
        for kind, page, crc in index_entries(card, number):
            if kind == pcc.TYPE_PAYLOAD:
                payload.append(page)
                if crc16_xmodem(card.page(page)) != crc:
                    bad += 1
            elif kind == pcc.TYPE_ANNOUNCEMENT:
                meta = tags_of(card.page(page))
        if payload:
            out[number] = dict(payload=payload, crc_failures=bad, meta=meta)
    return out


def text_objects(card):
    out = {}
    for number in range(len(card.data) // pcc.PAGE):
        page = card.page(number)
        if page[0] == pcc.TYPE_TEXT and page[7] == pcc.TEXT_TAG:
            out[number] = page[9:9 + page[8]].decode("latin-1")
    return out


def announcements(card, texts):
    """Every announcement record, with its text and the audio objects it references."""
    out = []
    for number in range(len(card.data) // pcc.PAGE):
        page = card.page(number)
        if page[0] != pcc.TYPE_ANNOUNCEMENT:
            continue
        tags = tags_of(page)
        ids = tags.get("id", [])
        refs = tags.get("text_ref", [])
        if not ids or not refs:
            continue
        text = None
        for ref in refs:
            if ref.startswith("A"):
                text = texts.get(int(ref[1:], 16) // pcc.PAGE)
                if text:
                    break
        if text is None:
            continue
        fragments = []
        for ref in FRAGMENT_RE.findall(bytes(page)):
            address = int(ref, 16)
            if card.data[address] == pcc.TYPE_AUDIO:
                fragments.append(card.data[address + 1] | (card.data[address + 2] << 8))
        out.append(dict(id=ids[0], page=number, text=text, fragments=fragments, tags=tags))
    return out


def safe(text):
    keep = "".join(c if c.isalnum() or c in " -_" else "" for c in text)
    return "_".join(keep.split())[:70] or "untitled"


def decode(decoder, payload, wav):
    raw = wav + ".bin"
    with open(raw, "wb") as handle:
        handle.write(payload)
    # msb 1 (big-endian words), 1 channel, chmode 1 (band-3 aux bit correction), no aux
    # buffer, auxwin 1 (band-1 correction over the 11 words the encoder reserves per page)
    result = subprocess.run([decoder, raw, wav, "1", "1", "1", "-1", "1"],
                            capture_output=True)
    os.remove(raw)
    return result.returncode == 0 and os.path.exists(wav)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("card")
    parser.add_argument("--out", default="extracted")
    parser.add_argument("--decoder", default=os.path.join(os.path.dirname(
                            os.path.abspath(__file__)), "aptxdec"),
                        help="path to the APT-X100 decoder binary (build it with "
                             "`make -C aptx100`)")
    parser.add_argument("--no-audio", action="store_true", help="metadata only")
    args = parser.parse_args(argv)

    if not args.no_audio and not os.path.exists(args.decoder):
        parser.error(f"decoder not found at {args.decoder}; run `make -C aptx100` first")

    card = pcc.load(args.card)
    texts = text_objects(card)
    objects = audio_objects(card)
    entries = announcements(card, texts)

    live = max(i for i, b in enumerate(card.data) if b != 0xFF) + 1
    total_pages = sum(len(o["payload"]) for o in objects.values())
    bad = sum(o["crc_failures"] for o in objects.values())
    declared = [(len(o["payload"]), int(o["meta"]["payload_pages"][0], 16))
                for o in objects.values() if o["meta"].get("payload_pages")]
    agree = sum(1 for actual, claimed in declared if actual == claimed)
    referenced = {f for e in entries for f in e["fragments"]}
    with_audio = sum(1 for e in entries if e["fragments"])
    print(f"card           {len(card.data)} bytes, live to {live}")
    print(f"text objects   {len(texts)}")
    print(f"audio objects  {len(objects)} ({total_pages} payload pages, "
          f"{len(referenced)} referenced)")
    print(f"CRC failures   {bad}")
    print(f"page counts    {agree}/{len(declared)} match their declared value")
    print(f"announcements  {len(entries)} ({with_audio} with audio)")

    for sub in ("announcements", "fragments"):
        os.makedirs(os.path.join(args.out, sub), exist_ok=True)

    with open(os.path.join(args.out, "text.txt"), "w") as handle:
        for page in sorted(texts):
            handle.write(f"{page:6d}  {texts[page]}\n")

    written, decoded = {}, 0
    if not args.no_audio:
        for number, obj in sorted(objects.items()):
            name = f"frag_{number:05d}"
            label = obj["meta"].get("id", [None])[0]
            if label:
                name += f"_{label}"
            path = os.path.join(args.out, "fragments", name + ".wav")
            payload = b"".join(card.page(p) for p in obj["payload"])
            if decode(args.decoder, payload, path):
                written[number] = os.path.basename(path)

    manifest = []
    for entry in entries:
        payload = b"".join(
            b"".join(card.page(p) for p in objects[f]["payload"])
            for f in entry["fragments"] if f in objects)
        name = f"{entry['id']}_{safe(entry['text'])}.wav"
        if payload and not args.no_audio:
            if decode(args.decoder, payload, os.path.join(args.out, "announcements", name)):
                decoded += 1
        manifest.append(dict(
            id=entry["id"],
            text=entry["text"],
            record_page=entry["page"],
            audio=name if payload else None,
            seconds=round(len(payload) / 2 / 4000, 3) if payload else 0.0,
            fragments=[dict(object_page=f,
                            file=written.get(f),
                            payload_pages=len(objects[f]["payload"]) if f in objects else 0,
                            declared_pages=int(objects[f]["meta"].get("payload_pages", ["0"])[0], 16)
                            if f in objects and objects[f]["meta"].get("payload_pages") else None)
                       for f in entry["fragments"]],
            tags={k: v for k, v in entry["tags"].items() if k != "record"},
        ))

    with open(os.path.join(args.out, "manifest.json"), "w") as handle:
        json.dump(manifest, handle, indent=1)

    with open(os.path.join(args.out, "index.txt"), "w") as handle:
        for m in sorted(manifest, key=lambda m: m["id"]):
            handle.write(f"{m['id']}  {m['seconds']:7.2f}s  {len(m['fragments'])} frag  {m['text']}\n")

    print(f"\nwritten to {args.out}/")
    print(f"  announcements/  {decoded} wav files")
    print(f"  fragments/      {len(written)} wav files")
    print(f"  manifest.json, index.txt, text.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main())
