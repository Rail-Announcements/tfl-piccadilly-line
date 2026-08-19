// Decode a PCC audio payload with the reference APT-X100 implementation.
//
// Usage: aptxdec <payload.bin> <out.wav> [msb] [channels] [chmode] [buffers]
//   msb      1 to byte-swap each aptX word before decoding (big-endian stream)
//   channels 1 for mono, 2 to treat the stream as interleaved stereo
//   chmode   1 to enable the band-3 bit correction (a PCC card needs this)
//   buffers  -1 for no auxiliary data buffer
//   auxwin   -1 stock decode (default); 0 same, via the manual loop; 1 or 2 apply the
//            band-1 correction over words 117-127 or 117-126 of each 128-word page
//
// A PCC payload needs 1 1 1 -1. See ../docs/audio-format.md.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "libaptx100/aptx100/aptx100.h"

static void write_wav(const char* path, const short* data, size_t n, int rate, int ch) {
  FILE* f = fopen(path, "wb");
  unsigned int datasz = (unsigned int)(n * sizeof(short));
  unsigned int riffsz = 36 + datasz;
  unsigned short bits = 16, chans = (unsigned short)ch;
  unsigned int byterate = rate * ch * 2;
  unsigned short align = (unsigned short)(ch * 2);
  unsigned int fmtsz = 16;
  unsigned short fmt = 1;
  fwrite("RIFF", 1, 4, f); fwrite(&riffsz, 4, 1, f); fwrite("WAVE", 1, 4, f);
  fwrite("fmt ", 1, 4, f); fwrite(&fmtsz, 4, 1, f); fwrite(&fmt, 2, 1, f);
  fwrite(&chans, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&byterate, 4, 1, f);
  fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&datasz, 4, 1, f);
  fwrite(data, 1, datasz, f);
  fclose(f);
}

// Mono replica of the per-word loop in aptxDec, so bitcorr_ch1 can be driven directly.
// The card reserves band 1's low bit over the tail of each page, but its aux index sits one
// word earlier than this implementation's parser expects, so `buffers 0` cannot reach it.
// Faithful only for buffers -1, where aptxDec's v22/v18 are both 0.
static void decode_manual(aptxCtx_t* ctx, size_t frames, short* pcm,
                          const unsigned short* words, bool msb, int auxwin) {
  int pcm4[4];
  int bitcorr_ch3 = ctx->aptxChannel[0].mode == 1 ? 1 : 0;
  if (ctx->mode & APTX_MODE::USE_MMX) {
    fprintf(stderr, "auxwin: MMX path not replicated; results would not be comparable\n");
    exit(1);
  }
  for (size_t n = 0; n < frames; ++n) {
    size_t w = n % 128;
    int bitcorr_ch1 = 0;
    if (auxwin > 0) {
      bitcorr_ch1 = (w >= 117 && w <= (auxwin == 1 ? 127u : 126u)) ? 1 : 0;
    }
    unsigned short v = words[n];
    if (msb) { v = (unsigned short)((v << 8) | (v >> 8)); }
    std_aptxChannelDecode(&ctx->aptxChannel[0], pcm4, v, bitcorr_ch1, bitcorr_ch3);
    for (int i = 0; i < 4; ++i) { pcm[4 * n + i] = (short)pcm4[i]; }
  }
}

int main(int argc, char** argv) {
  if (argc < 3) { fprintf(stderr, "usage: %s in.bin out.wav [msb] [channels] [chmode] [buffers] [auxwin]\n", argv[0]); return 1; }
  bool msb = argc > 3 ? atoi(argv[3]) != 0 : true;
  int channels = argc > 4 ? atoi(argv[4]) : 1;
  // chmode 1 forces the band-3 bit correction. A PCC card puts auxiliary data in band 3,
  // so its low bit is not audio and decoding it as audio clips the output.
  int chmode = argc > 5 ? atoi(argv[5]) : 0;
  int buffers = argc > 6 ? atoi(argv[6]) : -1;
  int auxwin = argc > 7 ? atoi(argv[7]) : -1;

  FILE* f = fopen(argv[1], "rb");
  if (!f) { perror("open"); return 1; }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  std::vector<unsigned short> words(sz / 2);
  fread(words.data(), 2, words.size(), f);
  fclose(f);

  size_t frames = words.size() / channels;     // aptX words per channel
  size_t samples = frames * 4;                 // PCM samples per channel
  std::vector<short> pcm(samples * channels, 0);

  int modes[2] = { chmode, chmode };
  aptxCtx_t* ctx = aptxCreate(APTX_MODE::DECODE, buffers, chmode ? modes : nullptr, channels);
  if (!ctx) { fprintf(stderr, "aptxCreate failed\n"); return 1; }

  int rc = 0;
  if (auxwin < 0) {
    rc = aptxDecode(ctx, 0, (int)samples, false, pcm.data(), msb, words.data());
  } else if (channels != 1) {
    fprintf(stderr, "auxwin modes are mono only\n"); return 1;
  } else {
    decode_manual(ctx, frames, pcm.data(), words.data(), msb, auxwin);
  }
  fprintf(stderr, "aptxDecode rc=%d words=%zu samples=%zu ch=%d msb=%d chmode=%d buffers=%d auxwin=%d\n",
          rc, words.size(), samples, channels, (int)msb, chmode, buffers, auxwin);

  long long acc = 0; short peak = 0;
  for (size_t i = 0; i < pcm.size(); ++i) {
    int v = pcm[i]; acc += (long long)v * v;
    short a = (short)(v < 0 ? -v : v);
    if (a > peak) peak = a;
  }
  fprintf(stderr, "peak=%d rms=%.1f\n", peak, pcm.empty() ? 0.0 : (double)acc / pcm.size());

  write_wav(argv[2], pcm.data(), pcm.size(), 16000, channels);
  aptxDelete(ctx);
  return 0;
}
