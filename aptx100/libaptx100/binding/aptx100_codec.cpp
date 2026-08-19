#include <aptx100_codec.h>
#include <aptx100_engine.h>

template<typename real_t> class aptx100_codec_t<real_t>::ctx_t : public aptx100_engine_t<real_t> {
};

template<typename real_t>aptx100_codec_t<real_t>::aptx100_codec_t() : ctx(nullptr) {
}

template<typename real_t> aptx100_codec_t<real_t>::~aptx100_codec_t() {
	delete ctx;
}

template<typename real_t> int aptx100_codec_t<real_t>::get_delay() {
	if (!ctx) {
		return 0;
	}
	return APTX_DELAY_IN_SAMPLES;
}

template<typename real_t> int aptx100_codec_t<real_t>::init(aptx100_mode_e mode, size_t channels, bool use_int16) {
	if (!ctx) {
		ctx = new ctx_t();
	}
	if (!ctx) {
		return -1;
	}
	auto use_mmx{ false };
	return ctx->init((mode == aptx100_mode_e::ENCODE ? APTX_MODE::ENCODE : APTX_MODE::DECODE) | (use_mmx ? APTX_MODE::USE_MMX : 0), channels, use_int16);
}

template<typename real_t> void aptx100_codec_t<real_t>::free() {
	if (!ctx) {
		return;
	}
	return ctx->free();
}

template<typename real_t> size_t aptx100_codec_t<real_t>::run(size_t samples, std::vector<std::vector<real_t>>& pcm_data, std::vector<std::vector<unsigned short>>& aptx_data) {
	if (!ctx) {
		return 0;
	}
	return ctx->run(samples, pcm_data, aptx_data);
}

template class aptx100_codec_t<float>;
template class aptx100_codec_t<double>;
