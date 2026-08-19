#pragma once

#include "aptx100.h"
#include <vector>

template<typename real_t> class aptx100fp_t {
  aptxCtx_t* aptxCtx;
	bool aptxInt;
	std::vector<short> pcmInt16;
public:
  aptx100fp_t();
  ~aptx100fp_t();
  bool init(unsigned int mode, bool use_int16);
  void free();
  int run(int samples, real_t* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status);
private:
  int decode(int samples, real_t* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status);
  void channelDecode(aptxChannel_t* aptxChannel, real_t pcm4[4], unsigned short aptxVal, int bitcorr_ch1, int bitcorr_ch3);
  void channelDecodeQMF(aptxChannel_t* aptxChannel, real_t pcm4[4]);
  int encode(int samples, real_t* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status);
  unsigned short channelEncode(aptxChannel_t* aptxChannel, real_t pcm4[4], int bitcorr_ch1, int bitcorr_ch3);
  void channelEncodeQMF(aptxChannel_t* aptxChannel, real_t pcm4[4]);
};
