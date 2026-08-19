#include "aptx100_engine.h"
#include <algorithm>
#include <execution>
#include <math.h>
#include <stdio.h>

template<typename real_t> aptx100_engine_t<real_t>::aptx100_engine_t() {
	mode = 0;
	channels = 0;
	run_threads = false;
}

template<typename real_t> aptx100_engine_t<real_t>::~aptx100_engine_t() {
	free();
}

template<typename real_t> int aptx100_engine_t<real_t>::init(unsigned int p_mode, size_t p_channels, bool p_use_int16) {
	mode = p_mode | APTX_MODE::USE_APTX_MSB;
	channels = p_channels;
	use_int16 = p_use_int16;
	reinit();
	return 0;
}

template<typename real_t> void aptx100_engine_t<real_t>::reinit() {
	free_slots();
	init_slots();
}

template<typename real_t> void aptx100_engine_t<real_t>::free() {
	free_slots();
}

template<typename real_t> size_t aptx100_engine_t<real_t>::run(size_t samples, std::vector<std::vector<real_t>>& pcm_data, std::vector<std::vector<unsigned short>>& aptx_data) {
	size_t ch;

	ch = 0;
	for (auto&& slot : slots) {
		slot.samples = samples;
		slot.pcm_data = pcm_data[ch];
		slot.aptx_data = aptx_data[ch];
		slot.inp_semaphore.release(); // Release worker thread on the loaded slot
#ifndef _USE_ST
		ch++;
	}
	ch = 0;
	for (auto&& slot : slots) {
#endif
		slot.out_semaphore.acquire();	// Wait until worker thread is complete
		pcm_data[ch] = slot.pcm_data;
		aptx_data[ch] = slot.aptx_data;
		ch++;
	}

	return samples;
}

template<typename real_t> bool aptx100_engine_t<real_t>::init_slots() {
	slots.resize(channels);
	for (auto&& slot : slots) {
		if (!slot.codec.init(mode, use_int16)) {
			return false;
		}
		run_threads = true;
		std::thread t([this, &slot]() { slot.run(run_threads); });
		if (!t.joinable()) {
			return false;
		}
		slot.thread = std::move(t);
	}
	return true;
}

template<typename real_t> void aptx100_engine_t<real_t>::free_slots() {
	run_threads = false;
	for (auto&& slot : slots) {
		slot.inp_semaphore.release(); // Release worker thread for exit
		slot.thread.join(); // Wait until worker thread exit
		slot.codec.free();
		slot.samples = 0;
		slot.pcm_data.clear();
		slot.aptx_data.clear();
	}
	slots.clear();
}

template class aptx100_engine_t<float>;
template class aptx100_engine_t<double>;
