#pragma once

#include "defs.h"
#include "aptx100fp.h"
#include <cmath>

template<typename real_t> aptx100fp_t<real_t>::aptx100fp_t() : aptxCtx(nullptr) {
}

template<typename real_t> aptx100fp_t<real_t>::~aptx100fp_t() {
	free();
}

template<typename real_t> bool aptx100fp_t<real_t>::init(unsigned int mode, bool use_int16) {
	aptxCtx = aptxCreate(mode, -1, 0, 1);
	aptxInt = use_int16;
	return aptxCtx != nullptr;
}

template<typename real_t> void aptx100fp_t<real_t>::free() {
	if (aptxCtx) {
		aptxDelete(aptxCtx);
		aptxCtx = nullptr;
	}
}

template<typename real_t> int aptx100fp_t<real_t>::run(int samples, real_t* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status) {
  int rc = 0;
  switch (aptxCtx->mode & 1) {
  case APTX_MODE::DECODE:
		if (aptxInt) {
      pcmInt16.resize(samples);
			rc = aptxDec(aptxCtx, samples, pcmInt16.data(), aptxBuf, channel_status);
			for (auto sample = 0; sample < samples; sample++) {
				pcmBuf[sample] = real_t(pcmInt16[sample]) / real_t(32767);
			}
		}
		else {
      rc = decode(samples, pcmBuf, aptxBuf, channel_status);
		}
    break;
  case APTX_MODE::ENCODE:
    if (aptxInt) {
      pcmInt16.resize(samples);
      for (auto sample = 0; sample < samples; sample++) {
        pcmInt16[sample] = short(std::round(pcmBuf[sample] * real_t(32767)));
      }
      rc = aptxEnc(aptxCtx, samples, pcmInt16.data(), aptxBuf, channel_status);
    }
    else {
      rc = encode(samples, pcmBuf, aptxBuf, channel_status);
    }
    break;
  }
  return rc;
}

template<typename real_t> int aptx100fp_t<real_t>::decode(int samples, real_t* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status) {
  int v18; // [esp+3Ch] [ebp-28h]
  int v22; // [esp+50h] [ebp-14h]
  int v24; // [esp+58h] [ebp-Ch]

  real_t pcm4[4];
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
      channelDecode(&aptxCtx->aptxChannel[ch], pcm4, aptxVal, bitcorr_ch1, bitcorr_ch3[ch]);
      pcmBuf[4 * channels * n + 0 * channels + ch] = real_t(pcm4[0]) / real_t(32768);
      pcmBuf[4 * channels * n + 1 * channels + ch] = real_t(pcm4[1]) / real_t(32768);
      pcmBuf[4 * channels * n + 2 * channels + ch] = real_t(pcm4[2]) / real_t(32768);
      pcmBuf[4 * channels * n + 3 * channels + ch] = real_t(pcm4[3]) / real_t(32768);
    }
  }
  return 0;
}

template<typename real_t> void aptx100fp_t<real_t>::channelDecode(aptxChannel_t* aptxChannel, real_t pcm4[4], unsigned short aptxVal, int bitcorr_ch1, int bitcorr_ch3) {
  pcm4[0] = real_t(std_dec_aptxQuantizeBank(&aptxChannel->quantizer[0], aptxVal & 0x7F, 7, 2816, 1, 4));
  pcm4[1] = real_t(std_dec_aptxQuantizeBank(&aptxChannel->quantizer[1], ((aptxVal >> 7) & 0xF) >> bitcorr_ch1, 4 - bitcorr_ch1, 3328, 1, 2));
  pcm4[2] = real_t(std_dec_aptxQuantizeBank(&aptxChannel->quantizer[2], (aptxVal >> 11) & 3, 2, 3584, 0, 1));
  pcm4[3] = real_t(std_dec_aptxQuantizeBank(&aptxChannel->quantizer[3], ((aptxVal >> 13) & 7) >> bitcorr_ch3, 3 - bitcorr_ch3, 3584, 0, 2));
  channelDecodeQMF(aptxChannel, pcm4);
}

