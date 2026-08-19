#include "defs.h"
#include "aptx100.h"
#include "libaptx100.h"

auto pcmClipValue = [](auto v) {
  if (v >= -32768) {
    if (v > 32767) {
      v = 32767;
    }
  }
  else {
    v = -32768;
  }
  return v;
};

bool aptxAutoAux(aptxCtx_t* aptxCtx, bool mode_bit_3) {
  if (mode_bit_3) {
    aptxCtx->mode |= APTX_MODE::USE_AUTO_AUX;
    if (aptxCtx->m_08 < 3) {
      ++aptxCtx->m_08;
    }
  }
  else {
    aptxCtx->mode &= ~APTX_MODE::USE_AUTO_AUX;
    if (aptxCtx->m_08 > -4) {
      --aptxCtx->m_08;
    }
  }
  return aptxCtx->m_08 >= 0;
}

void aptxInitialize(aptxCtx_t* aptxCtx, unsigned int mode, int buffers, int* channel_mode, int channels) {
  if (aptxCtx) {
    auto aptxData = aptxCtx->aptxData;
    if (use_mmx()) {              // Is MMX allowed?
      mode |= APTX_MODE::USE_MMX;
    }
    aptxCtx->mode = mode;
    aptxCtx->buffers = buffers;
    aptxCtx->m_08 = -3;
    aptxCtx->channels = channels;
    if (aptxData) {
      aptxData->m_00 = -1;
      aptxData->m_04 = -1;
      aptxData->m_08 = 0;
      aptxData->m_0c = 2;
      for (auto i = 0; i < 32; ++i) {
        aptxData->m_0e[i].m_00 = -1;
        aptxData->m_0e[i].m_02 = 0;
      }
      for (auto i = 0; i < 32; ++i) {
        aptxData->m_94[i] = 0;
      }
    }
    memset(aptxCtx->aptxChannel, 0, channels * sizeof(aptxChannel_t));
    for (auto ch = 0; ch < channels; ++ch) {
      aptxCtx->aptxChannel[ch].mode = channel_mode ? channel_mode[ch] : 0;
      if (!(mode & APTX_MODE::USE_MMX)) {
        LOBYTE(aptxCtx->aptxChannel[ch].quantizer[0].m_08[0]) = 1;
        HIBYTE(aptxCtx->aptxChannel[ch].quantizer[0].m_08[0]) = 1;
        LOBYTE(aptxCtx->aptxChannel[ch].quantizer[1].m_08[0]) = 1;
        HIBYTE(aptxCtx->aptxChannel[ch].quantizer[1].m_08[0]) = 1;
        LOBYTE(aptxCtx->aptxChannel[ch].quantizer[2].m_08[0]) = 1;
        HIBYTE(aptxCtx->aptxChannel[ch].quantizer[2].m_08[0]) = 1;
        LOBYTE(aptxCtx->aptxChannel[ch].quantizer[3].m_08[0]) = 1;
        HIBYTE(aptxCtx->aptxChannel[ch].quantizer[3].m_08[0]) = 1;
      }
      else {
        LOWORD(aptxCtx->aptxChannel[ch].qmf32A[0]) = 1;
        HIWORD(aptxCtx->aptxChannel[ch].qmf32A[0]) = 1;
        LOWORD(aptxCtx->aptxChannel[ch].qmf32A[1]) = 1;
        HIWORD(aptxCtx->aptxChannel[ch].qmf32A[1]) = 1;
        LOWORD(aptxCtx->aptxChannel[ch].qmf32A[2]) = 1;
        HIWORD(aptxCtx->aptxChannel[ch].qmf32A[2]) = 1;
        LOWORD(aptxCtx->aptxChannel[ch].qmf32A[3]) = 1;
        HIWORD(aptxCtx->aptxChannel[ch].qmf32A[3]) = 1;
      }
    }
  }
}

aptxCtx_t* aptxCreate(unsigned int mode, int buffers, int* channel_mode, int channels) {
  auto aptxCtx = (aptxCtx_t*)malloc(sizeof(aptxCtx_t));
  if (aptxCtx) {
    memset(aptxCtx, 0, sizeof(aptxCtx_t));
    aptxCtx->aptxData = nullptr;
    if ((mode & APTX_MODE::ENCODE) || buffers < 0 || (aptxCtx->aptxData = (aptxData_t*)malloc(sizeof(aptxData_t)))) {
      aptxInitialize(aptxCtx, mode, buffers, channel_mode, channels);
    }
    else {
      aptxDelete(aptxCtx);
      aptxCtx = nullptr;
    }
  }
  return aptxCtx;
}

void aptxDelete(aptxCtx_t* aptxCtx) {
  if (aptxCtx) {
    if (aptxCtx->aptxData) {
      free(aptxCtx->aptxData);
      aptxCtx->aptxData = nullptr;
    }
    free(aptxCtx);
  }
}

void aptxDecInit(aptxCtx_t* aptxCtx, int channels) {
  aptxCtx->aptxData = nullptr;
  aptxInitialize(aptxCtx, APTX_MODE::DECODE, -1, nullptr, channels);
}                                                                                    

int aptxDecode(aptxCtx_t* aptxCtx, int unused, int samples, bool mode_pcm_msb, short* pcmBuf,  bool mode_aptx_msb, unsigned short* aptxBuf) {
  aptxCtx->mode = (mode_aptx_msb ? APTX_MODE::USE_APTX_MSB : 0) | (mode_pcm_msb ? APTX_MODE::USE_PCM_MSB : 0) | (aptxCtx->mode & 0xfffffff9);
  return aptxDec(aptxCtx, samples, pcmBuf, aptxBuf, nullptr);
}

