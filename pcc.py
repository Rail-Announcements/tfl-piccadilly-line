"""Reader for KeTech PCC passenger information cards.

Usage:
    python3 pcc.py list "copy file.pcc"
    python3 pcc.py extract "copy file.pcc" --out clips
    python3 pcc.py info "copy file.pcc"
"""

import argparse
import json
import os
import re
import sys

PAGE = 0x100

TYPE_PAYLOAD = 0x04
TYPE_AUDIO = 0x05
TYPE_ANNOUNCEMENT = 0x06
TYPE_DIRECTORY = 0x07
TYPE_TEXT = 0x0B

TEXT_TAG = 0x41

# Field tags inside a type 0x06 announcement record. Each value is NUL-terminated ASCII.
TAG_MESSAGE_ID = re.compile(rb"\x01([0-9A-F]{4})\x00")
TAG_TEXT_ADDRESS = re.compile(rb"\x0eA([0-9A-F]{8})\x00")
TAG_FRAGMENT_REF = re.compile(rb"\x0d\x0a([0-9A-F]{8})\x00")


class Card:
    def __init__(self, data):
        self.data = data

    def page(self, number):
        start = number * PAGE
        return self.data[start:start + PAGE]

    def entries(self, first_page):
        """Walk an object's page chain and yield its (type, page) entries in order."""
        visited = set()
        number = first_page
        # A zero link ends the chain, so the test comes after the page is read: the master
        # directory itself starts at page 0.
        while number not in visited:
            visited.add(number)
            page = self.page(number)
            for offset in range(7, PAGE - 5, 6):
                kind = page[offset]
                if kind in (0x00, 0xFF):
                    continue
                yield kind, page[offset + 1] | (page[offset + 2] << 8)
            number = page[4] | (page[5] << 8)
            if not number:
                break

    def index_pages(self, first_page):
        visited = []
        number = first_page
        while number not in visited:
            visited.append(number)
            number = self.page(number)[4] | (self.page(number)[5] << 8)
            if not number:
                break
        return visited

    def directory(self):
        """Yield the (type, page) entries of the master index."""
        root = next((n for n in range(len(self.data) // PAGE)
                     if self.page(n)[0] == TYPE_DIRECTORY), None)
        if root is None:
            return
        yield from self.entries(root)

    def audio_pages(self):
        """Return every audio object page, in directory order.

        Scanning for pages that start with TYPE_AUDIO finds more than this: an object's index
        chain continues onto further pages, and each of those starts with TYPE_AUDIO too.
        Walking one of them yields a suffix of the object it belongs to, not a new object.
        """
        return [page for kind, page in self.directory() if kind == TYPE_AUDIO]

    def fragment(self, audio_page):
        """Return the payload bytes of a type 0x05 audio object."""
        pages = [p for kind, p in self.entries(audio_page) if kind == TYPE_PAYLOAD]
        return b"".join(self.page(p) for p in pages), pages

    def text_of(self, page_number):
        page = self.page(page_number)
        if page[0] != TYPE_TEXT or page[7] != TEXT_TAG:
            return None
        return page[9:9 + page[8]].decode("latin-1")

    def announcements(self):
        """Yield every announcement as a dict of message text, id, and fragments."""
        for number in range(len(self.data) // PAGE):
            page = self.page(number)
            if page[0] != TYPE_ANNOUNCEMENT:
                continue
            text_match = TAG_TEXT_ADDRESS.search(page)
            id_match = TAG_MESSAGE_ID.search(page)
            if not text_match or not id_match:
                continue
            text = self.text_of(int(text_match.group(1), 16) // PAGE)
            if text is None:
                continue
            fragments = []
            for ref in TAG_FRAGMENT_REF.findall(page):
                address = int(ref, 16)
                # A reference points at a 6-byte directory entry, not at the audio object.
                if self.data[address] != TYPE_AUDIO:
                    continue
                fragments.append(self.data[address + 1] | (self.data[address + 2] << 8))
            if fragments:
                yield {
                    "id": id_match.group(1).decode(),
                    "page": number,
                    "text": text,
                    "fragments": fragments,
                }


def load(path):
    with open(path, "rb") as handle:
        return Card(handle.read())


def sanitize(text):
    return re.sub(r"[^A-Za-z0-9]+", "_", text)[:64].strip("_") or "untitled"


def cmd_list(args):
    card = load(args.card)
    for entry in card.announcements():
        sizes = [len(card.fragment(p)[0]) for p in entry["fragments"]]
        print(f"{entry['id']}  {sum(sizes):8d}B  {len(sizes)} frag  {entry['text']}")


def cmd_extract(args):
    card = load(args.card)
    clips = os.path.join(args.out, "clips")
    frags = os.path.join(args.out, "fragments")
    os.makedirs(clips, exist_ok=True)
    os.makedirs(frags, exist_ok=True)

    manifest = []
    written = {}
    for entry in card.announcements():
        payload = b""
        details = []
        for page_number in entry["fragments"]:
            data, pages = card.fragment(page_number)
            name = f"frag_{page_number:05d}.bin"
            if name not in written:
                with open(os.path.join(frags, name), "wb") as handle:
                    handle.write(data)
                written[name] = len(data)
            details.append({
                "file": name,
                "object_page": page_number,
                "payload_pages": len(pages),
                "index_pages": len(card.index_pages(page_number)),
                "contiguous": pages == list(range(pages[0], pages[0] + len(pages))),
                "bytes": len(data),
            })
            payload += data
        name = f"{entry['id']}_{sanitize(entry['text'])}.bin"
        with open(os.path.join(clips, name), "wb") as handle:
            handle.write(payload)
        manifest.append({
            "file": name,
            "id": entry["id"],
            "text": entry["text"],
            "bytes": len(payload),
            "fragments": details,
        })

    with open(os.path.join(args.out, "manifest.json"), "w") as handle:
        json.dump(manifest, handle, indent=1)
    print(f"{len(manifest)} announcements and {len(written)} fragments written to {args.out}")


def cmd_info(args):
    card = load(args.card)
    live = max(i for i, b in enumerate(card.data) if b != 0xFF) + 1
    entries = list(card.announcements())
    objects = card.audio_pages()
    referenced = {p for e in entries for p in e["fragments"]}
    sizes = sorted(len(card.fragment(p)[0]) for p in objects)
    scattered = sum(
        0 if (pages := card.fragment(p)[1]) == list(range(pages[0], pages[0] + len(pages))) else 1
        for p in objects
    )
    print(f"card size        {len(card.data)} bytes")
    print(f"live data        {live} bytes (rest is erased 0xFF)")
    print(f"announcements    {len(entries)} with audio")
    print(f"fragments        {len(objects)} ({len(referenced)} referenced by an announcement)")
    print(f"  scattered      {scattered} have non-contiguous payload pages")
    print(f"  bytes          min {sizes[0]}, median {sizes[len(sizes) // 2]}, max {sizes[-1]}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("list", help="print every announcement and its payload size")
    p.add_argument("card")
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("extract", help="write clips, fragments, and a manifest")
    p.add_argument("card")
    p.add_argument("--out", default="extracted")
    p.set_defaults(func=cmd_extract)

    p = sub.add_parser("info", help="summarize the card layout")
    p.add_argument("card")
    p.set_defaults(func=cmd_info)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