template<typename real_t> void aptx100fp_t<real_t>::channelDecodeQMF(aptxChannel_t* aptxChannel, real_t pcm4[4]) {
  double qmf34A[2], qmf34B[2];
  aptxChannel->qmf32B[aptxChannel->qmf32idx + 0] = float(pcm4[0] + pcm4[1]);
  aptxChannel->qmf32B[aptxChannel->qmf32idx + 1] = float(pcm4[0] - pcm4[1]);
  aptxChannel->qmf32A[aptxChannel->qmf32idx + 0] = float(pcm4[2] + pcm4[3]);
  aptxChannel->qmf32A[aptxChannel->qmf32idx + 1] = float(pcm4[2] - pcm4[3]);
  aptxChannel->qmf32idx += 2;
  if (aptxChannel->qmf32idx >= 32) {
    aptxChannel->qmf32idx = 0;
  }
  pcm4[0] = real_t(aptxQMF32(&QMF32_FLT[32 + 0 - aptxChannel->qmf32idx], &aptxChannel->qmf32B[0]) / 32768);
  pcm4[1] = real_t(aptxQMF32(&QMF32_FLT[32 + 1 - aptxChannel->qmf32idx], &aptxChannel->qmf32B[1]) / 32768);
  pcm4[2] = real_t(aptxQMF32(&QMF32_FLT[32 + 0 - aptxChannel->qmf32idx], &aptxChannel->qmf32A[0]) / 32768);
  pcm4[3] = real_t(aptxQMF32(&QMF32_FLT[32 + 1 - aptxChannel->qmf32idx], &aptxChannel->qmf32A[1]) / 32768);
  aptxChannel->qmf34A[aptxChannel->qmf34idx + 0] = float(pcm4[1] + pcm4[3]);
  aptxChannel->qmf34B[aptxChannel->qmf34idx + 0] = float(pcm4[1] - pcm4[3]);
  aptxChannel->qmf34A[aptxChannel->qmf34idx + 1] = float(pcm4[0] + pcm4[2]);
  aptxChannel->qmf34B[aptxChannel->qmf34idx + 1] = float(pcm4[0] - pcm4[2]);
  aptxChannel->qmf34idx += 2;
  if (aptxChannel->qmf34idx >= 34) {
    aptxChannel->qmf34idx = 0;
  }
  aptxQMF34(qmf34A, &QMF34_FLT_0[34 - 2 - aptxChannel->qmf34idx], aptxChannel->qmf34A);
  aptxQMF34(qmf34B, &QMF34_FLT_1[34 - 2 - aptxChannel->qmf34idx], aptxChannel->qmf34B);
  pcm4[0] = real_t(qmf34B[0] / 32768);
  pcm4[1] = real_t(qmf34A[0] / 32768);
  pcm4[2] = real_t(qmf34B[1] / 32768);
  pcm4[3] = real_t(qmf34A[1] / 32768);
}