void aptxEncInit(aptxCtx_t* aptxCtx, int channels) {
  aptxCtx->aptxData = nullptr;
  aptxInitialize(aptxCtx, APTX_MODE::ENCODE, -1, nullptr, channels);
}

int aptxEncode(aptxCtx_t* aptxCtx, int unused, int samples, bool mode_mcm_msb, short* pcmBuf, bool mode_aptx_msb, unsigned short* aptxBuf) {
  aptxCtx->mode = (mode_aptx_msb ? APTX_MODE::USE_APTX_MSB : 0) | (mode_mcm_msb ? APTX_MODE::USE_PCM_MSB : 0) | (aptxCtx->mode & 0xfffffff9);
  return aptxEnc(aptxCtx, samples, pcmBuf, aptxBuf, nullptr);
}

int aptxEnc(aptxCtx_t* aptxCtx, int samples, short* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status) {
  int pcm4[4];
  int bitcorr_ch1, bitcorr_ch3;
  unsigned short aptxVal;
  auto channels = aptxCtx->channels;
  auto use_buffers = aptxCtx->buffers >= 0;
  for (auto ch = 0; ch < channels; ++ch) {
    if (samples != 512 && (use_buffers || aptxCtx->aptxChannel[ch].mode)) {
      return 2;
    }
    bitcorr_ch3 = 0;
    if ((aptxCtx->aptxChannel[ch].mode == 1) || (aptxCtx->aptxChannel[ch].mode == 2 && aptxCtx->m_08 >= 0)) {
      bitcorr_ch3 = 1;
    }
    bitcorr_ch1 = use_buffers;
    for (auto n = 0; n < samples >> 2; ++n) {
      if (aptxCtx->mode & APTX_MODE::USE_PCM_MSB) {
        pcm4[0] = (SLOBYTE(pcmBuf[4 * channels * n + 0 * channels + ch]) << 8) + (pcmBuf[4 * channels * n + 0 * channels + ch] >> 8);
        pcm4[1] = (SLOBYTE(pcmBuf[4 * channels * n + 1 * channels + ch]) << 8) + (pcmBuf[4 * channels * n + 1 * channels + ch] >> 8);
        pcm4[2] = (SLOBYTE(pcmBuf[4 * channels * n + 2 * channels + ch]) << 8) + (pcmBuf[4 * channels * n + 2 * channels + ch] >> 8);
        pcm4[3] = (SLOBYTE(pcmBuf[4 * channels * n + 3 * channels + ch]) << 8) + (pcmBuf[4 * channels * n + 3 * channels + ch] >> 8);
      }
      else {
        pcm4[0] = pcmBuf[4 * channels * n + 0 * channels + ch];
        pcm4[1] = pcmBuf[4 * channels * n + 1 * channels + ch];
        pcm4[2] = pcmBuf[4 * channels * n + 2 * channels + ch];
        pcm4[3] = pcmBuf[4 * channels * n + 3 * channels + ch];
      }
      if (aptxCtx->mode & APTX_MODE::USE_MMX) {
        aptxVal = mmx_aptxChannelEncode(&aptxCtx->aptxChannel[ch], pcm4, bitcorr_ch1, bitcorr_ch3);
      }
      else {
        aptxVal = std_aptxChannelEncode(&aptxCtx->aptxChannel[ch], pcm4, bitcorr_ch1, bitcorr_ch3);
      }
      if (aptxCtx->mode & APTX_MODE::USE_APTX_MSB) {
        aptxVal = (aptxVal << 8) | (aptxVal >> 8);
      }
      aptxBuf[channels * n + ch] = aptxVal;
      bitcorr_ch1 = 0;
      if (use_buffers) {
        bitcorr_ch1 = n >= 117;
      }
    }
    if (bitcorr_ch3 > 0 && channel_status && channel_status[ch]) {
      enc_100031AF(aptxCtx, &aptxBuf[ch], channel_status[ch]);
    }
    use_buffers = false;
  }
  enc_10003101(aptxCtx, aptxBuf);
  return 0;
}

unsigned short std_aptxChannelEncode(aptxChannel_t* aptxChannel, int pcm4[4], int bitcorr_ch1, int bitcorr_ch3) {
  std_enc_aptxQMF(aptxChannel, pcm4);
  pcm4[0] = std_enc_aptxQuantizeBank(&aptxChannel->quantizer[0], pcm4[0], 7, 2816, 1, 4);
  pcm4[1] = std_enc_aptxQuantizeBank(&aptxChannel->quantizer[1], pcm4[1], 4 - bitcorr_ch1, 3328, 1, 2) << bitcorr_ch1;
  pcm4[2] = std_enc_aptxQuantizeBank(&aptxChannel->quantizer[2], pcm4[2], 2, 3584, 0, 1);
  pcm4[3] = std_enc_aptxQuantizeBank(&aptxChannel->quantizer[3], pcm4[3], 3 - bitcorr_ch3, 3584, 0, 2) << bitcorr_ch3;
  return (pcm4[0] << 0) | (pcm4[1] << 7) | (pcm4[2] << (7 + 4)) | (pcm4[3] << (7 + 4 + 2));
}

