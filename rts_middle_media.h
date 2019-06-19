#ifndef __RTS_MIDDLE_MEDIA_H
#define __RTS_MIDDLE_MEDIA_H

#include <linux/types.h>
#include <stdbool.h>
#include <rtsavapi.h>
#include <qcam_video_input.h>
#include <qcam_audio_input.h>
#ifndef YSX_MID_DBG_MSG
#define YSX_LOG(level, fmt...)
#else
#include "ysx_debug.h"
#endif

#define AV_INIT_NO 115 /* attribute constructor must bigger than 112*/
#define AUDIO_INIT_NO 116 /* attribute constructor must bigger than 112*/

#define CHN_OK(x) ((&(x))->stat == RTS_CHN_STAT_OK)
#define CHN_RUN(x) ((&(x))->stat == RTS_CHN_STAT_RUN)
#define CHN_UNINIT(x) ((&(x))->stat == RTS_CHN_STAT_UNINIT)
#define CHN_ERR(x) ((&(x))->err)

/*********** video ***********/
#define STREAM_COUNT 4 /* stream 0,1,2 h264 stream; stream 3 isp stream */
#define MULTIPLEX_STM_ID 2
#define MULTIPLEX_STM_FPS 5
#define MAX_FPS 30
#define MAX_BITRATE 5000
#define QP_CBR_DEFAULT 45
#define QP_VBR_DEFAULT 42
#define QP_HIGH 42
#define QP_LOW 45
#define RTS_SEND 0
#define RTS_RECV 1
#define MJPEG_COMPRESS_RATE 15


#define GPIO_IR_CUT_0 20
#define GPIO_IR_CUT_1 21
#define GPIO_IR_LED 17
#define GPIO_IR_CUT_LOW 0
#define GPIO_IR_CUT_HIGH 1
#define GPIO_IR_LED_DAY 0
#define GPIO_IR_LED_NIGHT 1
#define ADC_CHN 0 /*TODO*/
#define ADC_FACTOR 128
#define ADC_DAY_THR 600 /*TODO adjust this value*/
#define ADC_NIGHT_THR 300 /*TODO adjust this value*/

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

#define CELL_W 8
#define CELL_H 8
#define MD_4x3_WIDTH 640
#define MD_4x3_HEIGHT 480
#define MD_SENSITIVITY 50
#define MD_PERCENTAGE 90
#define MD_FRAME_INTERVAL 5
#define MD_START_X 0
#define MD_START_Y 0
#define MD_AREA_COL 80
#define MD_AREA_ROW 60

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

	RTS_MIDDLE_VIDEO_CHN_ISP,
	RTS_MIDDLE_VIDEO_CHN_OSD,
	RTS_MIDDLE_VIDEO_CHN_H264,
	RTS_MIDDLE_VIDEO_CHN_MJPEG,
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

struct osd_m_tid {
	pthread_t tid;
	bool stat;
};

struct rts_m_osd2_common {
	/* each osd channel share same time template */
	uint8_t *tm_img_patt;
	uint8_t *tm_img_2222;
	/* flag: osd flush time run/stop */
	int run; /* 0:initial stat stop*/
	struct osd_m_tid osd_tid;
};

