#include "aptx100_engine_tbb.h"
#include <algorithm>
#include <execution>
#include <math.h>
#include <stdio.h>

aptx100_engine_t::aptx100_engine_t() {
	mode = 0;
	channels = 0;
}

aptx100_engine_t::~aptx100_engine_t() {
	free();
}

int aptx100_engine_t::init(int p_mode, size_t p_channels) {
	mode = p_mode | APTX_MODE::USE_APTX_MSB;
	channels = p_channels;
	reinit();
	return 0;
}

void aptx100_engine_t::reinit() {
	free_slots();
	init_slots();
}

void aptx100_engine_t::free() {
	free_slots();
}

size_t aptx100_engine_t::run(size_t samples, std::vector<std::vector<short>>& pcm_data, std::vector<std::vector<unsigned short>>& aptx_data) {
	size_t ch;

	ch = 0;
	for (auto&& slot : slots) {
		slot.pcm_data = pcm_data[ch];
		slot.aptx_data = aptx_data[ch];
		ch++;
	}

	std::for_each(
		std::execution::par_unseq,
		std::begin(slots),
		std::end(slots),
		[&samples](aptx100_slot_t& slot) {
			slot.encdec(slot.codec, (int)samples, slot.pcm_data.data(), slot.aptx_data.data(), nullptr);
		}
	);

	ch = 0;
	for (auto&& slot : slots) {
		pcm_data[ch] = slot.pcm_data;
		aptx_data[ch] = slot.aptx_data;
		ch++;
	}

	return samples;
}

bool aptx100_engine_t::init_slots() {
	slots.resize(channels);
	for (auto&& slot : slots) {
		slot.codec = aptxCreate(mode, -1, 0, 1);
		if (!slot.codec) {
			return false;
		}
		switch (mode & 1) {
		case APTX_MODE::DECODE:
			slot.encdec = aptxDec;
			break;
		case APTX_MODE::ENCODE:
			slot.encdec = aptxEnc;
			break;
		}
	}
	return true;
}

void aptx100_engine_t::free_slots() {
	for (auto&& slot : slots) {
		if (slot.codec) {
			aptxDelete(slot.codec);
		}
		slot.codec = nullptr;
		slot.encdec = nullptr;
		slot.pcm_data.clear();
		slot.aptx_data.clear();
	}
	slots.clear();
}
