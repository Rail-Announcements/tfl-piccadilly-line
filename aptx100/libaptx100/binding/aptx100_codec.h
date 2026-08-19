#pragma once

#include <cstddef>
#include <vector>

enum class aptx100_mode_e {
	DECODE = 0,
	ENCODE = 1
};

template<typename real_t> class aptx100_codec_t final {
	class ctx_t;
	ctx_t* ctx;
public:
	aptx100_codec_t();
	~aptx100_codec_t();
	int get_delay();
	int init(aptx100_mode_e mode, size_t channels, bool use_int16 = false);
	void free();
	size_t run(size_t samples, std::vector<std::vector<real_t>>& pcm_data, std::vector<std::vector<unsigned short>>& aptx_data);
};