void std_enc_aptxQMF(aptxChannel_t* aptxChannel, int pcm4[4]) {
  double qmf32A[2], qmf32B[2];
  aptxChannel->qmf34A[aptxChannel->qmf34idx + 0] = (float)pcm4[0];
  aptxChannel->qmf34B[aptxChannel->qmf34idx + 0] = (float)pcm4[1];
  aptxChannel->qmf34A[aptxChannel->qmf34idx + 1] = (float)pcm4[2];
  aptxChannel->qmf34B[aptxChannel->qmf34idx + 1] = (float)pcm4[3];
  aptxChannel->qmf34idx += 2;
  if (aptxChannel->qmf34idx >= 34) {
    aptxChannel->qmf34idx = 0;
  }
  aptxQMF34(qmf32B, &QMF34_FLT_0[34 - 2 - aptxChannel->qmf34idx], aptxChannel->qmf34A);
  aptxQMF34(qmf32A, &QMF34_FLT_1[34 - 2 - aptxChannel->qmf34idx], aptxChannel->qmf34B);
  aptxChannel->qmf32B[aptxChannel->qmf32idx + 0] = (float)aptxDoubleToIntStd(qmf32A[0] + qmf32B[0]);
  aptxChannel->qmf32A[aptxChannel->qmf32idx + 0] = (float)aptxDoubleToIntSym(qmf32A[0] - qmf32B[0]);
  aptxChannel->qmf32B[aptxChannel->qmf32idx + 1] = (float)aptxDoubleToIntStd(qmf32A[1] + qmf32B[1]);
  aptxChannel->qmf32A[aptxChannel->qmf32idx + 1] = (float)aptxDoubleToIntSym(qmf32A[1] - qmf32B[1]);
  aptxChannel->qmf32idx += 2;
  if (aptxChannel->qmf32idx >= 32) {
    aptxChannel->qmf32idx = 0;
  }
  qmf32B[0] = aptxQMF32(&QMF32_FLT[32 + 0 - aptxChannel->qmf32idx], &aptxChannel->qmf32B[0]);
  qmf32B[1] = aptxQMF32(&QMF32_FLT[32 + 1 - aptxChannel->qmf32idx], &aptxChannel->qmf32B[1]);
  qmf32A[0] = aptxQMF32(&QMF32_FLT[32 + 0 - aptxChannel->qmf32idx], &aptxChannel->qmf32A[0]);
  qmf32A[1] = aptxQMF32(&QMF32_FLT[32 + 1 - aptxChannel->qmf32idx], &aptxChannel->qmf32A[1]);
  pcm4[0] = aptxDoubleToIntStd(qmf32B[1] + qmf32B[0]);
  pcm4[1] = aptxDoubleToIntSym(qmf32B[1] - qmf32B[0]);
  pcm4[2] = aptxDoubleToIntStd(qmf32A[1] + qmf32A[0]);
  pcm4[3] = aptxDoubleToIntSym(qmf32A[1] - qmf32A[0]);
}

void aptxQMF34(double dst[2], float flt[34], float src[34]) {
  auto sum0{ 0.0 };
  for (auto i = 0; i < 34; i++) {
    sum0 += flt[34 - i] * src[33 - i];
  }
  dst[0] = sum0;
  auto sum1{ 0.0 };
  for (auto i = 0; i < 34; i++) {
    sum1 += flt[33 - i] * src[33 - i];
  }
  dst[1] = sum1;
}

double aptxQMF32(float flt[32], float src[32]) {
  double dst{ 0.0 };
  for (auto i = 0; i < 32; i += 2) {
    dst += flt[30 - i] * src[30 - i];
  }
  return dst;
}

int aptxDoubleToIntStd(double value) {
  if (value >= 2147418112.0) {
    return 32767;
  }
  if (value > -2147483648.0) {
    return ((int)value + 32768) >> 16;
  }
  return -32768;
}

int aptxDoubleToIntSym(double value) {
  if (value >= 2147418112.0) {
    return 32767;
  }
  if (value > -2147418112.0) {
    return ((int)value + 32767) >> 16;
  }
  return -32767;
}

int std_enc_aptxQuantizeBank(aptxQuantizer_t* aptxQuantizer, int pcmVal, int allocBits, int maxScale, int outShift, int windowLength) {
  int clpVal;
  int outSgn;
  int outVal;
  std_encdec_10002A1F(aptxQuantizer->pcm2, aptxQuantizer, windowLength);
  clpVal = pcmClipValue(pcmVal - aptxQuantizer->pcm2[1]);
  outSgn = (clpVal >= 0) ? 0 : 1 << (allocBits - 1);
  outVal = std_enc_10003054(QTZ_TABLE[allocBits].table2, allocBits - 2, abs(clpVal), aptxQuantizer->scale2[1]);
  clpVal = std_encdec_100028F1(aptxQuantizer->scale2, &QTZ_TABLE[allocBits], outSgn | outVal, maxScale, outShift);
  clpVal = pcmClipValue(clpVal);
  std_encdec_10002C26(clpVal, aptxQuantizer->pcm2, aptxQuantizer, windowLength);
  return outSgn | outVal;
}

int std_encdec_100028F1(int scale2[2], aptxQuantizationTable_t* qtz_entry, int pcmVal, int maxScale, int outShift) {
  int v8; // [esp+8h] [ebp-1Ch]
  int v9; // [esp+Ch] [ebp-18h]
  int v10; // [esp+10h] [ebp-14h]
  int v11; // [esp+14h] [ebp-10h]
  int v12; // [esp+14h] [ebp-10h]
  int v13; // [esp+18h] [ebp-Ch]
  int v14; // [esp+1Ch] [ebp-8h]
  int v15; // [esp+20h] [ebp-4h]

  v10 = (qtz_entry->encStates - 1) & pcmVal;
  v11 = scale2[1] * qtz_entry->table1[v10].c1;
  if (qtz_entry->encStates & pcmVal) {
    v11 = -v11;
  }
  v12 = (v11 + (1 << 13)) >> 14;
  v15 = qtz_entry->table1[v10].c2;
  v13 = ((16310 * scale2[0]) >> 14) + v15;
  if (v13 >= 0) {
    if (v13 >= maxScale) {
      v8 = maxScale;
    }
    else {
      v8 = ((16310 * scale2[0]) >> 14) + v15;
    }
    v9 = v8;
  }
  else {
    v9 = 0;
  }
  scale2[0] = v9;
  v14 = ENC_IDX_1000F4EC[(v9 >> 3) & 0x1F] << (outShift + BYTE1(v9) + 1);
  scale2[1] = pcmClipValue((v14 + (1 << 13)) >> 14);
  return v12;
}