template<typename real_t> int aptx100fp_t<real_t>::encode(int samples, real_t* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status) {
  real_t pcm4[4];
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
      pcm4[0] = pcmBuf[4 * channels * n + 0 * channels + ch];
      pcm4[1] = pcmBuf[4 * channels * n + 1 * channels + ch];
      pcm4[2] = pcmBuf[4 * channels * n + 2 * channels + ch];
      pcm4[3] = pcmBuf[4 * channels * n + 3 * channels + ch];
      aptxVal = channelEncode(&aptxCtx->aptxChannel[ch], pcm4, bitcorr_ch1, bitcorr_ch3);
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

template<typename real_t> unsigned short aptx100fp_t<real_t>::channelEncode(aptxChannel_t* aptxChannel, real_t pcm4[4], int bitcorr_ch1, int bitcorr_ch3) {
  int pcm4i[4];
  channelEncodeQMF(aptxChannel, pcm4);
  pcm4i[0] = int(std::round(pcm4[0] * 32768));
  pcm4i[1] = int(std::round(pcm4[1] * 32768));
  pcm4i[2] = int(std::round(pcm4[2] * 32768));
  pcm4i[3] = int(std::round(pcm4[3] * 32768));
  pcm4i[0] = std_enc_aptxQuantizeBank(&aptxChannel->quantizer[0], pcm4i[0], 7, 2816, 1, 4);
  pcm4i[1] = std_enc_aptxQuantizeBank(&aptxChannel->quantizer[1], pcm4i[1], 4 - bitcorr_ch1, 3328, 1, 2) << bitcorr_ch1;
  pcm4i[2] = std_enc_aptxQuantizeBank(&aptxChannel->quantizer[2], pcm4i[2], 2, 3584, 0, 1);
  pcm4i[3] = std_enc_aptxQuantizeBank(&aptxChannel->quantizer[3], pcm4i[3], 3 - bitcorr_ch3, 3584, 0, 2) << bitcorr_ch3;
  return (pcm4i[0] << 0) | (pcm4i[1] << 7) | (pcm4i[2] << (7 + 4)) | (pcm4i[3] << (7 + 4 + 2));
}

template<typename real_t> void aptx100fp_t<real_t>::channelEncodeQMF(aptxChannel_t* aptxChannel, real_t pcm4[4]) {
  double qmf32A[2], qmf32B[2];
  aptxChannel->qmf34A[aptxChannel->qmf34idx + 0] = float(pcm4[0]);
  aptxChannel->qmf34B[aptxChannel->qmf34idx + 0] = float(pcm4[1]);
  aptxChannel->qmf34A[aptxChannel->qmf34idx + 1] = float(pcm4[2]);
  aptxChannel->qmf34B[aptxChannel->qmf34idx + 1] = float(pcm4[3]);
  aptxChannel->qmf34idx += 2;
  if (aptxChannel->qmf34idx >= 34) {
    aptxChannel->qmf34idx = 0;
  }
  aptxQMF34(qmf32B, &QMF34_FLT_0[34 - 2 - aptxChannel->qmf34idx], aptxChannel->qmf34A);
  aptxQMF34(qmf32A, &QMF34_FLT_1[34 - 2 - aptxChannel->qmf34idx], aptxChannel->qmf34B);
	aptxChannel->qmf32B[aptxChannel->qmf32idx + 0] = float((qmf32A[0] + qmf32B[0]) / 65536);
  aptxChannel->qmf32A[aptxChannel->qmf32idx + 0] = float((qmf32A[0] - qmf32B[0]) / 65536);
  aptxChannel->qmf32B[aptxChannel->qmf32idx + 1] = float((qmf32A[1] + qmf32B[1]) / 65536);
  aptxChannel->qmf32A[aptxChannel->qmf32idx + 1] = float((qmf32A[1] - qmf32B[1]) / 65536);
  aptxChannel->qmf32idx += 2;
  if (aptxChannel->qmf32idx >= 32) {
    aptxChannel->qmf32idx = 0;
  }
  qmf32B[0] = aptxQMF32(&QMF32_FLT[32 + 0 - aptxChannel->qmf32idx], &aptxChannel->qmf32B[0]);
  qmf32B[1] = aptxQMF32(&QMF32_FLT[32 + 1 - aptxChannel->qmf32idx], &aptxChannel->qmf32B[1]);
  qmf32A[0] = aptxQMF32(&QMF32_FLT[32 + 0 - aptxChannel->qmf32idx], &aptxChannel->qmf32A[0]);
  qmf32A[1] = aptxQMF32(&QMF32_FLT[32 + 1 - aptxChannel->qmf32idx], &aptxChannel->qmf32A[1]);
  pcm4[0] = real_t((qmf32B[1] + qmf32B[0]) / 65536);
  pcm4[1] = real_t((qmf32B[1] - qmf32B[0]) / 65536);
  pcm4[2] = real_t((qmf32A[1] + qmf32A[0]) / 65536);
  pcm4[3] = real_t((qmf32A[1] - qmf32A[0]) / 65536);
}

template class aptx100fp_t<float>;
template class aptx100fp_t<double>;
