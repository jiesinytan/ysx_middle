#ifndef __RTS_MIDDLE_MEDIA_H
#define __RTS_MIDDLE_MEDIA_H

#include <rtsavapi.h>
#include <qcam_video_input.h>
#include <qcam_audio_input.h>

#define AV_INIT_NO 115 /* attribute constructor must bigger than 112*/
#define AUDIO_INIT_NO 116 /* attribute constructor must bigger than 112*/

#define CHN_OK(x) ((&(x))->stat == RTS_CHN_STAT_OK)
#define CHN_RUN(x) ((&(x))->stat == RTS_CHN_STAT_RUN)
#define CHN_UNINIT(x) ((&(x))->stat == RTS_CHN_STAT_UNINIT)
#define CHN_ERR(x) ((&(x))->err)

/*********** video ***********/
#define MAIN_STREAM 0
#define STREAM_COUNT 4 /* stream 0,1,2 h264 stream; stream 3 isp stream*/
#define YUV_CAPTURE_CHN 3
#define YUV_CAPTURE_BUFS 2
#define YUV_CAPTURE_FPS 10
#define MAX_FPS 30
#define MAX_BITRATE 5000
#define QP_CBR_DEFAULT 45
#define QP_VBR_DEFAULT 42
#define QP_HIGH 42
#define QP_LOW 45
#define RTS_SEND 0
#define RTS_RECV 1
#define MJPEG_COMPRESS_RATE 15


#define GPIO_IR_CUT 10
#define GPIO_IR_LED 16
#define GPIO_IR_CUT_DAY 0
#define GPIO_IR_CUT_NIGHT 1
#define GPIO_IR_LED_DAY 0
#define GPIO_IR_LED_NIGHT 1
#define ADC_CHN 0 /*TODO*/
#define ADC_FACTOR 128
#define ADC_DAY_THR 600 /*TODO adjust this value*/
#define ADC_NIGHT_THR 400 /*TODO adjust this value*/

#define OSD_TM_CHAR_LIB_FILE_PREFIX "/usr/osd_char_lib" /* TODO */
#define TM_PICT_HEIGHT 16
#define TM_PICT_WIDTH (TM_PICT_HEIGHT / 2)
#define TM_PICT_SIZE (TM_PICT_WIDTH * TM_PICT_HEIGHT)
#define TM_ELEM_NUM 20
#define TM_PICT_FORMAT RTS_OSD2_BLK_FMT_RGBA2222
#define OSD_TM_TIME_BLKIDX 0
#define OSD_TM_DATE_BLKIDX 1

/*TODO */
#define OSD_PICT_FORMAT RTS_OSD2_BLK_FMT_RGBA2222
#define OSD_PICT_WIDTH 640
#define OSD_PICT_HEIGHT 480
#define OSD_PICT_BLKIDX 2

enum ysx_bitrate_mode {
	YSX_BITRATE_MODE_CBR = 0,
	YSX_BITRATE_MODE_VBR,
};

enum stream_stat {
	RTS_STM_STAT_UNINIT = -1,
	RTS_STM_STAT_OK = 0,
	RTS_STM_STAT_ISP_FAIL,
	RTS_STM_STAT_OSD_FAIL,
	RTS_STM_STAT_H264_FAIL,
	RTS_STM_STAT_JPEG_FAIL,
	RTS_STM_STAT_FAIL,
};

enum channel_stat {
	RTS_CHN_STAT_UNINIT = -1,
	RTS_CHN_STAT_OK = 0,
	RTS_CHN_STAT_RUN,
	RTS_CHN_STAT_FAIL,
};

enum channel_err {
	RTS_CHN_E_UNINIT = -1,
	RTS_CHN_E_OK = 0,
	RTS_CHN_E_RUN,
	RTS_CHN_E_CREATE_FAIL,
	RTS_CHN_E_DESTROY_FAIL,
	RTS_CHN_E_ENABLE_FAIL,
	RTS_CHN_E_DISABLE_FAIL,
	RTS_CHN_E_START_SEND_FAIL,
	RTS_CHN_E_START_RECV_FAIL,
	RTS_CHN_E_STOP_SEND_FAIL,
	RTS_CHN_E_STOP_RECV_FAIL,
	RTS_CHN_E_BIND_FAIL,
	RTS_CHN_E_UNBIND_FAIL,
	RTS_CHN_E_GET_PROFILE_FAIL,
	RTS_CHN_E_SET_PROFILE_FAIL,
	RTS_CHN_E_SET_H264_FAIL,
	RTS_CHN_E_SET_JPEG_FAIL,
	RTS_CHN_E_QUERY_FAIL,
	RTS_CHN_E_ADD_THREAD_FAIL,
	RTS_CHN_E_SET_CB_FAIL,
	RTS_CHN_E_INIT_FIFO_FAIL,
	RTS_CHN_E_EXIT,
};

enum channel_type {
	RTS_MIDDLE_AUDIO_CHN_CAPTURE = 0,
	RTS_MIDDLE_AUDIO_CHN_ENCODE_ALAW,
	RTS_MIDDLE_AUDIO_CHN_ENCODE_AAC,
	RTS_MIDDLE_AUDIO_CHN_PLAYBACK,
	RTS_MIDDLE_AUDIO_CHN_AEC,
	RTS_MIDDLE_AUDIO_CHN_RESAMPLE,
	RTS_MIDDLE_AUDIO_CHN_DECODE,
	RTS_MIDDLE_AUDIO_CHN_MIXER,