void std_encdec_10002A1F(int pcm2[2], aptxQuantizer_t* aptxQuantizer, int windowLength) {
  auto pcmVal = pcmClipValue((aptxQuantizer->m_00[0] * aptxQuantizer->m_08[1] + aptxQuantizer->m_00[1] * aptxQuantizer->m_08[2] + 8192) >> 14);
  aptxQuantizer->m_08[2] = aptxQuantizer->m_08[1];
  auto sum{ 0.0 };
  for (auto i = 0; i < windowLength; ++i) {
    sum +=
      aptxQuantizer->m_10[6 * i + 24 + 5] * aptxQuantizer->m_10[6 * i + 5] +
      aptxQuantizer->m_10[6 * i + 24 + 4] * aptxQuantizer->m_10[6 * i + 4] +
      aptxQuantizer->m_10[6 * i + 24 + 3] * aptxQuantizer->m_10[6 * i + 3] +
      aptxQuantizer->m_10[6 * i + 24 + 2] * aptxQuantizer->m_10[6 * i + 2] +
      aptxQuantizer->m_10[6 * i + 24 + 1] * aptxQuantizer->m_10[6 * i + 1] +
      aptxQuantizer->m_10[6 * i + 24 + 0] * aptxQuantizer->m_10[6 * i + 0];
  }
  pcm2[0] = aptxDoubleToIntStd(sum * 4.0);
  for (auto i = 6 * windowLength; i > 0; --i) {
    aptxQuantizer->m_10[i + 24] = aptxQuantizer->m_10[i + 24 - 1];
  }
  pcm2[1] = pcmClipValue(pcmVal + pcm2[0]);
}

void std_encdec_10002C26(int pcmVal, int pcm2[2], aptxQuantizer_t* aptxQuantizer, int windowLength) {
  int v5; // [esp+4h] [ebp-48h]
  int v6; // [esp+8h] [ebp-44h]
  int v7; // [esp+Ch] [ebp-40h]
  int v8; // [esp+10h] [ebp-3Ch]
  int v9; // [esp+14h] [ebp-38h]
  int v10; // [esp+18h] [ebp-34h]
  int v11; // [esp+1Ch] [ebp-30h]
  int v14; // [esp+28h] [ebp-24h]
  int v15; // [esp+30h] [ebp-1Ch]
  int v16; // [esp+34h] [ebp-18h]
  int v17; // [esp+38h] [ebp-14h]
  int v18; // [esp+3Ch] [ebp-10h]
  int v19; // [esp+3Ch] [ebp-10h]
  int64_t v20; // [esp+40h] [ebp-Ch]
  int v21; // [esp+44h] [ebp-8h]

  aptxQuantizer->m_10[24] = pcmVal;
  aptxQuantizer->m_08[1] = pcmClipValue(pcm2[1] + pcmVal);
  v15 = aptxQuantizer->m_00[0];
  LODWORD(v20) = (((pcm2[0] + pcmVal) >> 31) ^ 1) - ((pcm2[0] + pcmVal) >> 31);
  v18 = SLOBYTE(aptxQuantizer->m_08[0]);
  HIDWORD(v20) = SHIBYTE(aptxQuantizer->m_08[0]);
  LOBYTE(aptxQuantizer->m_08[0]) = v20;
  HIBYTE(aptxQuantizer->m_08[0]) = v18;
  v19 = (int)((v20 ^ (v18 >> 31)) - (v18 >> 31));
  v21 = (int)llabs(v20);
  v16 = ((16320 * v15 + 8192) >> 14) + 192 * v19;
  if (v15 >= -8192) {
    if (v15 >= 8192) {
      v10 = 8192;
    }
    else {
      v10 = v15;
    }
    v11 = v10;
  }
  else {
    v11 = -8192;
  }
  v14 = (16256 * aptxQuantizer->m_00[1] + (((v21 << 12) + v11 * -v19) << 9) + 8192) >> 14;
  if (v14 >= -12288) {
    if (v14 >= 12288) {
      v8 = 12288;
    }
    else {
      v8 = (16256 * aptxQuantizer->m_00[1] + (((v21 << 12) + v11 * -v19) << 9) + 8192) >> 14;
    }
    v9 = v8;
  }
  else {
    v9 = -12288;
  }
  aptxQuantizer->m_00[1] = v9;
  if (v16 >= v9 - 15360) {
    if (v16 >= 15360 - v9) {
      v6 = 15360 - v9;
    }
    else {
      v6 = ((16320 * v15 + 8192) >> 14) + 192 * v19;
    }
    v7 = v6;
  }
  else {
    v7 = v9 - 15360;
  }
  aptxQuantizer->m_00[0] = v7;
  v17 = aptxQuantizer->m_10[24];
  if (v17 >= 0) {
    v5 = v17 != 0 ? 0x80 : 0;
  }
  else {
    v5 = -128;
  }
  for (auto n = 0; n < windowLength; ++n) {
    aptxQuantizer->m_10[6 * n] = (v5 ^ (aptxQuantizer->m_10[6 * n + 25] >> 31))
      - (aptxQuantizer->m_10[6 * n + 25] >> 31)
      + ((255 * aptxQuantizer->m_10[6 * n] + 128) >> 8);
    aptxQuantizer->m_10[6 * n + 1] = (v5 ^ (aptxQuantizer->m_10[6 * n + 26] >> 31))
      - (aptxQuantizer->m_10[6 * n + 26] >> 31)
      + ((255 * aptxQuantizer->m_10[6 * n + 1] + 128) >> 8);
    aptxQuantizer->m_10[6 * n + 2] = (v5 ^ (aptxQuantizer->m_10[6 * n + 27] >> 31))
      - (aptxQuantizer->m_10[6 * n + 27] >> 31)
      + ((255 * aptxQuantizer->m_10[6 * n + 2] + 128) >> 8);
    aptxQuantizer->m_10[6 * n + 3] = (v5 ^ (aptxQuantizer->m_10[6 * n + 28] >> 31))
      - (aptxQuantizer->m_10[6 * n + 28] >> 31)
      + ((255 * aptxQuantizer->m_10[6 * n + 3] + 128) >> 8);
    aptxQuantizer->m_10[6 * n + 4] = (v5 ^ (aptxQuantizer->m_10[6 * n + 29] >> 31))
      - (aptxQuantizer->m_10[6 * n + 29] >> 31)
      + ((255 * aptxQuantizer->m_10[6 * n + 4] + 128) >> 8);
    aptxQuantizer->m_10[6 * n + 5] = (v5 ^ (aptxQuantizer->m_88[6 * n] >> 31))
      - (aptxQuantizer->m_88[6 * n] >> 31)
      + ((255 * aptxQuantizer->m_10[6 * n + 5] + 128) >> 8);
  }
}