struct rts_m_jpg {
	int stm_id;
	int stm_exist;
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

struct rts_m_pthreadpool {
	pthread_t *tid;
	int pool_size;
	int current_size;
};

struct rts_middle_stream {
	int id; /* ysx channelid(streamid) */
	Mchannel isp, osd, h264, jpg;
	int stat;
	int exit;
	pthread_mutex_t mutex;
	int has_h264;
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

struct rts_m_md {
	int exit;
	pthread_t tid;
	bool t_stat;
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

struct ir_m_tid {
	pthread_t tid;
	bool stat;
};

struct rts_m_ir {
	enum rts_m_ir_stat ir_stat;
	QCAM_IR_MODE ir_mode;
	struct rts_gpio *gpio_ir_led;
	struct ir_m_tid ir_tid;
	int auto_ir_exit;
	int auto_ir_stat;
	pthread_mutex_t mutex;
};
/*********** video ***********/


/*********** audio ***********/
#define DFT_AD_IN_CHANNELS 1
#define DFT_AD_IN_BITFMT 16
#define DFT_AD_IN_RATE_8K 8000
#define DFT_AD_IN_PERIOD_TM 64 /* 64 ms */
#define DFT_AD_OUT_CHANNELS 1
#define DFT_AD_OUT_BITFMT 16
#define DFT_AD_OUT_RATE_8K 8000
#define AEC_RESAMPLE_RATE_16K 16000

#define AD_PLY_CACHE_BUFS 10 /* 200 ms if AD_PLY_BUF_LEN == 320*/
#define AD_PLY_BUF_LEN 320

#define DFT_AD_PLY_DGAIN 93	/* actual = 85 * 1.27, max = 127 */
#define DFT_AD_CAP_DGAIN 97	/* actual = 90 * 1.27, max = 127 */

#define GPIO_SPK 18 /* pull gpio_18 high before audio playback */
#define GPIO_SPK_LOW 0
#define GPIO_SPK_HIGH 1

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
	int amp_exit;
	int ao_idle_cnt;
	bool amp_stat;
	Mchannel *pai, *pao;
	pthread_mutex_t m_capture;
	pthread_t t_capture;
	pthread_t t_amp;
	bool t_capture_stat;
	bool t_amp_stat;
	QCamAudioInputCallback_aec aec_cb;
	int svr_init;
};

struct rts_m_spk_gpio {
	struct rts_gpio *io;
};
/*********** audio ***********/

/*********** sys *************/
/* assign key gpio number at kernel config */
#define KEY_NUM 3
#define LED_NUM 2

#define PWM_SETTING                                 "/sys/devices/platform/pwm_platform/settings/pwm" /* TODO */
#define BLINK_PERIOD                                1000000000//100000000 /* 100ms max: 1s min 200us*/
enum led_color {
	LED_YELLOW = 0,
	LED_BLUE,
};

enum pwm_request {
	PWM_FREE = 0,
	PWM_REQUEST = 1,
};

enum pwm_enable {
	PWM_DISABLE = 0,
	PWM_ENABLE = 1,
};

/* TODO pwm channel */
enum led_pwm {
	PWM_LED_YELLOW = 3,
	PWM_LED_BLUE = 1,
};

struct led {
	int pwm;
	int blink;
};

struct button_tid {
	pthread_t tid;
	bool stat;
};

struct led_tid {
	pthread_t tid;
	bool stat;
};

/* hconf partition:
 * size: 256kBytes
 * ota_flag_offset: 255kBytes
 */
#define HCONF_PARTITION		"/dev/mtd7"
#define OTA_FLAG_OFFSET		0x3fc00
#define HCONF_MSG_MAX_LEN		1024

#define NEU_HEADER_LEN				256
#define NEU_CHECKSUM_ALIGN			0x100000000
#define MTD_MAGIC_FEOF				0x46454f46
#define MTD_DEV_OPS_UNIT			0x10000
#define READ_UNIT_SIZE				0x10000
#define FLASH_SIZE				(16 * 1024 * 1024) /*TODO*/

#define UPDATE_TIME 60

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

struct wdt_m_para {
	int fd;
	int livetime;
};

struct section_head {
	__u32 magic;
	__u32 reserved1;
	__u64 reserved2;
	__u64 burnaddr;
	__u64 burnlen;
} __attribute__((packed));

struct section_tail {
	__u32 checksum;
} __attribute__((packed));

enum neu_status {
	NEU_STAT_PREPARE,
	NEU_STAT_CHECKING,
	NEU_STAT_BURNING,
	NEU_STAT_SUCCESS,
	NEU_STAT_FAILED = -1,
};

struct neu_data;
struct neu_cache {
	unsigned char data[READ_UNIT_SIZE];
	struct section_head head;
	char __head[sizeof(struct section_head)];
	struct section_tail tail;
	char __tail[sizeof(struct section_tail)];
	int (*flush)(struct neu_data *data, int fd);
};

struct neu_data {
	const char *filename;
	int burn_size;
	int burn_status;
	struct neu_cache *cache;
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
#endif