	RTS_MIDDLE_VIDEO_CHN,
};

/* OSD */
struct text_info {
	char *text;
	int cnt;
	uint32_t x;
	uint32_t y;
	uint8_t *pdata;
	uint32_t len;
};

struct rts_m_osd2_time {
	int enable;
	int x;
	int y;
	pthread_mutex_t mutex;
};

struct rts_m_osd2_pict {
	int enable;
	/*TODO not used currently*/
	uint8_t *buf;
	int x;
	int y;
	int format;
	int width;
	int height;
};

struct rts_m_osd2 {
	struct rts_m_osd2_time tm;
	struct rts_m_osd2_pict pict;;
};

struct rts_m_osd2_common {
	/* each osd channel share same time template */
	uint8_t *tm_img_patt;
	uint8_t *tm_img_2222;
	/* flag: osd flush time run/stop */
	int run; /* 0:initial stat stop*/
	pthread_t tid;
};

typedef struct Mchannel {
	/* common */
	int id; /* rts channelid */
	int type;
	int stat;
	int err;
	int buf_num;
	/* video */
	int fps;
	int width;
	int height;
	struct rts_m_osd2 osd_info;
	/* audio */
	int sample_rate;
	int bitfmt;
	int channels;
}Mchannel;

struct rts_middle_stream {
	int id; /* ysx channelid(streamid) */
	Mchannel isp, osd, h264, jpg;
	int stat;
	int exit;
	pthread_mutex_t mutex;
	struct rts_video_h264_ctrl *h264_ctrl;
	struct rts_video_mjpeg_ctrl *mjpeg_ctrl;
	struct rts_video_osd2_attr *osd_attr;
	QCam_Video_Input_cb cb;
};

struct rts_m_isp_params {
	/* rts isp attr */
	struct rts_isp_attr attr;
	/* rts profile */
	struct rts_av_profile profile;
};

struct rts_m_h264_params {
	/* rts h264 attr */
	struct rts_h264_attr attr;
	int bitrate_mode;
};

struct mjpeg_cb_data {
	char *pbuf;
	int *plen;
	int cb_ret;
	int cb_done;
};

enum rts_m_ir_stat {
	UNKNOWN = -1,
	NIGHT = 0,
	DAY,
};

enum rts_m_auto_ir_stat {
	AUTO_IR_STOP = 0,
	AUTO_IR_RUN,
};

struct rts_m_ir {
	enum rts_m_ir_stat ir_stat;
	QCAM_IR_MODE ir_mode;
	struct rts_gpio *gpio_ir_cut;
	struct rts_gpio *gpio_ir_led;
	pthread_t tid;
	int auto_ir_exit;
	int auto_ir_stat;
	pthread_mutex_t mutex;
};
/*********** video ***********/


/*********** audio ***********/
#define DFT_AD_IN_CHANNELS 1
#define DFT_AD_IN_BITFMT 16
#define DFT_AD_IN_RATE_8K 8000
#define DFT_AD_IN_PERIOD_TM 20 /* 20 ms */
#define DFT_AD_OUT_CHANNELS 1
#define DFT_AD_OUT_BITFMT 16
#define DFT_AD_OUT_RATE_8K 8000
#define AEC_RESAMPLE_RATE_16K 16000

#define AD_PLY_CACHE_BUFS 10 /* 200 ms if AD_PLY_BUF_LEN == 320*/
#define AD_PLY_BUF_LEN 320

#define DFT_AD_PLY_DGAIN 85	/* actual = 85 * 1.27, max = 127 */
#define DFT_AD_CAP_DGAIN 90	/* actual = 90 * 1.27, max = 127 */

#define AUDIO_DEVICE0 "hw:0,0"
#define AUDIO_DEVICE1 "hw:0,1"
struct audio_server {
	Mchannel ai, aec, ai_resample, aec_resample;
	Mchannel ao, enc_alaw, enc_aac, mixer, ao_resample, ptoa_resample;
	struct rts_audio_attr ai_attr, ao_attr;
	struct rts_av_profile ao_profile;
	int softaec;
	int capture_exit;
	int capture_start;
	int capture_run;
	Mchannel *pai, *pao;
	pthread_mutex_t m_capture;
	pthread_t t_capture;
	QCamAudioInputCallback_aec aec_cb;
	int svr_init;
};
/*********** audio ***********/

/*********** sys *************/
/* assign key gpio number at kernel config */
#define KEY_NUM 3
#define LED_NUM 2

#define BLINK_DUTY 200000 /* time: xx us */
enum led_color {
	LED_YELLOW = 0,
	LED_BLUE,
};

/* TODO system gpio? io number? */
enum led_gpio {
	GPIO_LED_YELLOW = 9,
	GPIO_LED_BLUE = 11,
};

struct led {
	struct rts_gpio *gpio;
	int blink;
};
/*********** sys *************/


void __init_chn_info(Mchannel *pchn, int type);
int __bind_chn(Mchannel *pchn1, Mchannel *pchn2);
int __unbind_chn(Mchannel *pchn1, Mchannel *pchn2);
int __destroy_chn(Mchannel *pchn);
int __enable_chn(Mchannel *pchn);
int __disable_chn(Mchannel *pchn);
int __start_chn(Mchannel *pchn, int dir);
int __stop_chn(Mchannel *pchn, int dir);

void __init_audio_server(void);
void __release_audio_server(void);

void __init_sys_daemon(void);
void __release_sys_daemon(void);

void __init_middleware_context(void);
void __release_middleware_context(void);
#endif