int std_enc_10003054(short* qtz_data, int bits, int absVal, int scale) {
  int delta;
  auto sclVal = absVal << 14;
  auto bitMsk = 1 << bits;
  auto qtz_ref = &qtz_data[(1 << bits) - 1];
  for (auto i = 0; i < bits; ++i) {
    bitMsk >>= 1;
    delta = (sclVal - qtz_ref[0] * scale) >> 31;
    qtz_ref += (bitMsk ^ delta) - delta;
  }
  delta = (sclVal - qtz_ref[0] * scale) >> 31;
  return (int)(&qtz_ref[delta + 1] - qtz_data);
}

void enc_10003101(aptxCtx_t* aptxCtx, unsigned short* aptxBuf) {
  int v2; // [esp+0h] [ebp-10h]
  int v3; // [esp+4h] [ebp-Ch]
  unsigned char* v5; // [esp+Ch] [ebp-4h]
  unsigned char* v6; // [esp+Ch] [ebp-4h]

  if (aptxCtx->buffers >= 0) {
    v2 = ENC_IDX_1000F56C[aptxCtx->buffers] << 7;
    v5 = (unsigned char*)aptxBuf + (aptxCtx->mode & APTX_MODE::USE_APTX_MSB ? 1 : 0);
    v3 = 2 * aptxCtx->channels;
    if (aptxCtx->mode & APTX_MODE::USE_AUTO_AUX) {
      *v5 |= 0x80u;
    }
    v6 = &v5[127 * v3];
    for (auto i = 0; i < 10; ++i) {
      *v6 |= v2 & 0x80;
      v6 -= v3;
      v2 >>= 1;
    }
  }
}

void enc_100031AF(aptxCtx_t* aptxCtx, unsigned short* aptxBuf, unsigned char* channel_status) {
  int v3; // [esp+0h] [ebp-14h]
  int v5; // [esp+8h] [ebp-Ch]
  unsigned char* v6; // [esp+Ch] [ebp-8h]

  v5 = 2 * aptxCtx->channels;
  v6 = (unsigned char*)&aptxBuf[127 * aptxCtx->channels] + (aptxCtx->mode & APTX_MODE::USE_PCM_MSB ? 1 : 0);
  for (auto i = 15; i >= 0; --i) {
    v3 = 32 * (char)channel_status[i];
    for (auto j = 0; j < 8; ++j) {
      *v6 |= v3 & 0x20;
      v6 -= v5;
      v3 >>= 1;
    }
  }
}

