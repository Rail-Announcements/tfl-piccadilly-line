#pragma once

enum APTX_MODE { DECODE = 0, ENCODE = 1, USE_PCM_MSB = 2, USE_APTX_MSB = 4, USE_AUTO_AUX = 8, USE_MMX = 0x80000000 };

constexpr int APTX_MAX_CHANNELS = 2;
constexpr int APTX_DELAY_IN_SAMPLES = 122;

typedef struct {
	short c1;
	short c2;
} aptxQuantizationPair_t;

// size = 0x0014 (20)
typedef struct {
	/* 0x00 */ int encStates; // must be power of 2
	/* 0x04 */ int maxState;
	/* 0x08 */ int index;
	/* 0x0c */ aptxQuantizationPair_t* table1;
	/* 0x10 */ short* table2;
} aptxQuantizationTable_t;

// size = 0x00e4 (228)
typedef struct {
	/* 0x0000 */ int m_00[2];
	/* 0x0008 */ short m_08[4];
	/* 0x0010 */ int m_10[0x1e];
	/* 0x0088 */ int m_88[0x13];
	/* 0x00d4 */ int scale2[2];
	/* 0x00dc */ int pcm2[2];
} aptxQuantizer_t;

// size = 0x114 (276)
typedef struct {
	/* 0x0000 */ int m_00;
	/* 0x0004 */ int m_04;
	/* 0x0008 */ int m_08;
	/* 0x000c */ unsigned char m_0c;
	/* 0x000d */ unsigned char m_0d;
	/* 0x000e */ struct { short m_00; unsigned char m_02; unsigned char m_03; } m_0e[32];
	/* 0x008e */ short m_8e;
	/* 0x0090 */ int m_90;
	/* 0x0094 */ int m_94[32];
} aptxData_t;

// size = 0x05d0 (1488)
typedef struct {
	/* 0x0000 */ int qmf34idx;
	/* 0x0004 */ int qmf32idx;
	/* 0x0008 */ float qmf34A[34];
	/* 0x0090 */ float qmf34B[34];
	/* 0x0118 */ float qmf32A[32];
	/* 0x0198 */ float qmf32B[32];
	/* 0x0218 */ aptxQuantizer_t quantizer[4];
	/* 0x05a8 */ int mode; // can be 1 - don't process or 2
	/* 0x05ac */ int status_index;
	/* 0x05b0 */ unsigned char status_data[32];
} aptxChannel_t;

// size = 0x0bb8 (3000)
typedef struct _aptxCtx_t {
	/* 0x0000 */ int mode; // bit 31 - is MMX supported, bit 2 - is MSB
	/* 0x0004 */ int buffers;
	/* 0x0008 */ int m_08; // -3
	/* 0x000c */ int m_0c;
	/* 0x0010 */ int channels;
	/* 0x0014 */ aptxData_t* aptxData; // size = 0x114 (276)
	/* 0x0018 */ aptxChannel_t aptxChannel[APTX_MAX_CHANNELS]; // size = 1488 for channels
} aptxCtx_t;

bool use_mmx();

bool aptxAutoAux(aptxCtx_t* aptxCtx, bool mode_bit_3);

void aptxInitialize(aptxCtx_t* aptxCtx, unsigned int mode, int buffers, int* channel_mode, int channels);
aptxCtx_t* aptxCreate(unsigned int mode, int buffers, int* channel_mode, int channels);
void aptxDelete(aptxCtx_t* aptxCtx);

void aptxDecInit(aptxCtx_t* aptxCtx, int channels);
int aptxDecode(aptxCtx_t* aptxCtx, int unused, int samples, bool mode_pcm_msb, short* pcmBuf, bool mode_aptx_msb, unsigned short* aptxBuf);
int aptxDec(aptxCtx_t* aptxCtx, int samples, short* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status); 

void aptxEncInit(aptxCtx_t* aptxCtx, int channels);
int aptxEncode(aptxCtx_t* aptxCtx, int unused, int samples, bool mode_pcm_msb, short* pcmBuf, bool mode_aptx_msb, unsigned short* aptxBuf); 
int aptxEnc(aptxCtx_t* aptxCtx, int samples, short* pcmBuf, unsigned short* aptxBuf, unsigned char** channel_status);

unsigned short mmx_aptxChannelEncode(aptxChannel_t* aptxChannel, int pcm4[4], int bitcorr_ch1, int bitcorr_ch3);
unsigned short std_aptxChannelEncode(aptxChannel_t* aptxChannel, int pcm4[4], int bitcorr_ch1, int bitcorr_ch3);
void std_enc_aptxQMF(aptxChannel_t* aptxChannel, int pcm4[4]);
void aptxQMF34(double dst[2], float flt[34], float src[34]);
double aptxQMF32(float flt[32], float src[32]);
int aptxDoubleToIntStd(double value);
int aptxDoubleToIntSym(double value);
int std_enc_aptxQuantizeBank(aptxQuantizer_t* aptxQuantizer, int pcmVal, int allocBits, int maxScale, int outShift, int windowLength);
int std_encdec_100028F1(int scale2[2], aptxQuantizationTable_t* qtz_entry, int pcmVal, int maxScale, int outShift);
void std_encdec_10002A1F(int pcm2[2], aptxQuantizer_t* aptxQuantizer, int windowLength);
void std_encdec_10002C26(int a1, int pcm2[2], aptxQuantizer_t* aptxQuantizer, int windowLength);
int std_enc_10003054(short* qtz_data, int bits, int absVal, int scale);
void enc_10003101(aptxCtx_t* aptxCtx, unsigned short* aptxBuf);
void enc_100031AF(aptxCtx_t* aptxCtx, unsigned short* aptxBuf, unsigned char* channel_status);
void mmx_aptxChannelDecode(aptxChannel_t* aptxChannel, int pcm4[4], unsigned short aptxVal, int bitcorr_ch1, int bitcorr_ch3);
void std_aptxChannelDecode(aptxChannel_t* aptxChannel, int pcm4[4], unsigned short aptxVal, int bitcorr_ch1, int bitcorr_ch3);
void std_dec_aptxQMF(aptxChannel_t* aptxChannel, int pcm4[4]);
int std_dec_aptxQuantizeBank(aptxQuantizer_t* aptxQuantizer, int aptxVal, int allocBits, int maxScale, int outShift, int windowLength);
unsigned char* dec_get_channel_status(aptxCtx_t* aptxCtx, int channel, unsigned short* aptxBuf);
void dec_aptx_data0_lt_0(aptxCtx_t* aptxCtx, unsigned short* aptxBuf);
void dec_aptx_data0_ge_0(aptxCtx_t* aptxCtx, unsigned short* aptxBuf);

extern float QMF34_FLT_0[34 + 34];
extern float QMF34_FLT_1[34 + 34];
extern float QMF32_FLT[32 + 32];

extern int ENC_IDX_1000F4EC[32];
extern int ENC_IDX_1000F56C[9];
extern aptxQuantizationTable_t QTZ_TABLE[8];
