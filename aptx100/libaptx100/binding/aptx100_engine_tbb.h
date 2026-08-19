#pragma once

#include "aptx100/aptx100.h"
#include <vector>

typedef int aptxEncDec_t(aptxCtx_t* aptxCtx, int samples, short* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status);

class aptx100_slot_t {
public:
	aptxCtx_t*                  codec;
	aptxEncDec_t*               encdec;
	std::vector<short>          pcm_data;
	std::vector<unsigned short> aptx_data;

 	aptx100_slot_t() : codec(nullptr), encdec(nullptr) {
	}
	aptx100_slot_t(const aptx100_slot_t& slot) {
		codec     = slot.codec;
		encdec    = slot.encdec;
		pcm_data  = slot.pcm_data;
		aptx_data = slot.aptx_data;
	}
	aptx100_slot_t(aptx100_slot_t&& slot) {
		codec     = std::move(slot.codec);
		encdec    = std::move(slot.encdec);
		pcm_data  = std::move(slot.pcm_data);
		aptx_data = std::move(slot.aptx_data);
	}
	aptx100_slot_t& operator=(aptx100_slot_t&& slot) = delete;
};

class aptx100_engine_t {
	int mode;
	size_t channels;
	std::vector<aptx100_slot_t> slots;

public:
	aptx100_engine_t();
	~aptx100_engine_t();
	int init(int p_mode, size_t p_channels);
	void free();
	size_t run(size_t samples, std::vector<std::vector<short>>& pcm_data, std::vector<std::vector<unsigned short>>& aptx_data);
private:
	void reinit();
	bool init_slots();
	void free_slots();
};