int aptxDec(aptxCtx_t* aptxCtx, int samples, short* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status) {
  int v18; // [esp+3Ch] [ebp-28h]
  int v22; // [esp+50h] [ebp-14h]
  int v24; // [esp+58h] [ebp-Ch]

  int pcm4[4];
  int bitcorr_ch1, bitcorr_ch3[2];
  auto channels = aptxCtx->channels;
  v18 = 0;
  v22 = 0;
  v24 = 0;
  bitcorr_ch1 = 0;
  for (auto ch = 0; ch < channels; ++ch) {
    bitcorr_ch3[ch] = 0;
    if ((aptxCtx->aptxChannel[ch].mode == 1) || (aptxCtx->aptxChannel[ch].mode == 2 && aptxCtx->m_08 >= 0)) {
      bitcorr_ch3[ch] = 1;
    }
  }
  if (aptxCtx->aptxData) {
    auto aptxData = aptxCtx->aptxData;
    if (channels > 2) {
      return 2;
    }
    if (aptxData->m_00 < 0) {
      dec_aptx_data0_lt_0(aptxCtx, aptxBuf);
    }
    if (aptxData->m_00 >= 0) {
      dec_aptx_data0_ge_0(aptxCtx, aptxBuf);
    }
    if (aptxData->m_00 < 0) {
      for (auto n = 0; n < samples * channels; ++n) {
        pcmBuf[n] = 0;
      }
      if (channel_status) {
        for (auto ch = 0; ch < channels; ++ch) {
          channel_status[ch] = nullptr;
        }
      }
      return 1;
    }
    v22 = aptxData->m_00;
    v24 = -aptxData->m_04 & 0x7F;
    v18 = 1;
  }
  if (channel_status) {
    for (auto ch = 0; ch < channels; ++ch) {
      channel_status[ch] = dec_get_channel_status(aptxCtx, ch, aptxBuf);
    }
  }
  for (auto n = 0; n < samples >> 2; ++n) {
    bitcorr_ch1 = ((v24 - 11) >> 31) & v18;
    if (v24 == 10) {
      for (auto ch = 0; ch < channels; ++ch) {
        if (aptxCtx->aptxChannel[ch].mode == 2) {
          bitcorr_ch3[ch] = aptxCtx->m_08 >= 0;
        }
      }
    }
    for (auto ch = 0; ch < channels; ++ch) {
      auto aptxVal = aptxBuf[channels * n + ch];
      if (aptxCtx->mode & APTX_MODE::USE_APTX_MSB) {
        aptxVal = (aptxVal << 8) | (aptxVal >> 8);
      }
      aptxVal = (unsigned int)(__PAIR64__(aptxCtx->m_0c, (aptxVal << 16) >> v22)) >> 16;
      if (aptxCtx->mode & APTX_MODE::USE_MMX) {
        mmx_aptxChannelDecode(&aptxCtx->aptxChannel[ch], pcm4, aptxVal, bitcorr_ch1, bitcorr_ch3[ch]);
      }
      else {
        std_aptxChannelDecode(&aptxCtx->aptxChannel[ch], pcm4, aptxVal, bitcorr_ch1, bitcorr_ch3[ch]);
      }
      if (aptxCtx->mode & APTX_MODE::USE_PCM_MSB) {
        pcmBuf[4 * channels * n + 0 * channels + ch] = (SLOBYTE(pcm4[0]) << 8) | (SHIBYTE(pcm4[0]) >> 8);
        pcmBuf[4 * channels * n + 1 * channels + ch] = (SLOBYTE(pcm4[1]) << 8) | (SHIBYTE(pcm4[1]) >> 8);
        pcmBuf[4 * channels * n + 2 * channels + ch] = (SLOBYTE(pcm4[2]) << 8) | (SHIBYTE(pcm4[2]) >> 8);
        pcmBuf[4 * channels * n + 3 * channels + ch] = (SLOBYTE(pcm4[3]) << 8) | (SHIBYTE(pcm4[3]) >> 8);
      }
      else {
        pcmBuf[4 * channels * n + 0 * channels + ch] = pcm4[0];
        pcmBuf[4 * channels * n + 1 * channels + ch] = pcm4[1];
        pcmBuf[4 * channels * n + 2 * channels + ch] = pcm4[2];
        pcmBuf[4 * channels * n + 3 * channels + ch] = pcm4[3];
      }
    }
  }
  return 0;
}

void std_aptxChannelDecode(aptxChannel_t* aptxChannel, int pcm4[4], unsigned short aptxVal, int bitcorr_ch1, int bitcorr_ch3) { 
  pcm4[0] = std_dec_aptxQuantizeBank(&aptxChannel->quantizer[0], aptxVal & 0x7F, 7, 2816, 1, 4);
  pcm4[1] = std_dec_aptxQuantizeBank(&aptxChannel->quantizer[1], ((aptxVal >> 7) & 0xF) >> bitcorr_ch1, 4 - bitcorr_ch1, 3328, 1, 2);
  pcm4[2] = std_dec_aptxQuantizeBank(&aptxChannel->quantizer[2], (aptxVal >> 11) & 3, 2, 3584, 0, 1);
  pcm4[3] = std_dec_aptxQuantizeBank(&aptxChannel->quantizer[3], ((aptxVal >> 13) & 7) >> bitcorr_ch3, 3 - bitcorr_ch3, 3584, 0, 2);
  std_dec_aptxQMF(aptxChannel, pcm4);
}

void std_dec_aptxQMF(aptxChannel_t* aptxChannel, int pcm4[4]) {
  double qmf34A[2], qmf34B[2];
  aptxChannel->qmf32B[aptxChannel->qmf32idx + 0] = (float)pcmClipValue(pcm4[0] + pcm4[1]);
  aptxChannel->qmf32B[aptxChannel->qmf32idx + 1] = (float)pcmClipValue(pcm4[0] - pcm4[1]);
  aptxChannel->qmf32A[aptxChannel->qmf32idx + 0] = (float)pcmClipValue(pcm4[2] + pcm4[3]);
  aptxChannel->qmf32A[aptxChannel->qmf32idx + 1] = (float)pcmClipValue(pcm4[2] - pcm4[3]);
  aptxChannel->qmf32idx += 2;
  if (aptxChannel->qmf32idx >= 32) {
    aptxChannel->qmf32idx = 0;
  }
  double v;
  v = aptxQMF32(&QMF32_FLT[32 + 0 - aptxChannel->qmf32idx], &aptxChannel->qmf32B[0]);
  pcm4[0] = aptxDoubleToIntStd(2 * v);
  v = aptxQMF32(&QMF32_FLT[32 + 1 - aptxChannel->qmf32idx], &aptxChannel->qmf32B[1]);
  pcm4[1] = aptxDoubleToIntStd(2 * v);
  v = aptxQMF32(&QMF32_FLT[32 + 0 - aptxChannel->qmf32idx], &aptxChannel->qmf32A[0]);
  pcm4[2] = aptxDoubleToIntStd(2 * v);
  v = aptxQMF32(&QMF32_FLT[32 + 1 - aptxChannel->qmf32idx], &aptxChannel->qmf32A[1]);
  pcm4[3] = aptxDoubleToIntStd(2 * v);
  aptxChannel->qmf34A[aptxChannel->qmf34idx + 0] = (float)pcmClipValue(pcm4[1] + pcm4[3]);
  aptxChannel->qmf34B[aptxChannel->qmf34idx + 0] = (float)pcmClipValue(pcm4[1] - pcm4[3]);
  aptxChannel->qmf34A[aptxChannel->qmf34idx + 1] = (float)pcmClipValue(pcm4[0] + pcm4[2]);
  aptxChannel->qmf34B[aptxChannel->qmf34idx + 1] = (float)pcmClipValue(pcm4[0] - pcm4[2]);
  aptxChannel->qmf34idx += 2;
  if (aptxChannel->qmf34idx >= 34) {
    aptxChannel->qmf34idx = 0;
  }
  aptxQMF34(qmf34A, &QMF34_FLT_0[34 - 2 - aptxChannel->qmf34idx], aptxChannel->qmf34A);
  aptxQMF34(qmf34B, &QMF34_FLT_1[34 - 2 - aptxChannel->qmf34idx], aptxChannel->qmf34B);
  pcm4[0] = aptxDoubleToIntStd(2 * qmf34B[0]);
  pcm4[1] = aptxDoubleToIntStd(2 * qmf34A[0]);
  pcm4[2] = aptxDoubleToIntStd(2 * qmf34B[1]);
  pcm4[3] = aptxDoubleToIntStd(2 * qmf34A[1]);
}

