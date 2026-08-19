#pragma once

#include "aptx100fp.h"
#include <thread>
#include <vector>
#include <std_semaphore.h>

template<typename real_t> class aptx100_slot_t {
public:
	std::thread                 thread;
	semaphore_t                 inp_semaphore;
	semaphore_t                 out_semaphore;

	aptx100fp_t<real_t>         codec;
	size_t                      samples;
	std::vector<real_t>         pcm_data;
	std::vector<unsigned short> aptx_data;

 	aptx100_slot_t() : inp_semaphore(0), out_semaphore(0), samples(0) {
	}
	aptx100_slot_t(const aptx100_slot_t& slot) : inp_semaphore(0), out_semaphore(0) {
		codec     = slot.codec;
		samples   = slot.samples;
		pcm_data  = slot.pcm_data;
		aptx_data = slot.aptx_data;
	}
	aptx100_slot_t(aptx100_slot_t&& slot) : inp_semaphore(0), out_semaphore(0) {
		codec     = std::move(slot.codec);
		samples   = std::move(slot.samples);
		pcm_data  = std::move(slot.pcm_data);
		aptx_data = std::move(slot.aptx_data);
	}
	aptx100_slot_t& operator=(aptx100_slot_t&& slot) = delete;
	void run(bool& running) {
		while (running) {
			inp_semaphore.acquire();
			if (running) {
				codec.run(static_cast<int>(samples), pcm_data.data(), aptx_data.data(), nullptr);
			}
			out_semaphore.release();
		}
	}
};

template<typename real_t> class aptx100_engine_t {
	unsigned int mode;
	size_t channels;
	bool use_int16;
	std::vector<aptx100_slot_t<real_t>> slots;
 	bool run_threads;

public:
	aptx100_engine_t();
	~aptx100_engine_t();
	int init(unsigned int p_mode, size_t p_channels, bool p_use_int16);
	void free();
	size_t run(size_t samples, std::vector<std::vector<real_t>>& pcm_data, std::vector<std::vector<unsigned short>>& aptx_data);
private:
	void reinit();
	bool init_slots();
	void free_slots();
};