int std_dec_aptxQuantizeBank(aptxQuantizer_t* aptxQuantizer, int aptxVal, int allocBits, int maxScale, int outShift, int windowLength) {
  auto v = std_encdec_100028F1(aptxQuantizer->scale2, &QTZ_TABLE[allocBits], aptxVal, maxScale, outShift);
  v = pcmClipValue(v);
  std_encdec_10002A1F(aptxQuantizer->pcm2, aptxQuantizer, windowLength);
  std_encdec_10002C26(v, aptxQuantizer->pcm2, aptxQuantizer, windowLength);
  return aptxQuantizer->m_08[1];
}

unsigned char* dec_get_channel_status(aptxCtx_t* aptxCtx, int channel, unsigned short* aptxBuf) {
  unsigned char *v4; // [esp+0h] [ebp-30h]
  unsigned char *v5; // [esp+0h] [ebp-30h]
  int v6; // [esp+4h] [ebp-2Ch]
  unsigned char *v13; // [esp+10h] [ebp-20h]
  int v14; // [esp+14h] [ebp-1Ch]
  int v15; // [esp+14h] [ebp-1Ch]
  int v16; // [esp+18h] [ebp-18h]
  int v17; // [esp+1Ch] [ebp-14h]
  int v18; // [esp+24h] [ebp-Ch]
  char v19; // [esp+24h] [ebp-Ch]
  unsigned char *v20; // [esp+28h] [ebp-8h]
  aptxData_t *aptxData; // [esp+2Ch] [ebp-4h]

  v20 = 0;
  aptxData = aptxCtx->aptxData;
  v6 = 2 * aptxCtx->channels;
  v18 = 16 * channel + 2;
  v14 = 0;
  v16 = 128;
	v17 = 0;
  if (aptxData) {
    v18 -= aptxData->m_00;
    v15 = aptxData->m_04 + 10;
    if (v18 < 0) {
      v18 += 16 * aptxCtx->channels;
      LOBYTE(v15) = LOBYTE(aptxData->m_04) + 9;
    }
    v14 = v15 & 0x7F;
    v16 = 128 - v14;
  }
  if ((aptxCtx->mode & 2) == 0) {
    v18 ^= 8u;
  }
  v13 = (unsigned char *)aptxBuf + (v18 >> 3);
  v19 = 7 - (v18 & 7);
  if ((v14 > 0) && aptxCtx->aptxChannel[channel].status_index >= 0) {
    v20 = &aptxCtx->aptxChannel[channel].status_data[16 * aptxCtx->aptxChannel[channel].status_index];
    v4 = &v20[v16 >> 3];
    if ((v14 & 7) != 0) {
      v17 = (char)*v4;
      for (auto i = 0; i < (v14 & 7); ++i) {
        v17 = (((char)*v13 >> v19) & 1) | (2 * v17);
        v13 += v6;
      }
      *v4++ = v17;
    }
    for (auto j = 0; j < v14 >> 3; ++j) {
      for (auto k = 0; k < 8; ++k) {
        v17 = (((char)*v13 >> v19) & 1) | (2 * v17);
        v13 += v6;
      }
      *v4++ = v17;
    }
    aptxCtx->aptxChannel[channel].status_index = 1 - aptxCtx->aptxChannel[channel].status_index;
  }
  if ((aptxCtx->aptxChannel[channel].mode == 1) || (aptxCtx->aptxChannel[channel].mode == 2 && aptxCtx->m_08 >= 0)) {
    aptxCtx->aptxChannel[channel].status_index &= 1u;
    v5 = &aptxCtx->aptxChannel[channel].status_data[16 * aptxCtx->aptxChannel[channel].status_index];
    for (auto m = 0; m < v16 >> 3; ++m) {
      for (auto n = 0; n < 8; ++n) {
        v17 = (((char)*v13 >> v19) & 1) | (2 * v17);
        v13 += v6;
      }
      *v5++ = v17;
    }
    if ((v16 & 7) != 0) {
      for (auto i = 0; i < (v16 & 7); ++i) {
        v17 = (((char)*v13 >> v19) & 1) | (2 * v17);
        v13 += v6;
      }
      *v5 = v17;
    }
    if (v16 == 128) {
      return &aptxCtx->aptxChannel[channel].status_data[16 * aptxCtx->aptxChannel[channel].status_index];
    }
  }
  else {
    aptxCtx->aptxChannel[channel].status_index = -1;
  }
  return v20;
} 

void dec_aptx_data0_lt_0(aptxCtx_t* aptxCtx, unsigned short* aptxBuf) {
  int v2; // [esp+0h] [ebp-B8h]
  int v3; // [esp+4h] [ebp-B4h]
  int v4; // [esp+4h] [ebp-B4h]
  int v5; // [esp+8h] [ebp-B0h]
  int v6; // [esp+Ch] [ebp-ACh]
  int v7; // [esp+10h] [ebp-A8h]
  int channels; // [esp+14h] [ebp-A4h]
  int v9; // [esp+18h] [ebp-A0h]
  int *v16; // [esp+28h] [ebp-90h]
  int v17; // [esp+2Ch] [ebp-8Ch]
  int v18[32]; // [esp+30h] [ebp-88h]
  int v19; // [esp+B0h] [ebp-8h]

  auto aptxData = aptxCtx->aptxData;
  v19 = ENC_IDX_1000F56C[aptxCtx->buffers];
  channels = aptxCtx->channels;
  v9 = 0;
  for (auto i = 0; i < 128; ++i) {
    v16 = aptxData->m_94;
    for (auto j = 0; j < channels; ++j ) {
      v7 = (short)*aptxBuf++;
      for (auto k = 0; k < 16; ++k) {
        v6 = (v7 & 1) | (2 * *v16);
        v7 >>= 1;
        if ((v6 & 0x3FF) == v19) {
          v18[v9] = (i << 8) + k + 16 * j;
          v9 = ((unsigned char)v9 + 1) & 0x1F;
        }
        *v16++ = v6;
      }
    }
  }
  v17 = (char)aptxData->m_0c;
  aptxData->m_0c = v17 + 1;
  for (auto m = 0; m < v9; ++m) {
    int n;
    for (n = 0; n < 32 && ((unsigned short)aptxData->m_0e[n].m_00 != v18[m] || (char)aptxData->m_0e[n].m_02 != v17); ++n);
    if (n < 32) {
      aptxData->m_0e[n].m_02 = aptxData->m_0c;
      if (!--aptxData->m_0e[n].m_03) {
        v2 = (int)(unsigned short)aptxData->m_0e[n].m_00 >> 8;
        v3 = (unsigned char)aptxData->m_0e[n].m_00 ^ 0xF;
        if (aptxCtx->mode & APTX_MODE::USE_APTX_MSB) {
          v3 = (unsigned char)aptxData->m_0e[n].m_00 ^ 7;
        }
        v4 = v3 - 9;
        if (v4 < 0) {
          v4 += 16 * channels;
          LOBYTE(v2) = v2 - 1;
        }
        aptxData->m_00 = 16 * channels - 1 - v4;
        aptxData->m_04 = ((unsigned char)v2 - 8) & 0x7F;
        aptxData->m_90 = 0;
        aptxCtx->m_08 = -1;
      }
    }
    else {
      v5 = aptxData->m_08;
      aptxData->m_0e[v5].m_00 = v18[m];
      aptxData->m_0e[v5].m_02 = aptxData->m_0c;
      aptxData->m_0e[v5].m_03 = 2;
      for (auto i = 0; i < 32; ++i) {
        v5 = ((unsigned char)v5 + 1) & 0x1F;
        if ( ((aptxData->m_0e[v5].m_02 - (unsigned char)v17) & 0xFE) != 0 ) {
          aptxData->m_08 = v5;
          break;
        }
      }
    }
  }
}

void dec_aptx_data0_ge_0(aptxCtx_t* aptxCtx, unsigned short* aptxBuf) {
  int channels; // [esp+0h] [ebp-20h]
  int v3; // [esp+4h] [ebp-1Ch]
  int v5; // [esp+10h] [ebp-10h]
  char v6; // [esp+10h] [ebp-10h]
  int v7; // [esp+14h] [ebp-Ch]
  int v9; // [esp+1Ch] [ebp-4h]
  int v10; // [esp+1Ch] [ebp-4h]

  auto aptxData = aptxCtx->aptxData;
  v7 = ENC_IDX_1000F56C[aptxCtx->buffers];
  channels = aptxCtx->channels;
  v3 = 1;
  v9 = channels * (2 * aptxData->m_04 + 20);
  v5 = 8 - aptxData->m_00;
  if (!(aptxCtx->mode & APTX_MODE::USE_APTX_MSB)) {
    v5 ^= 8u;
  }
  if (v5 < 0) {
    v5 += 16 * channels;
    v9 -= 2 * channels;
  }
  v10 = (v5 >> 3) + v9;
  v6 = 7 - (v5 & 7);
  if (v10 < 0) {
    v10 += channels << 8;
  }
  if (v10 >= channels << 8) {
    v10 -= channels << 8;
  }
  if ((*((char *)aptxBuf + v10) >> v6) & 1) {
    if (aptxCtx->m_08 < 3) {
      ++aptxCtx->m_08;
    }
  }
  else if (aptxCtx->m_08 > -4) {
    --aptxCtx->m_08;
  }
  for (auto i = 0; i < 10; ++i) {
    v10 = ((channels << 8) & ((v10 - 2 * channels) >> 31)) + v10 - 2 * channels;
    if (((unsigned char)v7 ^ (unsigned char)(*((char *)aptxBuf + v10) >> v6)) & 1) {
      v3 = 0;
      break;
    }
    v7 >>= 1;
  }
  if (v3) {
    if (aptxData->m_90 > 0) {
      --aptxData->m_90;
    }
  }
  else if (++aptxData->m_90 >= 3) {
    aptxData->m_00 = -1;
  }
}
